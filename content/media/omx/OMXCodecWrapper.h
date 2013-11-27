/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef OMXCodecWrapper_h_
#define OMXCodecWrapper_h_

#include <gui/Surface.h>
#include <stagefright/foundation/AMessage.h>
#include <stagefright/foundation/ABuffer.h>
#include <stagefright/MediaCodec.h>

#include "AudioSampleFormat.h"
#include "MediaSegment.h"
#include "GonkNativeWindow.h"
#include "GonkNativeWindowClient.h"

namespace android {

class OMXAudioEncoder;
class OMXVideoEncoder;

/**
 * This class (and its subclasses) wraps the video and audio codec from
 * MediaCodec API in libstagefright. Currently only AVC/H.264 video encoder and
 * AAC audio encoder are supported.
 *
 * OMXCodecWrapper has static creator functions that returns actual codec
 * instances for different types of codec supported and serves as superclass to
 * provide a function to read encoded data as byte array from codec. Two
 * subclasses, OMXAudioEncoder and OMXVideoEncoder, respectively provides
 * functions for encoding data from audio and video track.
 *
 * A typical usage is as follows:
 * - Call one of the creator function Create...() to get either a
 *   OMXAudioEncoder or OMXVideoEncoder object.
 * - Configure codec by providing characteristics of input raw data, such as
 *   video frame width and height, using Configure...().
 * - Send raw data (and notify end of stream) with Encode...().
 * - Get encoded data through GetNextEncodedFrame().
 * - Repeat previous 2 steps until end of stream.
 * - Destory the object.
 *
 * The lifecycle of underlying OMX codec is binded with construction and
 * descruction of OMXCodecWrapper and subclass objects. For some types of
 * codecs, such as HW accelarated AVC/H.264 encoder, there can be only one
 * instance system-wise at a time, attempting to create another instance will
 * fail.
 */
class OMXCodecWrapper
{
public:
  // Codec types.
  enum CodecType {
    AAC_ENC, // AAC encoder.
    AVC_ENC, // AVC/H.264 encoder.
    TYPE_COUNT
  };

  // Input and output flags.
  enum {
    // For EncodeVideoFrame() and EncodeAudioSamples(), it indicates the end of
    // input stream; For GetNextEncodedFrame(), it indicates the end of output
    // stream.
    BUFFER_EOS = MediaCodec::BUFFER_FLAG_EOS,
    // For GetNextEncodedFrame(). It indicates the output buffer is an I-frame.
    BUFFER_SYNC_FRAME = MediaCodec::BUFFER_FLAG_SYNCFRAME,
    // For GetNextEncodedFrame(). It indicates that the output buffer contains
    // codec specific configuration info. (SPS & PPS for AVC/H.264;
    // DecoderSpecificInfo for AAC)
    BUFFER_CODEC_CONFIG = MediaCodec::BUFFER_FLAG_CODECCONFIG,
  };

  // Hard-coded values for AAC DecoderConfigDescriptor in libstagefright.
  // See MPEG4Writer::Track::writeMp4aEsdsBox()
  // Exposed for the need of MP4 container writer.
  enum {
    kAACBitrate = 96000, // kbps
    kAACFrameSize = 768, // bytes
    kAACFrameDuration = 1024, // How many samples per AAC frame.
  };

  /** Create a AAC audio encoder. Returns nullptr when failed. */
  static already_AddRefed<OMXAudioEncoder> CreateAACEncoder();

  /** Create a AVC/H.264 video encoder. Returns nullptr when failed. */
  static already_AddRefed<OMXVideoEncoder> CreateAVCEncoder();

  virtual ~OMXCodecWrapper();

  NS_INLINE_DECL_REFCOUNTING(OMXCodecWrapper)

  /**
   * Get the next available encoded data from MediaCodec. The data will be
   * copied into aOutputBuf array, with its timestamp (in microseconds) in
   * aOutputTimestamp.
   * Wait at most aTimeout microseconds to dequeue a outpout buffer.
   */
  void GetNextEncodedFrame(nsTArray<uint8_t>* aOutputBuf,
                           int64_t* aOutputTimestamp, int* aOutputFlags,
                           int64_t aTimeOut);

  /** Get the media codec type */
  int GetCodecType() { return mCodecType; }

private:
  // Hide these. User should always use creator functions to get a media codec.
  OMXCodecWrapper() MOZ_DELETE;
  OMXCodecWrapper(const OMXCodecWrapper&) MOZ_DELETE;
  OMXCodecWrapper& operator=(const OMXCodecWrapper&) MOZ_DELETE;

  /**
   * Create a media codec of given type. It will be a AVC/H.264 video encoder if
   * aCodecType if CODEC_AAC_ENC, or AAC audio encoder if aCodecType is
   * CODEC_AVC_ENC.
   */
  OMXCodecWrapper(CodecType aCodecType);

  // For subclasses to access hidden constructor and implementation details.
  friend class OMXAudioEncoder;
  friend class OMXVideoEncoder;

  /**
   * Start the media codec.
   */
  status_t Start();

  /**
   * Stop the media codec.
   */
  status_t Stop();

  /**
   * Push a buffer of raw audio or video data into the input buffers of
   * MediaCodec. aData is the pointer of buffer, with buffer size of aSize and
   * its timestamp value (in microseconds) in aTimestamp. aDuration indicates
   * the duration of audio data given. For video, it should be 0.
   * Wait at most aTimeout microseconds to dequeue a buffer out from the input
   * buffers.
   */
  status_t PushInput(const void* aData, size_t aSize, int64_t aTimestamp,
                    int64_t aDuration, int aInputFlags, int64_t aTimeOut);

  // The actual codec instance provided by libstagefright.
  sp<MediaCodec> mCodec;

  // A dedicate message loop with its own thread used by MediaCodec.
  sp<ALooper> mLooper;

  Vector<sp<ABuffer> > mInputBufs; // MediaCodec buffers to hold input data.
  Vector<sp<ABuffer> > mOutputBufs; // MediaCodec buffers to hold output data.

  CodecType mCodecType;
  bool mStarted; // Has MediaCodec been started?
};

/**
 * Audio encoder.
 */
class OMXAudioEncoder MOZ_FINAL : public OMXCodecWrapper {
public:
  /**
   * Configure audio codec parameters and start media codec. It must be called
   * before calling EncodeAudioSamples() and GetNextEncodedFrame().
   */
  nsresult ConfigureAudio(int aChannelCount, int aSampleRate);

  /**
   * Encode 16-bit PCM audio samples stored in aSamples array. The
   * timestamp/presentation time (in microseconds) of 1st audio sample is given
   * in aTimestamp and duration (also in microseconds) in aDuration. To notify
   * end of stream, set aInputFlags to BUFFER_EOS.
   */
  void EncodeAudioSamples(const nsTArray<mozilla::AudioDataValue>& aSamples,
                          int64_t aTimestamp, int64_t aDuration,
                          int aInputFlags = 0);

private:
  // Hide these. User should always use creator functions to get a media codec.
  OMXAudioEncoder() MOZ_DELETE;
  OMXAudioEncoder(const OMXAudioEncoder&) MOZ_DELETE;
  OMXAudioEncoder& operator=(const OMXAudioEncoder&) MOZ_DELETE;

  /**
   * Create a audio codec. It will be a AAC encoder if aCodecType is
   * CODEC_AAC_ENC.
   */
  OMXAudioEncoder(CodecType aCodecType): OMXCodecWrapper(aCodecType) {}

  // For creator function to access hidden constructor.
  friend class OMXCodecWrapper;
};

/**
 * Video encoder.
 */
class OMXVideoEncoder MOZ_FINAL : public OMXCodecWrapper {
public:
  /**
   * Configure video codec parameters and start media codec. It must be called
   * before calling EncodeVideoFrame() and GetNextEncodedFrame().
   */
  nsresult ConfigureVideo(int aWidth, int aHeight, int aFrameRate);

  /**
   * Encode a video frame of semi-planar YUV420 format stored in the buffer of
   * aImage. aTimestamp gives the frame timestamp/presentation time (in
   * microseconds). To notify end of stream, set aInputFlags to BUFFER_EOS.
   */
  void EncodeVideoFrame(const mozilla::layers::Image& aImage,
                        int64_t aTimestamp, int aInputFlags = 0);

  /**
   * Encode a video frame of semi-planar YUV420 format stored in the aImage
   * array. aTimestamp gives the frame timestamp/presentation time (in
   * microseconds). To notify end of stream, set aInputFlags to BUFFER_EOS.
   */
  void EncodeVideoFrame(const nsTArray<uint8_t>& aImage,
                        int64_t aTimestamp, int aInputFlags = 0);

private:
  // Hide these. User should always use creator functions to get a media codec.
  OMXVideoEncoder() MOZ_DELETE;
  OMXVideoEncoder(const OMXVideoEncoder&) MOZ_DELETE;
  OMXVideoEncoder& operator=(const OMXVideoEncoder&) MOZ_DELETE;

  /**
   * Create a video codec. It will be a AVC/H.264 encoder if aCodecType is
   * CODEC_AVC_ENC.
   */
  OMXVideoEncoder(CodecType aCodecType): OMXCodecWrapper(aCodecType) {}

  // For creator function to access hidden constructor.
  friend class OMXCodecWrapper;
};

}
#endif
