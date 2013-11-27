/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ISOTrackMetadata_h_
#define ISOTrackMetadata_h_

#include "TrackMetadataBase.h"

namespace mozilla {


/**
 * track type from spec 8.4.3.3
 */
#define Audio_Track 0x01
#define Video_Track 0x02

/**
 * ES profile indication from 14496-1 table 8-5 'objectProfileIndication Values'.
 */
#define k14496_2_SIMPLE 0x20
#define k14496_2_CORE   0x21
#define k14496_2_MAIN   0x22
#define k14496_3_AAC    0x40

/**
 * elementary stream type
 */
#define kVisualStreamType 0x04
#define kAudioStreamType  0x05

/**
 * Audio object type, from ISO 14496-3, table 1.15.
 * We only list part of list here.
 */
#define kAudioObjType_AAC_MAIN 0x01
#define kAudioObjType_AAC_LC   0x02

class AACTrackMetadata : public TrackMetadataBase {
public:
  uint32_t AACProfile;     // It shoule be k14496_3_AAC_MAIN or k14496_3_AAC_LC.
  uint32_t MaxBitrate;
  uint32_t AvgBitrate;
  uint32_t SampleRate;     // From 14496-3 table 1.16, it could be 7350 ~ 96000.
  uint32_t FrameDuration;  // Audio frame duration based on SampleRate.
  uint32_t FrameSize;      // Audio frame size, 0 is variant size.
  uint32_t Channels;       // Channel number, it should be 1 or 2.

  AACTrackMetadata();
  MetadataKind GetKind() const MOZ_OVERRIDE { return METADATA_AAC; }
};

class AVCTrackMetadata : public TrackMetadataBase {
public:
  uint32_t Height;
  uint32_t Width;
  uint32_t VideoFrequence;  // for AVC, it should be 90k Hz.
  uint32_t FrameRate;       // frames per second
  MetadataKind GetKind() const MOZ_OVERRIDE { return METADATA_AVC; }
};

}

#endif
