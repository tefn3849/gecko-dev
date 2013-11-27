/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "OMXCodecWrapper.h"

#include <binder/ProcessState.h>
#include <media/ICrypto.h>
#include <media/IOMX.h>
#include <OMX_Component.h>
#include <stagefright/MediaDefs.h>
#include <stagefright/MediaErrors.h>

#include <mozilla/Monitor.h>

using namespace mozilla;
using namespace mozilla::layers;

namespace android {

#define ENCODER_CONFIG_BITRATE 200000 // bps
// How many seconds between I-frames.
#define ENCODER_CONFIG_I_FRAME_INTERVAL 1
// Wait up to 5ms for input buffers.
#define INPUT_BUFFER_TIMEOUT_US (5 * 1000ll)

OMXCodecWrapper*
OMXCodecWrapper::CreateEncoder(Type aMediaType) {
  OMXCodecWrapper* codec = new OMXCodecWrapper(aMediaType);
  if (codec->mCodec == nullptr) {
    delete codec;
    codec = nullptr;
  }
  return codec;
}

OMXCodecWrapper::OMXCodecWrapper(Type aMediaType)
  : mMediaType(aMediaType), mStarted(false)
{
  ProcessState::self()->startThreadPool();

  mLooper = new ALooper();
  mLooper->start();

  if (mMediaType == Type::VIDEO) {
    mCodec = MediaCodec::CreateByType(mLooper, MEDIA_MIMETYPE_VIDEO_AVC, true);
  } else if (mMediaType == Type::AUDIO) {
    mCodec = MediaCodec::CreateByType(mLooper, MEDIA_MIMETYPE_AUDIO_AAC, true);
  }
}

OMXCodecWrapper::~OMXCodecWrapper()
{
  if (mCodec.get()) {
    Stop();
    mCodec->release();
  }
  mLooper->stop();
}

status_t
OMXCodecWrapper::Start()
{
  // Already started.
  if (mStarted) {
    return OK;
  }

  status_t err = mCodec->start();
  mStarted = (err == OK);

  // Get references to MediaCodec buffers.
  if (err == OK) {
    mCodec->getInputBuffers(&mInputBufs);
    mCodec->getOutputBuffers(&mOutputBufs);
  }

  return err;
}

status_t
OMXCodecWrapper::Stop()
{
  // Already stopped.
  if (!mStarted) {
    return OK;
  }

  status_t err = mCodec->stop();
  mStarted = !(err == OK);

  return err;
}

nsresult
OMXCodecWrapper::ConfigureVideo(int aWidth, int aHeight, int aFrameRate)
{
  MOZ_ASSERT(mMediaType == Type::VIDEO && !mStarted);

  NS_ENSURE_TRUE(aWidth > 0 && aHeight > 0 && aFrameRate > 0,
                 NS_ERROR_INVALID_ARG);

  // Set up configuration parameters for AVC/H.264 encoder.
  sp<AMessage> format = new AMessage;
  // Fixed values
  format->setString("mime", MEDIA_MIMETYPE_VIDEO_AVC);
  format->setInt32("bitrate", ENCODER_CONFIG_BITRATE);
  format->setInt32("i-frame-interval", ENCODER_CONFIG_I_FRAME_INTERVAL);
  // See mozilla::layers::GrallocImage, supports YUV 4:2:0, CbCr width and
  // height is half that of Y
  format->setInt32("color-format", OMX_COLOR_FormatYUV420SemiPlanar);
  format->setInt32("profile", OMX_VIDEO_AVCProfileBaseline);
  format->setInt32("level", OMX_VIDEO_AVCLevel3);
  format->setInt32("bitrate-mode", OMX_Video_ControlRateConstant);
  format->setInt32("store-metadata-in-buffers", 0);
  format->setInt32("prepend-sps-pps-to-idr-frames", 0);
  // Input values.
  format->setInt32("width", aWidth);
  format->setInt32("height", aHeight);
  format->setInt32("stride", aWidth);
  format->setInt32("slice-height", aHeight);
  format->setInt32("frame-rate", aFrameRate);

  status_t err = mCodec->configure(format, nullptr, nullptr,
                                   MediaCodec::CONFIGURE_FLAG_ENCODE);
  if (err == OK) {
    err = Start();
  }
  return err == OK ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
OMXCodecWrapper::ConfigureAudio(int aChannels, int aSamplingRate)
{
  MOZ_ASSERT(mMediaType == Type::AUDIO && !mStarted);

  NS_ENSURE_TRUE(aChannels > 0 && aSamplingRate > 0, NS_ERROR_INVALID_ARG);

  // Set up configuration parameters for AAC encoder.
  sp<AMessage> format = new AMessage;
  // Fixed values.
  format->setString("mime", MEDIA_MIMETYPE_AUDIO_AAC);
  format->setInt32("bitrate", kAACBitrate);
  format->setInt32("profile", OMX_AUDIO_AACObjectLC);
  // Input values.
  format->setInt32("channel-count", aChannels);
  format->setInt32("sample-rate", aSamplingRate);

  status_t err = mCodec->configure(format, nullptr, nullptr,
                                   MediaCodec::CONFIGURE_FLAG_ENCODE);

  if (err == OK) {
    err = Start();
  }
  return err == OK ? NS_OK : NS_ERROR_FAILURE;
}

void
OMXCodecWrapper::EncodeVideoFrame(const Image& aImage, int64_t aTimestamp,
                                  int aInputFlags)
{
  MOZ_ASSERT(mStarted && mMediaType == Type::VIDEO);

  GrallocImage& nativeImage = const_cast<GrallocImage&>(
                              static_cast<const GrallocImage&>(aImage));
  SurfaceDescriptor handle = nativeImage.GetSurfaceDescriptor();
  SurfaceDescriptorGralloc grallocHandle = handle.get_SurfaceDescriptorGralloc();
  sp<GraphicBuffer> graphicBuffer = GrallocBufferActor::GetFrom(grallocHandle);

  // Size of PLANAR_YCBCR 4:2:0.
  uint32_t frameLen = graphicBuffer->getWidth()*graphicBuffer->getHeight()*3/2;
  //
  void *imgPtr = nullptr;
  graphicBuffer->lock(GraphicBuffer::USAGE_SW_READ_MASK, &imgPtr);
  PushInput(imgPtr, frameLen, aTimestamp, 0, aInputFlags,
            INPUT_BUFFER_TIMEOUT_US);
  graphicBuffer->unlock();
}

void
OMXCodecWrapper::EncodeVideoFrame(const nsTArray<uint8_t>& aImage,
                                  int64_t aTimestamp, int aInputFlags)
{
  MOZ_ASSERT(mStarted && mMediaType == Type::VIDEO);

  PushInput(aImage.Elements(), aImage.Length(), aTimestamp, 0, aInputFlags,
            INPUT_BUFFER_TIMEOUT_US);
}

void
OMXCodecWrapper::EncodeAudioSamples(const nsTArray<AudioDataValue>& aSamples,
                                    int64_t aTimestamp, int64_t aDuration,
                                    int aInputFlags)
{
// MediaCodec accepts only 16-bit PCM data.
#ifndef MOZ_SAMPLE_TYPE_S16
  MOZ_ASSERT(false);
#endif
  MOZ_ASSERT(mStarted && mMediaType == Type::AUDIO);

  PushInput(aSamples.Elements(), aSamples.Length() * sizeof(int16_t),
            aTimestamp, aDuration, aInputFlags, INPUT_BUFFER_TIMEOUT_US);
}

status_t
OMXCodecWrapper::PushInput(const void* aData, size_t aSize, int64_t aTimestamp,
                           int64_t aDuration, int aInputFlags, int64_t aTimeOut)
{
  MOZ_ASSERT(aData != nullptr);

  status_t err;
  size_t copied = 0;
  int64_t timeOffset = 0;
  do {
    // Dequeue an input buffer.
    uint32_t index;
    err = mCodec->dequeueInputBuffer(&index, aTimeOut);
    if (err != OK) {
      break;
    }
    const sp<ABuffer>& inBuf = mInputBufs.itemAt(index);
    size_t bufferSize = inBuf->capacity();

    size_t remain = aSize - copied;
    size_t toCopy = remain > bufferSize? bufferSize:remain;
    inBuf->setRange(0, toCopy);

    // Copy data to this input buffer.
    memcpy(inBuf->data(), aData + copied, toCopy);
    copied += toCopy;
    timeOffset = aDuration * copied / aSize;

    int inFlags = aInputFlags;
    // Don't signal EOS if there is still data to copy.
    if (copied < aSize) {
      inFlags &= ~BUFFER_EOS;
    }
    // Queue this input buffer.
    err = mCodec->queueInputBuffer(index, 0, toCopy,
                                   aTimestamp + timeOffset, inFlags);
    if (err != OK) {
      break;
    }
  } while (copied < aSize);

  return err;
}

void
OMXCodecWrapper::GetNextEncodedFrame(nsTArray<uint8_t>* aOutputBuf,
                                     int64_t* aOutputTimestamp,
                                     int* aOutputFlags, int64_t aTimeOut)
{
  MOZ_ASSERT(mStarted);

  // Dequeue a buffer from output buffers.
  size_t index = 0;
  size_t outOffset = 0;
  size_t outSize = 0;
  int64_t outTimeUs = 0;
  uint32_t outFlags = 0;
  bool retry = false;
  do {
    status_t err = mCodec->dequeueOutputBuffer(&index, &outOffset, &outSize,
                           &outTimeUs, &outFlags, aTimeOut);
    switch (err) {
      case OK:
        break;
      case INFO_OUTPUT_BUFFERS_CHANGED:
        // Update our references to new buffers.
        err = mCodec->getOutputBuffers(&mOutputBufs);
        // Get output from a new buffer.
        retry = true;
        break;
      case INFO_FORMAT_CHANGED:
        return;
      case -EAGAIN:
        return;
      default:
        MOZ_ASSERT(false);
    }
  } while (retry);

  if (aOutputBuf) {
    sp<ABuffer> outBuf = mOutputBufs.itemAt(index);
    aOutputBuf->SetLength(outSize);
    memcpy(aOutputBuf->Elements(), outBuf->data(), outSize);
  }
  mCodec->releaseOutputBuffer(index);

  if (aOutputTimestamp) {
    *aOutputTimestamp = outTimeUs;
  }

  if (aOutputFlags) {
    *aOutputFlags = outFlags;
  }
}

}
