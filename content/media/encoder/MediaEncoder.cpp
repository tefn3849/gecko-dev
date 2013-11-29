/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaEncoder.h"
#include "MediaDecoder.h"
#include "nsIPrincipal.h"
#include "nsMimeTypes.h"

#define WEBM_ENCODER defined(MOZ_VP8_ENCODER) && defined(MOZ_VORBIS_ENCODER)

#ifdef MOZ_OGG
#include "OggWriter.h"
#endif
#ifdef MOZ_OPUS
#include "OpusTrackEncoder.h"
#endif
#if WEBM_ENCODER
#include "VorbisTrackEncoder.h"
#include "VP8TrackEncoder.h"
#include "WebMWriter.h"
#endif
#ifdef MOZ_OMX_ENCODER
#include "OmxTrackEncoder.h"
#include "ISOMediaWriter.h"
#endif
#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "MediaEncoder", ## args);
#else
#define LOG(args,...)
#endif

namespace mozilla {

#define TRACK_BUFFER_LEN 8192

void
MediaEncoder::NotifyQueuedTrackChanges(MediaStreamGraph* aGraph,
                                       TrackID aID,
                                       TrackRate aTrackRate,
                                       TrackTicks aTrackOffset,
                                       uint32_t aTrackEvents,
                                       const MediaSegment& aQueuedMedia)
{
  // Process the incoming raw track data from MediaStreamGraph, called on the
  // thread of MediaStreamGraph.
  if (aQueuedMedia.GetType() == MediaSegment::AUDIO) {
    mAudioEncoder->NotifyQueuedTrackChanges(aGraph, aID, aTrackRate,
                                            aTrackOffset, aTrackEvents,
                                            aQueuedMedia);

  } else {
    // Type video is not supported for now.
    if (mVideoEncoder) {
      mVideoEncoder->NotifyQueuedTrackChanges(aGraph, aID, aTrackRate,
                                              aTrackOffset, aTrackEvents,
                                              aQueuedMedia);
    }
  }
}

void
MediaEncoder::NotifyRemoved(MediaStreamGraph* aGraph)
{
  // In case that MediaEncoder does not receive a TRACK_EVENT_ENDED event.
  LOG("NotifyRemoved in [MediaEncoder].");
  mAudioEncoder->NotifyRemoved(aGraph);
}
/* static */
void
MediaEncoder::GetDefaultEncodeMIMEType(uint8_t aTrackTypes, nsAString& aMIMEType)
{
  aMIMEType = NS_LITERAL_STRING(VIDEO_MP4);
  return;

  MOZ_ASSERT(aTrackTypes, "Should has aTrackTypes");
  if (aTrackTypes == ContainerWriter::HAS_AUDIO) {
    aMIMEType = NS_LITERAL_STRING(AUDIO_OGG);
    return;
  }
  if (aTrackTypes & ContainerWriter::HAS_VIDEO ) {
#if defined(MOZ_OMX_ENCODER)
    aMIMEType = NS_LITERAL_STRING(VIDEO_MP4);
#elif WEBM_ENCODER
    aMIMEType = NS_LITERAL_STRING(VIDEO_WEBM);
#endif
    return;
  }
}

/* static */
already_AddRefed<MediaEncoder>
MediaEncoder::CreateEncoder(const nsAString& aMIMEType, uint8_t aTrackTypes)
{
  nsAutoPtr<ContainerWriter> writer;
  nsAutoPtr<AudioTrackEncoder> audioEncoder;
  nsAutoPtr<VideoTrackEncoder> videoEncoder;
  nsRefPtr<MediaEncoder> encoder;
  nsString mimeType;
  if (aMIMEType.IsEmpty()) {
    GetDefaultEncodeMIMEType(aTrackTypes, mimeType);
    aTrackTypes = ContainerWriter::HAS_AUDIO;
  } else {
    mimeType = aMIMEType;
  }

  if (mimeType.Equals(NS_LITERAL_STRING(AUDIO_OGG))) {
#ifdef MOZ_OGG
    writer = new OggWriter();
#ifdef MOZ_OPUS
    audioEncoder = new OpusTrackEncoder();
#endif
#endif
    NS_ENSURE_TRUE(writer, nullptr);
    NS_ENSURE_TRUE(audioEncoder, nullptr);
  } else if (mimeType.Equals(NS_LITERAL_STRING(VIDEO_WEBM))) {
    if (aTrackTypes & ContainerWriter::HAS_AUDIO) {
#if WEBM_ENCODER
      audioEncoder = new OpusTrackEncoder();
#endif
      NS_ENSURE_TRUE(audioEncoder, nullptr);
    }
#if WEBM_ENCODER
    videoEncoder = new VTrackEncoder();
    writer = new WebMWriter(aTrackTypes);
#endif
    NS_ENSURE_TRUE(writer, nullptr);
    NS_ENSURE_TRUE(videoEncoder, nullptr);
  } else if (mimeType.Equals(NS_LITERAL_STRING(VIDEO_MP4))) {
    if (aTrackTypes & ContainerWriter::HAS_AUDIO) {
#ifdef MOZ_OMX_ENCODER
      audioEncoder = new OmxAudioTrackEncoder();
#endif
      NS_ENSURE_TRUE(audioEncoder, nullptr);
    }
#ifdef MOZ_OMX_ENCODER
    //videoEncoder = new OmxVideoTrackEncoder();
    writer = new ISOMediaWriter(aTrackTypes);
#endif
    NS_ENSURE_TRUE(writer, nullptr);
    //NS_ENSURE_TRUE(videoEncoder, nullptr);
  } else {
    return nullptr;
  }

  encoder = new MediaEncoder(writer.forget(), audioEncoder.forget(),
                             videoEncoder.forget(), mimeType);
  return encoder.forget();
}

/**
 * GetEncodedData() runs as a state machine, starting with mState set to
 * GET_METADDATA, the procedure should be as follow:
 *
 * While non-stop
 *   If mState is GET_METADDATA
 *     Get the meta data from audio/video encoder
 *     If a meta data is generated
 *       Get meta data from audio/video encoder
 *       Set mState to ENCODE_TRACK
 *       Return the final container data
 *
 *   If mState is ENCODE_TRACK
 *     Get encoded track data from audio/video encoder
 *     If a packet of track data is generated
 *       Insert encoded track data into the container stream of writer
 *       If the final container data is copied to aOutput
 *         Return the copy of final container data
 *       If this is the last packet of input stream
 *         Set mState to ENCODE_DONE
 *
 *   If mState is ENCODE_DONE or ENCODE_ERROR
 *     Stop the loop
 */
void
MediaEncoder::GetEncodedData(nsTArray<nsTArray<uint8_t> >* aOutputBufs,
                             nsAString& aMIMEType)
{
  MOZ_ASSERT(!NS_IsMainThread());

  aMIMEType = mMIMEType;

  bool reloop = true;
  while (reloop) {
    switch (mState) {
    case ENCODE_METADDATA: {
      nsresult rv = SetEncodedMetadataToMuxer(mAudioEncoder.get());
      if (NS_FAILED(rv)) {
        break;
      }
      rv = SetEncodedMetadataToMuxer(mVideoEncoder.get());
      if (NS_FAILED(rv)) {
        break;
      }

      rv = mWriter->GetContainerData(aOutputBufs,
                                     ContainerWriter::GET_HEADER);
      if (NS_FAILED(rv)) {
       LOG("ERROR! writer fail to generate header!");
       mState = ENCODE_ERROR;
       break;
      }

      mState = ENCODE_TRACK;
      break;
    }

    case ENCODE_TRACK: {
      EncodedFrameContainer encodedData;
      nsresult rv = NS_OK;
      rv = WrtieEncodedDataToMuxer(mAudioEncoder.get());
      if (NS_FAILED(rv)) {
        break;
      }
      rv = WrtieEncodedDataToMuxer(mVideoEncoder.get());
      if (NS_FAILED(rv)) {
        break;
      }

      bool isFinished = mAudioEncoder != nullptr ? mAudioEncoder->IsEncodingComplete() : true;
      isFinished &= mVideoEncoder != nullptr ? mVideoEncoder->IsEncodingComplete() : true;
      rv = mWriter->GetContainerData(aOutputBufs,
                                     isFinished ?
                                     ContainerWriter::FLUSH_NEEDED : 0);
      if (NS_SUCCEEDED(rv)) {
        // Successfully get the copy of final container data from writer.
        reloop = false;
      }
      mState = (mWriter->IsWritingComplete()) ? ENCODE_DONE : ENCODE_TRACK;

      break;
    }

    case ENCODE_DONE:
    case ENCODE_ERROR:
      LOG("MediaEncoder has been shutdown or got Error.");
      mShutdown = true;
      reloop = false;
      break;
    default:
      MOZ_CRASH("Invalid encode state");
    }
  }
}

template<class T> nsresult
MediaEncoder::WrtieEncodedDataToMuxer(T aEncoder)
{
  if (aEncoder == nullptr) {
    return NS_OK;
  }
  if (aEncoder->IsEncodingComplete()) {
    return NS_OK;
  }
  EncodedFrameContainer encodedVideoData;
  nsresult rv = NS_OK;
  LOG("Will GetEncodedTrack()");
  rv = aEncoder->GetEncodedTrack(encodedVideoData);
  if (NS_FAILED(rv)) {
    // Encoding might be canceled.
    LOG("ERROR! Fail to get encoded data from video encoder.");
    mState = ENCODE_ERROR;
    return rv;
  }
  LOG("Did GetEncodedTrack()");
  rv = mWriter->WriteEncodedTrack(encodedVideoData,
                                  aEncoder->IsEncodingComplete() ?
                                  ContainerWriter::END_OF_STREAM : 0);
  if (NS_FAILED(rv)) {
    LOG("ERROR! Fail to write encoded video track to the media container.");
    mState = ENCODE_ERROR;
  }
  return rv;
}

template<class T> nsresult
MediaEncoder::SetEncodedMetadataToMuxer(T aEncoder)
{
  if (aEncoder == nullptr) {
    return NS_OK;
  }
  nsRefPtr<TrackMetadataBase> meta = aEncoder->GetMetadata();
  nsresult rv = NS_OK;
  if (meta == nullptr) {
    mState = ENCODE_ERROR;
    return NS_ERROR_ABORT;
  }

  rv = mWriter->SetMetadata(meta);
  if (NS_FAILED(rv)) {
   mState = ENCODE_ERROR;
  }
  return rv;
}
}
