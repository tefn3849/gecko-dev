/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxTrackEncoder.h"
#include "OMXCodecWrapper.h"
#include "VideoUtils.h"
#include "ISOTrackMetadata.h"

#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define OMX_LOG(args...) __android_log_print(ANDROID_LOG_INFO, "MediaEncoder", ## args);
#else
#define OMX_LOG(args, ...)
#endif

#define DEBUG
#ifdef DEBUG
#include "stagefright/MediaDefs.h"
#include "stagefright/MediaErrors.h"
#include "stagefright/MediaMuxer.h"
static android::sp<android::MediaMuxer> sMuxer;
static ssize_t sTrackIndex = -1;
#endif

using namespace android;

namespace mozilla {

#define ENCODER_CONFIG_FRAME_RATE 30 // fps
#define GET_ENCODED_VIDEO_FRAME_TIMEOUT 100000 // microseconds

nsresult
OmxVideoTrackEncoder::Init(int aWidth, int aHeight, TrackRate aTrackRate)
{
  mFrameWidth = aWidth;
  mFrameHeight = aHeight;
  mTrackRate = aTrackRate;

  mEncoder = OMXCodecWrapper::CreateAVCEncoder();
  NS_ENSURE_TRUE(mEncoder, NS_ERROR_FAILURE);

  nsresult rv = mEncoder->ConfigureVideo(mFrameWidth, mFrameHeight,
                                         ENCODER_CONFIG_FRAME_RATE);

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mInitialized = (rv == NS_OK);

  mReentrantMonitor.NotifyAll();

  return rv;
}

already_AddRefed<TrackMetadataBase>
OmxVideoTrackEncoder::GetMetadata()
{
  {
    // Wait if mEncoder is not initialized nor is being canceled.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    while (!mCanceled && !mInitialized) {
      mReentrantMonitor.Wait();
    }
  }

  if (mCanceled || mEncodingComplete) {
    return nullptr;
  }

  //TODO: Create a metadata of AVCTrackMetadata().
  nsRefPtr<AVCTrackMetadata> meta = new AVCTrackMetadata();
  meta->Width = mFrameWidth;
  meta->Height = mFrameHeight;
  meta->FrameRate = ENCODER_CONFIG_FRAME_RATE;
  meta->VideoFrequence = 90000; // Hz
  return meta.forget();
}

nsresult
OmxVideoTrackEncoder::GetEncodedTrack(EncodedFrameContainer& aData)
{
  VideoSegment segment;
  {
    // Move all the samples from mRawSegment to segment. We only hold the
    // monitor in this block.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // Wait if mEncoder is not initialized nor is being canceled.
    while (!mCanceled && (!mInitialized ||
          (mRawSegment.GetDuration() == 0 && !mEndOfStream))) {
      mReentrantMonitor.Wait();
    }

    if (mCanceled || mEncodingComplete) {
      return NS_ERROR_FAILURE;
    }

    segment.AppendFrom(&mRawSegment);
  }

  // Start queuing raw frames to the input buffers of OMXCodecWrapper.
  VideoSegment::ChunkIterator iter(segment);

  // Send the EOS signal and an empty frame to OMXCodecWrapper.
  if (mEndOfStream && iter.IsEnded() && !mEosSetInEncoder) {
    mEosSetInEncoder = true;
    nsTArray<uint8_t> imgBuf;
    CreateMutedFrame(&imgBuf);
    uint64_t totalDurationUs = mTotalFrameDuration * USECS_PER_S / mTrackRate;
    mEncoder->EncodeVideoFrame(imgBuf, totalDurationUs,
                               OMXCodecWrapper::BUFFER_EOS);
  }

  while (!iter.IsEnded()) {
    VideoChunk chunk = *iter;

    // Send only the unique video frames to OMXCodecWrapper.
    if (mLastFrame != chunk.mFrame) {
      uint64_t totalDurationUs = mTotalFrameDuration * USECS_PER_S / mTrackRate;
      if (chunk.IsNull() || chunk.mFrame.GetForceBlack()) {
        nsTArray<uint8_t> imgBuf;
        CreateMutedFrame(&imgBuf);
        mEncoder->EncodeVideoFrame(imgBuf, totalDurationUs);
      } else {
        mEncoder->EncodeVideoFrame(*chunk.mFrame.GetImage(), totalDurationUs);
      }
    }

    mLastFrame.TakeFrom(&chunk.mFrame);
    mLastFrame.SetForceBlack(chunk.mFrame.GetForceBlack());
    mTotalFrameDuration += chunk.GetDuration();

    iter.Next();
  }

  // Dequeue an encoded frame from the output buffers of OMXCodecWrapper.
  nsTArray<uint8_t> buffer;
  int outFlags = 0;
  int64_t outTimeStampUs = 0;
  mEncoder->GetNextEncodedFrame(&buffer, &outTimeStampUs, &outFlags,
                                GET_ENCODED_VIDEO_FRAME_TIMEOUT);
  if (!buffer.IsEmpty()) {
    nsRefPtr<EncodedFrame> videoData = new EncodedFrame();
    if (outFlags & OMXCodecWrapper::BUFFER_CODEC_CONFIG) {
      videoData->SetFrameType(EncodedFrame::AVC_CSD);
    } else {
      videoData->SetFrameType((outFlags & OMXCodecWrapper::BUFFER_SYNC_FRAME) ?
                              EncodedFrame::I_FRAME : EncodedFrame::P_FRAME);
    }
    videoData->SetFrameData(&buffer);
    videoData->SetTimeStamp(outTimeStampUs);
    aData.AppendEncodedFrame(videoData);
  }

  if (outFlags & OMXCodecWrapper::BUFFER_EOS) {
    mEncodingComplete = true;
    OMX_LOG("OmxVideoTrackEncoder::GetEncodedTrack, Done encoding.");
  }

  return NS_OK;
}

nsresult
OmxAudioTrackEncoder::Init(int aChannels, int aSamplingRate)
{
  MOZ_ASSERT(!mInitialized && !mEncoder);

  mChannels = aChannels;
  mSamplingRate = aSamplingRate;
  mSampleDurationNs = 1000000000 / aSamplingRate; // in nanoseconds

  OMX_LOG("OmxAudioTrackEncoder::Init() ch:%d, rate:%d", mChannels, mSamplingRate);

  mEncoder = OMXCodecWrapper::CreateAACEncoder();
  nsresult rv = mEncoder->ConfigureAudio(mChannels, mSamplingRate);

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  mInitialized = (rv == NS_OK);

  mReentrantMonitor.NotifyAll();

  return NS_OK;
}

already_AddRefed<TrackMetadataBase>
OmxAudioTrackEncoder::GetMetadata()
{
  {
    // Wait if mEncoder is not initialized nor is being canceled.
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    while (!mCanceled && !mInitialized) {
      mReentrantMonitor.Wait();
    }
  }

  if (mCanceled || mEncodingComplete) {
    return nullptr;
  }

  nsRefPtr<AACTrackMetadata> meta = new AACTrackMetadata();
  meta->AACProfile = k14496_3_AAC;
  meta->MaxBitrate = meta->AvgBitrate = OMXCodecWrapper::kAACBitrate;
  meta->SampleRate = mSamplingRate;
  meta->Channels = mChannels;
  meta->FrameSize = OMXCodecWrapper::kAACFrameSize;
  meta->FrameDuration = OMXCodecWrapper::kAACFrameDuration;

  OMX_LOG("OmxAudioTrackEncoder::GetMetadata() r:%u d:%u s:%u",
      meta->SampleRate, meta->FrameDuration, meta->FrameSize);

  return meta.forget();
}

size_t
OmxAudioTrackEncoder::fillPCMBuffer(AudioSegment& aSegment,
                                    nsTArray<AudioDataValue>& aBuffer)
{
  size_t frames = aSegment.GetDuration();
  OMX_LOG("fillPCMBuffer() frames:%u", frames);
  MOZ_ASSERT(frames > 0);

  size_t copied = 0;
  // Sent input PCM data to encoder until queue is empty
  if (frames > 0) {
    // Get raw data from source
    AudioSegment::ChunkIterator iter(aSegment);
    size_t toCopy = 0;
    while (!iter.IsEnded()) {
      AudioChunk chunk = *iter;
      toCopy = chunk.GetDuration();
      aBuffer.SetLength((copied + toCopy) * mChannels);

      if (!chunk.IsNull()) {
        // Append the interleaved data to the end of pcm buffer.
        InterleaveTrackData(chunk, toCopy, mChannels,
                            aBuffer.Elements() + copied * mChannels);
      } else {
        memset(aBuffer.Elements() + copied * mChannels, 0,
               toCopy * mChannels * sizeof(AudioDataValue));
      }

      copied += toCopy;
      iter.Next();
      OMX_LOG("fillPCMBuffer() copied:%u", copied);
    }
    MOZ_ASSERT(copied == frames);
  }
  return copied;
}

nsresult
OmxAudioTrackEncoder::GetEncodedTrack(EncodedFrameContainer& aData)
{
  OMX_LOG("GetEncodedTrack() done:%d eos:%d init:%d cancel:%d",
    mEncodingComplete, mEndOfStream, mInitialized, mCanceled);
  MOZ_ASSERT(!mEncodingComplete);

  AudioSegment segment;
  // Move all the samples from mRawSegment to segment. We only hold
  // the monitor in this block.
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // Wait if mEncoder is not initialized nor canceled.
    while (!mInitialized && !mCanceled) {
      OMX_LOG("GetEncodedTrack() waiting...");
      mReentrantMonitor.Wait();
      OMX_LOG("GetEncodedTrack() done waiting");
    }

    if (mCanceled || mEncodingComplete) {
      OMX_LOG("GetEncodedTrack() fail");
      return NS_ERROR_FAILURE;
    }

    segment.AppendFrom(&mRawSegment);
  }

  size_t copied = 0;
  int64_t duration = 0;
  if (segment.GetDuration() > 0) {
    nsAutoTArray<AudioDataValue, 9600> pcm;
    copied = fillPCMBuffer(segment, pcm);
    duration = copied * mSampleDurationNs / 1000; // ns -> us
    MOZ_ASSERT(copied == segment.GetDuration());
    segment.RemoveLeading(copied);

    OMX_LOG("GetEncodedTrack() encoding...");
    // feed PCM to encoder
    mEncoder->EncodeAudioSamples(pcm, mTimestamp,
                                 duration,
                                 mEndOfStream? OMXCodecWrapper::BUFFER_EOS:0);
    OMX_LOG("GetEncodedTrack() done encoding");
  } else if (mEndOfStream) {
    // No audio data left in segment but we still have to feed something to
    // MediaCodec in order to notify EOS.
    nsAutoTArray<AudioDataValue, 1> dummy;
    dummy.SetLength(mChannels);
    memset(dummy.Elements(), 0, sizeof(AudioDataValue) * mChannels);
    duration = mSampleDurationNs / 1000;
    mEncoder->EncodeAudioSamples(dummy, mTimestamp, duration,
                                 OMXCodecWrapper::BUFFER_EOS);
  }
  mTimestamp += duration; // ns -> us

  nsTArray<uint8_t> frameData;
  int outFlags = 0;
  int64_t outTime = -1;
  OMX_LOG("GetEncodedTrack() getting next...");
  mEncoder->GetNextEncodedFrame(&frameData, &outTime, &outFlags,
                                3 * 1000); // wait up to 3ms
  OMX_LOG("GetEncodedTrack() done next %s", frameData.IsEmpty()? "not ok":"ok");
  if (!frameData.IsEmpty()) {
    bool isCSD = false;
    OMX_LOG("GetEncodedTrack() flags:0x%x, time:%lld len:%u", outFlags, outTime, frameData.Length());
    if (outFlags & OMXCodecWrapper::BUFFER_CODEC_CONFIG) { // codec specific data
      isCSD = true;
    } else if (outFlags & OMXCodecWrapper::BUFFER_EOS) { // last frame
      mEncodingComplete = true;
    } else {
      MOZ_ASSERT(frameData.Length() == OMXCodecWrapper::kAACFrameSize);
    }

#ifdef DEBUG
    if (!sMuxer.get()) {
      sMuxer = new android::MediaMuxer("/data/local/tmp/muxer.mp4", android::MediaMuxer::OUTPUT_FORMAT_MPEG_4);
      if (sMuxer.get()) {
        sp<AMessage> format = new AMessage();
        format->setString("mime", MEDIA_MIMETYPE_AUDIO_AAC);
        format->setInt32("channel-count", mChannels);
        format->setInt32("sample-rate", mSamplingRate);
        MOZ_ASSERT(isCSD);
        sp<ABuffer> csd = new ABuffer(frameData.Elements() + 2, frameData.Length() - 2);
        format->setBuffer("csd-0", csd);
        sTrackIndex = sMuxer->addTrack(format);
        sMuxer->start();
      }
    }
    if (sMuxer.get() && !isCSD) {
      sp<ABuffer> buffer = new ABuffer(frameData.Elements(), frameData.Length());
      sMuxer->writeSampleData(buffer, sTrackIndex, outTime, outFlags);
      if (mEncodingComplete) {
        sMuxer->stop();
        sMuxer.clear();
      }
    }
#endif

    EncodedFrame* audiodata = new EncodedFrame();
    audiodata->SetFrameType(isCSD?
      EncodedFrame::AAC_CSD : EncodedFrame::AUDIO_FRAME);
    audiodata->SetDuration(OMXCodecWrapper::kAACFrameDuration);
    audiodata->SetFrameData(&frameData);
    aData.AppendEncodedFrame(audiodata);
  }

  return NS_OK;
}

}
