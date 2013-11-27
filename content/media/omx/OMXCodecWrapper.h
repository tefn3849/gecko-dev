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

/**
 * This class wraps video and audio encoders provided by MediaCodec API in
 * libstagefright. Each object represents either a AVC/H.264 or AAC encoder that
 * can encode one track.
 */
class OMXCodecWrapper MOZ_FINAL
{
public:
  typedef mozilla::MediaSegment::Type Type;

  // Input and output flags
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
  };

  /**
   * Create a media codec. It will be AVC/H.264 encoder if aMediaType is
   * MediaSegment::Type::VIDEO, or AAC encoder if aMediaType is
   * MediaSegment::Type::AUDIO. Returns nullptr when fail.
   */
  static OMXCodecWrapper* CreateEncoder(Type aMediaType);

  ~OMXCodecWrapper();

  NS_INLINE_DECL_REFCOUNTING(OMXCodecWrapper)

  /**
   * Configure video codec parameters and start media codec. It must be called
   * before calling EncodeVideoFrame() and GetNextEncodedFrame().
   */
  nsresult ConfigureVideo(int aWidth, int aHeight, int aFrameRate);

  /**
   * Configure audio codec parameters and start media codec. It must be called
   * before calling EncodeAudioSamples() and GetNextEncodedFrame().
   */
  nsresult ConfigureAudio(int aChannelCount, int aSampleRate);

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

  /**
   * Encode 16-bit PCM audio samples stored in aSamples array. The
   * timestamp/presentation time (in microseconds) of 1st audio sample is given
   * in aTimestamp and duration (also in microseconds) in aDuration. To notify
   * end of stream, set aInputFlags to BUFFER_EOS.
   */
  void EncodeAudioSamples(const nsTArray<mozilla::AudioDataValue>& aSamples,
                          int64_t aTimestamp, int64_t aDuration,
                          int aInputFlags = 0);

  /**
   * Get the next available encoded data from MediaCodec. The data will be
   * copied into aOutputBuf array, with its timestamp (in microseconds) in
   * aOutputTimestamp.
   * Wait at most aTimeout microseconds to dequeue a outpout buffer.
   */
  void GetNextEncodedFrame(nsTArray<uint8_t>* aOutputBuf,
                           int64_t* aOutputTimestamp, int* aOutputFlags,
                           int64_t aTimeOut);

private:
  // User should call CreateEncoder() to get a media codec instead.
  OMXCodecWrapper() {}
  OMXCodecWrapper(const OMXCodecWrapper&) {}
  OMXCodecWrapper& operator=(const OMXCodecWrapper&) { return *this; }
  /**
   * This will create a AVC/H.264 encoder if aMediaType is
   * MediaSegment::Type::VIDEO, or AAC encoder if aMediaType is
   * MediaSegment::Type::AUDIO
   */
  OMXCodecWrapper(const Type aMediaType);

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

  Type mMediaType; // Video or audio?
  bool mStarted; // Has MediaCodec been started?
};

}
#endif
