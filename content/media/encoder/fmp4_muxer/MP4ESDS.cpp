/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include "ISOCompositor.h"
#include "ISOMediaBoxes.h"
#include "MP4ESDS.h"

namespace mozilla {

AACTrackMetadata::AACTrackMetadata()
 : AACProfile(0)
 , MaxBitrate(0)
 , AvgBitrate(0)
 , SampleRate(0)
 , FrameDuration(0)
 , FrameSize(0)
 , Channels(0)
{
}

SLConfigDescriptor::SLConfigDescriptor(ISOCompositor* aCompositor)
  : mCompositor(aCompositor)
{
}

nsresult
SLConfigDescriptor::Generate(uint32_t* aSize)
{
  // hack, omx aac encoder always generates following data
  SlConfigInfo.AppendElement(0x06);
  SlConfigInfo.AppendElement(0x01);
  SlConfigInfo.AppendElement(0x02);
  *aSize = SlConfigInfo.Length();
  return NS_OK;
}

nsresult
SLConfigDescriptor::Write()
{
  uint32_t len = SlConfigInfo.Length();
  for (uint32_t i = 0; i < len; i++) {
    mCompositor->Write(SlConfigInfo[i]);
  }
  return NS_OK;
}

nsresult
DecoderConfigDescriptor::Generate(uint32_t* aBoxSize)
{
  Box::MetaHelper meta;
  meta.Init(mCompositor);
  objectProfileIndication = meta.mAudMeta->AACProfile;
  streamType = kAudioStreamType;
  // Mono AAC buffer size is 6144 bits in spec 14496-3
  // '4.5.3.1 Minimum decoder input buffer'.
  std::bitset<24> tmp_bufferSize (6144 / CHAR_BIT * meta.mAudMeta->Channels);
  bufferSizeDB = tmp_bufferSize;
  maxBitrate = meta.mAudMeta->MaxBitrate;
  avgBitrate = meta.mAudMeta->AvgBitrate;

  // android omx aac encoder always generates following data.
  // 0x05, 0x02, 0x14, 0x08
  nsresult rv;
  Fragmentation* frag = mCompositor->GetFragment(Audio_Track);
  rv = frag->GetCSD(DecodeSpecificInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  length = sizeof(objectProfileIndication) +
           (streamType.size() + upStream.size() + reserved.size() + bufferSizeDB.size()) / CHAR_BIT +
           sizeof(maxBitrate) +
           sizeof(avgBitrate) +
           DecodeSpecificInfo.Length();

  *aBoxSize = length + sizeof(tag) + sizeof(length);
  return NS_OK;
}

nsresult
DecoderConfigDescriptor::Write()
{
  mCompositor->Write(tag);
  mCompositor->Write(length);
  mCompositor->Write(objectProfileIndication);
  mCompositor->WriteBits(streamType.to_ulong(), streamType.size());
  mCompositor->WriteBits(upStream.to_ulong(), upStream.size());
  mCompositor->WriteBits(reserved.to_ulong(), reserved.size());
  mCompositor->WriteBits(bufferSizeDB.to_ulong(), bufferSizeDB.size());
  mCompositor->Write(maxBitrate);
  mCompositor->Write(avgBitrate);
  mCompositor->Write(DecodeSpecificInfo.Elements(), DecodeSpecificInfo.Length());

  return NS_OK;
}

DecoderConfigDescriptor::DecoderConfigDescriptor(ISOCompositor* aCompositor)
  : tag(DecConfigDescrTag)
  , length(0)
  , objectProfileIndication(0)
  , streamType(0)
  , upStream(0)
  , reserved(1)
  , bufferSizeDB(0)
  , maxBitrate(0)
  , avgBitrate(0)
  , mCompositor(aCompositor)
{
}

nsresult
ES_Descriptor::Write()
{
  // TODO lack of a size checker?
  mCompositor->Write(tag);
  mCompositor->Write(length);
  mCompositor->Write(ES_ID);
  mCompositor->WriteBits(streamDependenceFlag.to_ulong(), streamDependenceFlag.size());
  mCompositor->WriteBits(URL_Flag.to_ulong(), URL_Flag.size());
  mCompositor->WriteBits(reserved.to_ulong(), reserved.size());
  mCompositor->WriteBits(streamPriority.to_ulong(), streamPriority.size());
  decConfigDescriptor->Write();
  slConfigDescriptor->Write();
  return NS_OK;
}

nsresult
ES_Descriptor::Generate(uint32_t* aBoxSize)
{
  nsresult rv;
  uint32_t size;

  length = sizeof(ES_ID) + 1;
  rv = decConfigDescriptor->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  length += size;
  rv = slConfigDescriptor->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  length += size;

  *aBoxSize = sizeof(tag) + sizeof(length) + length;
  return NS_OK;
}

ES_Descriptor::ES_Descriptor(ISOCompositor* aCompositor)
  : tag(ESDescrTag)
  , length(0)
  , ES_ID(0)
  , streamDependenceFlag(0)
  , URL_Flag(0)
  , reserved(0)
  , streamPriority(0)
  , mCompositor(aCompositor)
{
  decConfigDescriptor = new DecoderConfigDescriptor(aCompositor);
  slConfigDescriptor = new SLConfigDescriptor(aCompositor);
}

}
