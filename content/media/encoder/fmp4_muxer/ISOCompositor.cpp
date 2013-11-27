/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "ISOCompositor.h"
#include "ISOMediaBoxes.h"
#include "EncodedFrameContainer.h"

namespace mozilla {

const static uint32_t MUXING_BUFFER_SIZE = 512*1024;

Fragmentation::Fragmentation(uint32_t aTrackType, uint32_t aFragDuration,
                             TrackMetadataBase* aMetadata)
  : mTrackType(aTrackType)
  , mFragDuration(aFragDuration)
  , mSamplePerFragment(0)
  , mEOS(false)
{

  if (mTrackType == Audio_Track) {
    nsRefPtr<AACTrackMetadata> audMeta = static_cast<AACTrackMetadata*>(aMetadata);
    MOZ_ASSERT(audMeta);
    mSamplePerFragment = mFragDuration * audMeta->SampleRate / audMeta->FrameDuration;
  } else {
    nsRefPtr<AVCTrackMetadata> vidMeta = static_cast<AVCTrackMetadata*>(aMetadata);
    MOZ_ASSERT(vidMeta);
    mSamplePerFragment = mFragDuration * vidMeta->FrameRate;
  }
}

bool
Fragmentation::HasEnoughData()
{
  return GetCurrentAvailableSampleNumber() > GetSampleNumberPerFragment();
}

nsresult
Fragmentation::GetFrame(uint32_t aIdx, EncodedFrame** aFrame)
{
  NS_ENSURE_TRUE(aIdx < mFrames.Length(), NS_ERROR_FAILURE);
  NS_ADDREF(*aFrame = mFrames[aIdx]);
  return NS_OK;
}

nsresult
Fragmentation::GetCSD(nsTArray<uint8_t>& aCSD)
{
  if (!mCSDFrame) {
    return NS_ERROR_FAILURE;
  }
  aCSD.AppendElements(mCSDFrame->GetFrameData().Elements(),
                      mCSDFrame->GetFrameData().Length());

  return NS_OK;
}

nsresult
Fragmentation::Flush()
{
  mFrames.ClearAndRetainStorage();
  return NS_OK;
}

nsresult
Fragmentation::AddFrame(EncodedFrame* aFrame)
{
  EncodedFrame::FrameType type = aFrame->GetFrameType();
  if (type == EncodedFrame::AAC_CSD || type == EncodedFrame::AVC_CSD) {
    mCSDFrame = aFrame;
    return NS_OK;
  }
  mFrames.AppendElement(aFrame);
  return NS_OK;
}

uint32_t
Fragmentation::GetSampleNumberPerFragment()
{
  return mSamplePerFragment;
}

uint32_t
Fragmentation::GetCurrentAvailableSampleNumber()
{
  return mFrames.Length();
}

ISOCompositor::ISOCompositor()
  : mAudioFragmentation(nullptr)
  , mVideoFragmentation(nullptr)
  , mFragNum(0)
  , mFragDuration(0)
  , mOutputSize(0)
  , mBitCount(0)
  , mBit(0)
{
  mOutBuffer.SetCapacity(MUXING_BUFFER_SIZE);
}

uint32_t
ISOCompositor::GetNextTrackID()
{
  return (mMetaArray.Length() + 1);
}

uint32_t
ISOCompositor::GetTrackID(uint32_t aTrackType)
{
  TrackMetadataBase::MetadataKind kind;
  if (aTrackType == Audio_Track) {
    kind = TrackMetadataBase::METADATA_AAC;
  } else {
    kind = TrackMetadataBase::METADATA_AVC;
  }

  for (uint32_t i = 0; i < mMetaArray.Length(); i++) {
    if (mMetaArray[i]->GetKind() == kind) {
      return (i + 1);
    }
  }

  return 0;
}

nsresult
ISOCompositor::SetMetadata(TrackMetadataBase* aTrackMeta)
{
  if (aTrackMeta->GetKind() == TrackMetadataBase::METADATA_AAC ||
      aTrackMeta->GetKind() == TrackMetadataBase::METADATA_AVC) {
    mMetaArray.AppendElement(aTrackMeta);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
ISOCompositor::GetAudioMetadata(AACTrackMetadata** aAudMeta)
{
  for (uint32_t i = 0; i < mMetaArray.Length() ; i++) {
    if (mMetaArray[i]->GetKind() == TrackMetadataBase::METADATA_AAC) {
      NS_ENSURE_ARG_POINTER(aAudMeta);
      NS_IF_ADDREF(*aAudMeta = static_cast<AACTrackMetadata*>(mMetaArray[i].get()));
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult
ISOCompositor::GetVideoMetadata(AVCTrackMetadata** aVidMeta)
{
  for (uint32_t i = 0; i < mMetaArray.Length() ; i++) {
    if (mMetaArray[i]->GetKind() == TrackMetadataBase::METADATA_AVC) {
      NS_ENSURE_ARG_POINTER(aVidMeta);
      NS_IF_ADDREF(*aVidMeta = static_cast<AVCTrackMetadata*>(mMetaArray[i].get()));
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

bool
ISOCompositor::HasAudioTrack()
{
  nsRefPtr<AACTrackMetadata> audMeta;
  GetAudioMetadata(getter_AddRefs(audMeta));
  return audMeta;
}

bool
ISOCompositor::HasVideoTrack()
{
  nsRefPtr<AVCTrackMetadata> vidMeta;
  GetVideoMetadata(getter_AddRefs(vidMeta));
  return vidMeta;
}

nsresult
ISOCompositor::SetFragment(Fragmentation* aFragment)
{
  if (aFragment->GetType() == Audio_Track) {
    mAudioFragmentation = aFragment;
  } else {
    mVideoFragmentation = aFragment;
  }
  return NS_OK;
}

Fragmentation*
ISOCompositor::GetFragment(uint32_t aType)
{
  if (aType == Audio_Track) {
    return mAudioFragmentation;
  } else if (aType == Video_Track){
    return mVideoFragmentation;
  }
  MOZ_ASSERT(0);
  return nullptr;
}

nsresult
ISOCompositor::GetBuf(nsTArray<uint8_t>& aOutBuf)
{
  mOutputSize += mOutBuffer.Length();
  aOutBuf.SwapElements(mOutBuffer);
  return FlushBuf();
}

nsresult
ISOCompositor::FlushBuf()
{
  mOutBuffer.SetCapacity(MUXING_BUFFER_SIZE);
  mLastWrittenBoxPos = 0;
  return NS_OK;
}

uint32_t
ISOCompositor::WriteBits(uint64_t aBits, size_t aNumBits)
{
  uint8_t output_byte = 0;

  MOZ_ASSERT(aNumBits <= 64);
  // TODO: rewrriten following with bitset?
  for (size_t i = aNumBits; i > 0; i--) {
    mBit |= (((aBits >> (i - 1)) & 1) << (8 - ++mBitCount));
    if (mBitCount == 8) {
      Write(&mBit, sizeof(uint8_t));
      mBit = 0;
      mBitCount = 0;
      output_byte++;
    }
  }
  return output_byte;
}

uint32_t
ISOCompositor::Write(uint8_t* aBuf, uint32_t aSize)
{
  mOutBuffer.AppendElements(aBuf, aSize);
  return aSize;
}

uint32_t
ISOCompositor::Write(uint8_t aData)
{
  MOZ_ASSERT(!mBitCount);
  Write((uint8_t*)&aData, sizeof(uint8_t));
  return sizeof(uint8_t);
}

uint32_t
ISOCompositor::WriteFourCC(const char* aType)
{
  // Bit operation should be aligned to byte before writing any byte data.
  MOZ_ASSERT(!mBitCount);

  uint32_t size = strlen(aType);
  if (size == 4) {
    return Write((uint8_t*)aType, size);
  }

  return 0;
}

nsresult
ISOCompositor::GenerateFtyp()
{
  nsresult rv;
  uint32_t size;
  nsAutoPtr<FileTypeBox> type_box(new FileTypeBox(this));
  rv = type_box->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = type_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
ISOCompositor::GenerateMoov()
{
  nsresult rv;
  uint32_t size;
  nsAutoPtr<MovieBox> moov_box(new MovieBox(this));
  rv = moov_box->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = moov_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult
ISOCompositor::GenerateMoof(uint32_t aTrackType)
{
  mFragNum++;

  nsresult rv;
  uint32_t size;
  uint64_t first_sample_offset = mOutputSize + mLastWrittenBoxPos;
  nsAutoPtr<MovieFragmentBox> moof_box(new MovieFragmentBox(aTrackType, this));
  nsAutoPtr<TrackBox> mdat_box(new TrackBox(aTrackType, this));

  rv = moof_box->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  first_sample_offset += size;
  rv = mdat_box->Generate(&size);
  NS_ENSURE_SUCCESS(rv, rv);
  first_sample_offset += mdat_box->FirstSampleOffsetInTrackBox();

  // correct offset info
  nsAutoPtr<TrackFragmentHeaderBox> tfhd;
  rv = moof_box->Find(NS_LITERAL_CSTRING("tfhd"), (MuxerOperation**)&tfhd);
  if (NS_SUCCEEDED(rv)) {
    tfhd->UpdateBaseDataOffset(first_sample_offset);
  }

  rv = moof_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mdat_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  if (HasAudioTrack()) {
    mAudioFragmentation->Flush();
  }
  if (HasVideoTrack()) {
    mVideoFragmentation->Flush();
  }

  return NS_OK;
}

uint32_t
ISOCompositor::GetTime()
{
  uint32_t now = PR_IntervalToSeconds(PR_IntervalNow());
  uint32_t mpeg4Time = now + (66 * 365 + 17) * (24 * 60 * 60);
  return mpeg4Time;
}

}
