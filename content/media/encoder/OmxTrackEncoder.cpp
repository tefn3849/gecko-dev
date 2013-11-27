/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxTrackEncoder.h"
#include "OMXCodecWrapper.h"
#include "VideoUtils.h"

#undef LOG
#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "MediaEncoder", ## args);
#else
#define LOG(args, ...)
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

  mEncoder = OMXCodecWrapper::CreateEncoder(MediaSegment::Type::VIDEO);
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

  return nullptr;
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
      videoData->SetFrameType(EncodedFrame::UNKNOW);
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
    LOG("OmxVideoTrackEncoder::GetEncodedTrack, Done encoding.");
  }

  return NS_OK;
}

}
