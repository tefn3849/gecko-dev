/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MP4ESDS_h_
#define MP4ESDS_h_

#include <bitset>
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "MuxerOperation.h"

namespace mozilla {

class ISOCompositor;

/**
 * ESDS tag
 */
#define ESDescrTag        0x03
#define DecConfigDescrTag 0x04

/**
 * 14496-1 '8.3.4 DecoderConfigDescriptor'
 */
class DecoderConfigDescriptor : public MuxerOperation {
public:
  uint8_t tag;                      // DecConfigDescrTag
  uint8_t length;
  uint8_t objectProfileIndication;  // One of item in k14496_ table.
  std::bitset<6> streamType;        // Elementary stream type, it could be kAudioStreamType
                                    // or kVideoStreamType.
  std::bitset<1> upStream;
  std::bitset<1> reserved;
  std::bitset<24> bufferSizeDB;     // decoder input buffer size, 14496-3
                                    // 4.5.3.1 'Minimum decoder input buffer'
  uint32_t maxBitrate;
  uint32_t avgBitrate;

  /**
   * DecodeSpecificInfo is from AAC encoder directly, the first sample from
   * AAC encoder should be this part of data.
   */
  nsTArray<uint8_t> DecodeSpecificInfo;

  nsresult Generate(uint32_t* aBoxSize);
  nsresult Write();

  DecoderConfigDescriptor(ISOCompositor* aCompositor);

protected:
  ISOCompositor* mCompositor;
};

/**
 * 14496-1 '10.2.3 SL Packet Header Configuration'
 */
class SLConfigDescriptor : public MuxerOperation {
public:
  nsTArray<uint8_t> SlConfigInfo;

  nsresult Generate(uint32_t* aBoxSize);
  nsresult Write();

  SLConfigDescriptor(ISOCompositor* aCompositor);

protected:
  ISOCompositor* mCompositor;
};

/**
 * 14496-1 '8.3.3 ES_Descriptor'.
 * It supports DecoderConfigDescriptor and SLConfigDescriptor.
 */
class ES_Descriptor : public MuxerOperation {
public:
  uint8_t tag;      // ESDescrTag
  uint8_t length;
  uint16_t ES_ID;
  std::bitset<1> streamDependenceFlag;
  std::bitset<1> URL_Flag;
  std::bitset<1> reserved;
  std::bitset<5> streamPriority;

  nsRefPtr<DecoderConfigDescriptor> decConfigDescriptor;
  nsRefPtr<SLConfigDescriptor> slConfigDescriptor;

  nsresult Generate(uint32_t* aBoxSize);
  nsresult Write();

  ES_Descriptor(ISOCompositor* aCompositor);

protected:
  ISOCompositor* mCompositor;
};

}

#endif
