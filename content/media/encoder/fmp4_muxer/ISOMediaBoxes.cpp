/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include "TrackMetadataBase.h"
#include "ISOMediaBoxes.h"
#include "ISOCompositor.h"
#include "EncodedFrameContainer.h"
#include "ISOTrackMetadata.h"
#include "MP4ESDS.h"

namespace mozilla {

#define WRITE_FULLBOX(_compositor, _size)       \
  BoxSizeChecker checker(_compositor, _size);   \
  FullBox::Write();

#define FOURCC(a,b,c,d) ( ((a)<<24) | ((b)<<16) | ((c)<<8) | (d) )

// 14496-12 6.2.2 'Data Types and fields'
const uint32_t iso_matrix[] = { 0x00010000, 0, 0, 0,
                                0x00010000, 0, 0, 0, 0x40000000 };

class BoxSizeChecker {
public:
  BoxSizeChecker(ISOCompositor* aCompositor, uint32_t aSize) {
    mCompositor = aCompositor;
    ori_size = mCompositor->GetBufPos();
    box_size = aSize;
  }
  ~BoxSizeChecker() {
    uint32_t cur_size = mCompositor->GetBufPos();
    if ((cur_size - ori_size) != box_size) {
      MOZ_ASSERT(false);
    }
    mCompositor->mLastWrittenBoxPos = mCompositor->mOutBuffer.Length();
  }
public:
  uint32_t ori_size;
  uint32_t box_size;
  ISOCompositor* mCompositor;
};

nsresult
ESDBox::Generate(uint32_t* aBoxSize)
{
  uint32_t box_size;
  es_descriptor->Generate(&box_size);
  size += box_size;
  *aBoxSize = size;
  return NS_OK;
}

nsresult
ESDBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  es_descriptor->Write();
  return NS_OK;
}

ESDBox::ESDBox(ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("esds"), 0, 0, aCompositor)
{
  es_descriptor = new ES_Descriptor(aCompositor);
}

nsresult
TrackBox::Generate(uint32_t* aBoxSize)
{
  Fragmentation* frag = mCompositor->GetFragment(mTrackType);
  mFirstSampleOffset = size;
  for (uint32_t i = 0; i < frag->GetCurrentAvailableSampleNumber(); i++) {
    nsRefPtr<EncodedFrame> frame;
    frag->GetFrame(i, getter_AddRefs(frame));
    size += frame->GetFrameData().Length();
  }
  *aBoxSize = size;
  return NS_OK;
}

nsresult
TrackBox::Write()
{
  BoxSizeChecker checker(mCompositor, size);
  Box::Write();
  Fragmentation* frag = mCompositor->GetFragment(mTrackType);
  for (uint32_t i = 0; i < frag->GetCurrentAvailableSampleNumber(); i++) {
    nsRefPtr<EncodedFrame> frame;
    frag->GetFrame(i, getter_AddRefs(frame));
    mCompositor->Write((uint8_t*)frame->GetFrameData().Elements(), frame->GetFrameData().Length());
  }
  return NS_OK;
}

TrackBox::TrackBox(uint32_t aTrackType, ISOCompositor* aCompositor)
  : Box(NS_LITERAL_CSTRING("mdat"), aCompositor)
  , mFirstSampleOffset(0)
  , mTrackType(aTrackType)
{
}

uint32_t
TrackRunBox::fillSampleTable()
{
  uint32_t table_size = 0;
  Fragmentation* frag = mCompositor->GetFragment(mTrackType);
  sample_info_table = new tbl[frag->GetCurrentAvailableSampleNumber()];
  for (uint32_t i = 0; i < frag->GetCurrentAvailableSampleNumber(); i++) {
    nsRefPtr<EncodedFrame> frame;
    frag->GetFrame(i, getter_AddRefs(frame));
    sample_info_table[i].sample_duration = 0;
    sample_info_table[i].sample_size = frame->GetFrameData().Length();
    table_size += sizeof(uint32_t);
    sample_info_table[i].sample_flags = 0;
    sample_info_table[i].sample_composition_time_offset = 0;
  }
  return table_size;
}

nsresult
TrackRunBox::Generate(uint32_t* aBoxSize)
{
  Fragmentation* frag = mCompositor->GetFragment(mTrackType);
  sample_count = frag->GetCurrentAvailableSampleNumber();
  size += sizeof(sample_count);
  size += fillSampleTable();

  *aBoxSize = size;

  return NS_OK;
}

nsresult
TrackRunBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(sample_count);
  for (uint32_t i = 0; i < sample_count; i++) {
    mCompositor->Write(sample_info_table[i].sample_size);
  }

  return NS_OK;
}

TrackRunBox::TrackRunBox(uint32_t aType, ISOCompositor* aCompositor)
  // TODO: tf_flags, we may need to customize it from caller
  : FullBox(NS_LITERAL_CSTRING("trun"), 0, flags_sample_size_present, aCompositor)
  , sample_count(0)
  , data_offset(0)
  , first_sample_flags(0)
  , sample_info_table(nullptr)
  , mTrackType(aType)
{
}

TrackRunBox::~TrackRunBox()
{
  if (sample_info_table) {
    delete[] sample_info_table;
  }
}

nsresult
TrackFragmentHeaderBox::UpdateBaseDataOffset(uint64_t aOffset)
{
  base_data_offset = aOffset;
  return NS_OK;
}

nsresult
TrackFragmentHeaderBox::Generate(uint32_t* aBoxSize)
{
  track_ID = mCompositor->GetTrackID(mTrackType);
  size += sizeof(track_ID);

  if (flags.to_ulong() | base_data_offset_present) {
    // base_data_offset needs to add size of 'trun', 'tfhd' and
    // header of 'mdat 'later.
    base_data_offset = 0;
    size += sizeof(base_data_offset);
  }
  if (flags.to_ulong() | default_sample_duration_present) {
    default_sample_duration = mMeta.mAudMeta->FrameDuration;
    size += sizeof(default_sample_duration);
  }
  *aBoxSize = size;
  return NS_OK;
}

nsresult
TrackFragmentHeaderBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(track_ID);
  if (flags.to_ulong() | base_data_offset_present) {
    mCompositor->Write(base_data_offset);
  }
  if (flags.to_ulong() | default_sample_duration_present) {
    mCompositor->Write(default_sample_duration);
  }
  return NS_OK;
}

TrackFragmentHeaderBox::TrackFragmentHeaderBox(uint32_t aType,
                                               ISOCompositor* aCompositor)
  // TODO: tf_flags, we may need to customize it from caller
  : FullBox(NS_LITERAL_CSTRING("tfhd"),
            0,
            base_data_offset_present | default_sample_duration_present,
            aCompositor)
  , track_ID(0)
  , base_data_offset(0)
  , default_sample_duration(0)
{
    mTrackType = aType;
    mMeta.Init(mCompositor);
}

TrackFragmentBox::TrackFragmentBox(uint32_t aType, ISOCompositor* aCompositor)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("traf"), aCompositor)
{
  boxes.AppendElement(new TrackFragmentHeaderBox(aType, aCompositor));
  boxes.AppendElement(new TrackRunBox(aType, aCompositor));
}

nsresult
MovieFragmentHeaderBox::Generate(uint32_t* aBoxSize)
{
  sequence_number = mCompositor->GetCurFragmentNumber();
  size += sizeof(sequence_number);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
MovieFragmentHeaderBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(sequence_number);
  return NS_OK;
}

MovieFragmentHeaderBox::MovieFragmentHeaderBox(ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("mfhd"), 0, 0, aCompositor)
  , sequence_number(0)
{}

MovieFragmentBox::MovieFragmentBox(uint32_t aType, ISOCompositor* aCompositor)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("moof"), aCompositor)
{
  boxes.AppendElement(new MovieFragmentHeaderBox(aCompositor));
  boxes.AppendElement(new TrackFragmentBox(aType, aCompositor));
}

nsresult
TrackExtendsBox::Generate(uint32_t* aBoxSize)
{
  track_ID = mCompositor->GetTrackID(mTrackType);

  if (mTrackType == Audio_Track) {
    default_sample_description_index = 1;
    default_sample_duration = mMeta.mAudMeta->FrameDuration;
    default_sample_size = mMeta.mAudMeta->FrameSize;
    default_sample_flags = 0;
  } else if (mTrackType == Video_Track) {
    default_sample_description_index = 1;
    default_sample_duration =
      mMeta.mVidMeta->VideoFrequence / mMeta.mVidMeta->FrameRate;
    default_sample_size = 0;
    default_sample_flags = 0;
  } else {
    MOZ_ASSERT(0);
    return NS_ERROR_FAILURE;
  }

  size += sizeof(track_ID) +
          sizeof(default_sample_description_index) +
          sizeof(default_sample_duration) +
          sizeof(default_sample_size) +
          sizeof(default_sample_flags);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
TrackExtendsBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(track_ID);
  mCompositor->Write(default_sample_description_index);
  mCompositor->Write(default_sample_duration);
  mCompositor->Write(default_sample_size);
  mCompositor->Write(default_sample_flags);

  return NS_OK;
}

TrackExtendsBox::TrackExtendsBox(uint32_t aType, ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("trex"), 0, 0, aCompositor)
  , track_ID(0)
  , default_sample_description_index(0)
  , default_sample_duration(0)
  , default_sample_size(0)
  , default_sample_flags(0)
  , mTrackType(aType)
{
  mMeta.Init(aCompositor);
}

MovieExtendsBox::MovieExtendsBox(ISOCompositor* aCompositor)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("mvex"), aCompositor)
{
  mMeta.Init(aCompositor);
  if (mMeta.mAudMeta) {
    boxes.AppendElement(new TrackExtendsBox(Audio_Track, aCompositor));
  }
  if (mMeta.mVidMeta) {
    boxes.AppendElement(new TrackExtendsBox(Video_Track, aCompositor));
  }
}

nsresult
ChunkOffsetBox::Generate(uint32_t* aBoxSize)
{
  // fragmented, we don't need time to sample table
  entry_count = 0;
  size += sizeof(entry_count);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
ChunkOffsetBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(entry_count);
  return NS_OK;
}

ChunkOffsetBox::ChunkOffsetBox(uint32_t aType, ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("stco"), 0, 0, aCompositor)
  , entry_count(0)
  , sample_tbl(nullptr)
{}

nsresult
SampleToChunkBox::Generate(uint32_t* aBoxSize)
{
  // fragmented, we don't need time to sample table
  entry_count = 0;
  size += sizeof(entry_count);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
SampleToChunkBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(entry_count);
  return NS_OK;
}

SampleToChunkBox::SampleToChunkBox(uint32_t aType, ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("stsc"), 0, 0, aCompositor)
  , entry_count(0)
  , sample_tbl(nullptr)
{
}

nsresult
TimeToSampleBox::Generate(uint32_t* aBoxSize)
{
  // fragmented, we don't need time to sample table
  entry_count = 0;
  size += sizeof(entry_count);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
TimeToSampleBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(entry_count);
  return NS_OK;
}

TimeToSampleBox::TimeToSampleBox(uint32_t aType, ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("stts"), 0, 0, aCompositor)
  , entry_count(0)
  , sample_tbl(nullptr)
{}

nsresult
AVCSampleEntry::Generate(uint32_t* aBoxSize)
{
  // both fields occupy 16 bits defined in 14496-2 6.2.3.
  width = mMeta.mVidMeta->Width << 16;
  height = mMeta.mVidMeta->Height << 16;

  size += sizeof(reserved) +
          sizeof(width) +
          sizeof(height) +
          sizeof(reserved2) +
          sizeof(compressorName) +
          sizeof(reserved3);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
AVCSampleEntry::Write()
{
  BoxSizeChecker checker(mCompositor, size);
  SampleEntryBox::Write();

  mCompositor->Write(reserved, sizeof(reserved));
  mCompositor->Write(width);
  mCompositor->Write(height);
  mCompositor->Write(reserved2, sizeof(reserved2));
  mCompositor->Write(compressorName, sizeof(compressorName));
  mCompositor->Write(reserved3, sizeof(reserved3));
  return NS_OK;
}

AVCSampleEntry::AVCSampleEntry(ISOCompositor* aCompositor)
  : SampleEntryBox(NS_LITERAL_CSTRING("mp4v"), Video_Track, aCompositor)
  , width(0)
  , height(0)
{
  mMeta.Init(mCompositor);
  memset(reserved, 0 , sizeof(reserved));
  memset(reserved2, 0 , sizeof(reserved2));
  memset(compressorName, 0 , sizeof(compressorName));
  memset(reserved3, 0 , sizeof(reserved3));
}

nsresult
MP4AudioSampleEntry::Generate(uint32_t* aBoxSize)
{
  sound_version = 0;
  // step reserved2
  sample_size = 16;
  channels = mMeta.mAudMeta->Channels;
  compressionId = 0;
  packet_size = 0;
  timeScale = mMeta.mAudMeta->SampleRate;

  size += sizeof(sound_version) +
          sizeof(reserved2) +
          sizeof(sample_size) +
          sizeof(channels) +
          sizeof(packet_size) +
          sizeof(compressionId) +
          sizeof(timeScale);

  uint32_t box_size;
  nsresult rv = es->Generate(&box_size);
  NS_ENSURE_SUCCESS(rv, rv);
  size += box_size;

  *aBoxSize = size;
  return NS_OK;
}

nsresult
MP4AudioSampleEntry::Write()
{
  BoxSizeChecker checker(mCompositor, size);
  SampleEntryBox::Write();
  mCompositor->Write(sound_version);
  mCompositor->Write(reserved2, sizeof(reserved2));
  mCompositor->Write(channels);
  mCompositor->Write(sample_size);
  mCompositor->Write(compressionId);
  mCompositor->Write(packet_size);
  mCompositor->Write(timeScale);

  nsresult rv = es->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

MP4AudioSampleEntry::MP4AudioSampleEntry(ISOCompositor* aCompositor)
  : SampleEntryBox(NS_LITERAL_CSTRING("mp4a"), Audio_Track, aCompositor)
  , sound_version(0)
  , channels(2)
  , sample_size(16)
  , compressionId(0)
  , packet_size(0)
  , timeScale(0)
{
  es = new ESDBox(aCompositor);
  mMeta.Init(mCompositor);
  memset(reserved2, 0 , sizeof(reserved2));
}

nsresult
SampleDescriptionBox::Generate(uint32_t* aBoxSize)
{
  entry_count = 1;
  size += sizeof(entry_count);

  nsresult rv;
  uint32_t box_size;
  rv = sample_entry_box->Generate(&box_size);
  NS_ENSURE_SUCCESS(rv, rv);
  size += box_size;
  *aBoxSize = size;

  return NS_OK;
}

nsresult
SampleDescriptionBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  nsresult rv;
  mCompositor->Write(entry_count);
  rv = sample_entry_box->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

SampleDescriptionBox::SampleDescriptionBox(uint32_t aType, ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("stsd"), 0, 0, aCompositor)
  , entry_count(0)
{
  mTrackType = aType;

  switch (mTrackType) {
  case Audio_Track:
    {
      sample_entry_box = new MP4AudioSampleEntry(aCompositor);
    } break;
  case Video_Track:
    {
      sample_entry_box = new AVCSampleEntry(aCompositor);
    } break;
  }
}

nsresult
SampleSizeBox::Generate(uint32_t* aBoxSize)
{
  size += sizeof(sample_size) +
          sizeof(sample_count);
  *aBoxSize = size;
  return NS_OK;
}

nsresult
SampleSizeBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(sample_size);
  mCompositor->Write(sample_count);
  return NS_OK;
}

SampleSizeBox::SampleSizeBox(ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("stsz"), 0, 0, aCompositor)
  , sample_size(0)
  , sample_count(0)
{
}

SampleTableBox::SampleTableBox(uint32_t aType, ISOCompositor* aCompositor)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("stbl"), aCompositor)
{
  boxes.AppendElement(new SampleDescriptionBox(aType, aCompositor));
  boxes.AppendElement(new TimeToSampleBox(aType, aCompositor));
  boxes.AppendElement(new SampleToChunkBox(aType, aCompositor));
  boxes.AppendElement(new SampleSizeBox(aCompositor));
  boxes.AppendElement(new ChunkOffsetBox(aType, aCompositor));
}

nsresult
DataEntryUrlBox::Generate(uint32_t* aBoxSize)
{
  // location is null here, do nothing
  size += location.Length();
  *aBoxSize = size;

  return NS_OK;
}

nsresult
DataEntryUrlBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  return NS_OK;
}

DataEntryUrlBox::DataEntryUrlBox()
  : FullBox(NS_LITERAL_CSTRING("url "), 0, 0, (ISOCompositor*) nullptr)
{}

DataEntryUrlBox::DataEntryUrlBox(ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("url "), 0, flags_media_at_the_same_file, aCompositor)
{}

DataEntryUrlBox::DataEntryUrlBox(const DataEntryUrlBox& aBox)
  : FullBox(aBox.boxType, aBox.version, aBox.flags.to_ulong(), aBox.mCompositor)
{
  location = aBox.location;
}

nsresult DataReferenceBox::Generate(uint32_t* aBoxSize)
{
  entry_count = 1;  // only allow on entry here
  size += sizeof(uint32_t);

  for (uint32_t i = 0; i < entry_count; i++) {
    uint32_t box_size = 0;
    DataEntryUrlBox* url = new DataEntryUrlBox(mCompositor);
    url->Generate(&box_size);
    size += box_size;
    urls.AppendElement(url);
  }

  *aBoxSize = size;

  return NS_OK;
}

nsresult DataReferenceBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(entry_count);

  for (uint32_t i = 0; i < entry_count; i++) {
    urls[i]->Write();
  }

  return NS_OK;
}

DataReferenceBox::DataReferenceBox(ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("dref"), 0, 0, aCompositor)
  , entry_count(0)
{}

DataInformationBox::DataInformationBox(ISOCompositor* aCompositor)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("dinf"), aCompositor)
{
  boxes.AppendElement(new DataReferenceBox(aCompositor));
}

nsresult
VideoMediaHeaderBox::Generate(uint32_t* aBoxSize)
{
  size += sizeof(graphicsmode) +
          sizeof(opcolor);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
VideoMediaHeaderBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(graphicsmode);
  mCompositor->WriteArray(opcolor, 3);
  return NS_OK;
}

VideoMediaHeaderBox::VideoMediaHeaderBox(ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("vmhd"), 0, 1, aCompositor)
  , graphicsmode(0)
{
  memset(opcolor, 0 , sizeof(opcolor));
}

nsresult
SoundMediaHeaderBox::Generate(uint32_t* aBoxSize)
{
  balance = 0;
  reserved = 0;
  size += sizeof(balance) +
          sizeof(reserved);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
SoundMediaHeaderBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(balance);
  mCompositor->Write(reserved);

  return NS_OK;
}

SoundMediaHeaderBox::SoundMediaHeaderBox(ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("smhd"), 0, 0, aCompositor)
{
}

MediaInformationBox::MediaInformationBox(uint32_t aType, ISOCompositor* aCompositor)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("minf"), aCompositor)
{
  mTrackType = aType;

  if (mTrackType == Audio_Track) {
    boxes.AppendElement(new SoundMediaHeaderBox(aCompositor));
  } else if (mTrackType == Video_Track) {
    boxes.AppendElement(new VideoMediaHeaderBox(aCompositor));
  } else {
    MOZ_ASSERT(0);
  }

  boxes.AppendElement(new DataInformationBox(aCompositor));
  boxes.AppendElement(new SampleTableBox(aType, aCompositor));
}

nsresult
HandlerBox::Generate(uint32_t* aBoxSize)
{
  pre_defined = 0;
  if (mTrackType == Audio_Track) {
    handler_type = FOURCC('s', 'o', 'u', 'n');
  } else if (mTrackType == Video_Track) {
    handler_type = FOURCC('v', 'i', 'd', 'e');
  }

  size += sizeof(pre_defined) +
          sizeof(handler_type) +
          sizeof(reserved);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
HandlerBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(pre_defined);
  mCompositor->Write(handler_type);
  mCompositor->WriteArray(reserved, 3);

  return NS_OK;
}

HandlerBox::HandlerBox(uint32_t aType, ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("hdlr"), 0, 0, aCompositor)
  , pre_defined(0)
  , handler_type(0)
{
  mTrackType = aType;
  memset(reserved, 0 , sizeof(reserved));
}

MediaHeaderBox::MediaHeaderBox(uint32_t aType, ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("mdhd"), 0, 0, aCompositor)
  , creation_time(0)
  , modification_time(0)
  , timescale(0)
  , duration(0)
  , pad(0)
  , lang1(0)
  , lang2(0)
  , lang3(0)
  , pre_defined(0)
{
  mTrackType = aType;
  mMeta.Init(aCompositor);
}

uint32_t
MediaHeaderBox::GetTimeScale()
{
  if (mTrackType == Audio_Track) {
    return mMeta.mAudMeta->SampleRate;
  }

  return mMeta.mVidMeta->VideoFrequence;
}

nsresult
MediaHeaderBox::Generate(uint32_t* aBoxSize)
{
  creation_time = mCompositor->GetTime();
  modification_time = mCompositor->GetTime();
  timescale = GetTimeScale();
  duration = 0; // fragmented mp4

  pad = 0;
  lang1 = 'u' - 0x60; // "und" underdetermined language
  lang2 = 'n' - 0x60;
  lang3 = 'd' - 0x60;
  size += (pad.size() + lang1.size() + lang2.size() + lang3.size()) / CHAR_BIT;

  pre_defined = 0;
  size += sizeof(creation_time) +
          sizeof(modification_time) +
          sizeof(timescale) +
          sizeof(duration) +
          sizeof(pre_defined);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
MediaHeaderBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(creation_time);
  mCompositor->Write(modification_time);
  mCompositor->Write(timescale);
  mCompositor->Write(duration);
  mCompositor->WriteBits(pad.to_ulong(), pad.size());
  mCompositor->WriteBits(lang1.to_ulong(), lang1.size());
  mCompositor->WriteBits(lang2.to_ulong(), lang2.size());
  mCompositor->WriteBits(lang3.to_ulong(), lang3.size());
  mCompositor->Write(pre_defined);

  return NS_OK;
}

MovieBox::MovieBox(ISOCompositor* aCompositor)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("moov"), aCompositor)
{
  boxes.AppendElement(new MovieHeaderBox(aCompositor));
  if (aCompositor->HasAudioTrack()) {
    boxes.AppendElement(new TrakBox(Audio_Track, aCompositor));
  }
  if (aCompositor->HasVideoTrack()) {
    boxes.AppendElement(new TrakBox(Video_Track, aCompositor));
  }
  boxes.AppendElement(new MovieExtendsBox(aCompositor));
}

nsresult
MovieHeaderBox::Generate(uint32_t* aBoxSize)
{
  creation_time = mCompositor->GetTime();
  modification_time = mCompositor->GetTime();
  timescale = GetTimeScale();
  duration = 0;     // The duration is always 0 in fragmented mp4.
  next_track_ID = mCompositor->GetNextTrackID();

  size += sizeof(next_track_ID) +
          sizeof(creation_time) +
          sizeof(modification_time) +
          sizeof(timescale) +
          sizeof(duration) +
          sizeof(rate) +
          sizeof(volume) +
          sizeof(reserved16) +
          sizeof(reserved32) +
          sizeof(matrix) +
          sizeof(pre_defined);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
MovieHeaderBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(creation_time);
  mCompositor->Write(modification_time);
  mCompositor->Write(timescale);
  mCompositor->Write(duration);
  mCompositor->Write(rate);
  mCompositor->Write(volume);
  mCompositor->Write(reserved16);
  mCompositor->WriteArray(reserved32, 2);
  mCompositor->WriteArray(matrix, 9);
  mCompositor->WriteArray(pre_defined, 6);
  mCompositor->Write(next_track_ID);

  return NS_OK;
}

uint32_t
MovieHeaderBox::GetTimeScale()
{
  if (mMeta.AudioOnly()) {
    return mMeta.mAudMeta->SampleRate;
  }

  // return video rate
  return mMeta.mVidMeta->VideoFrequence;
}

MovieHeaderBox::MovieHeaderBox(ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("mvhd"), 0, 0, aCompositor)
  , creation_time(0)
  , modification_time(0)
  , timescale(90000)
  , duration(0)
  , rate(0x00010000)
  , volume(0x0100)
  , reserved16(0)
  , next_track_ID(1)
{
  mMeta.Init(aCompositor);
  memcpy(matrix, iso_matrix, sizeof(matrix));
  memset(reserved32, 0, sizeof(reserved32));
  memset(pre_defined, 0, sizeof(pre_defined));
}

TrackHeaderBox::TrackHeaderBox(uint32_t aType, ISOCompositor* aCompositor)
  : FullBox(NS_LITERAL_CSTRING("tkhd"), 0,
            flags_track_enabled | flags_track_in_movie | flags_track_in_preview,
            aCompositor)
  , creation_time(0)
  , modification_time(0)
  , track_ID(0)
  , reserved(0)
  , duration(0)
  , layer(0)
  , alternate_group(0)
  , volume(0)
  , reserved3(0)
  , width(0)
  , height(0)
{
  mTrackType = aType;
  mMeta.Init(aCompositor);
  memcpy(matrix, iso_matrix, sizeof(matrix));
  memset(reserved2, 0, sizeof(reserved2));
}

nsresult
TrackHeaderBox::Generate(uint32_t* aBoxSize)
{
  creation_time = mCompositor->GetTime();
  modification_time = mCompositor->GetTime();
  track_ID = (mTrackType == Audio_Track ? mCompositor->GetTrackID(Audio_Track)
                                        : mCompositor->GetTrackID(Video_Track));
  // fragmented mp4
  duration = 0;

  // volume, audiotrack is always 0x0100 in 14496-12 8.3.2.2
  volume = (mTrackType == Audio_Track ? 0x0100 : 0);

  if (mTrackType == Video_Track) {
    width = mMeta.mVidMeta->Width;
    height = mMeta.mVidMeta->Height;
  }

  size += sizeof(creation_time) +
          sizeof(modification_time) +
          sizeof(track_ID) +
          sizeof(reserved) +
          sizeof(duration) +
          sizeof(reserved2) +
          sizeof(layer) +
          sizeof(alternate_group) +
          sizeof(volume) +
          sizeof(reserved3) +
          sizeof(matrix) +
          sizeof(width) +
          sizeof(height);

  *aBoxSize = size;

  return NS_OK;
}

nsresult
TrackHeaderBox::Write()
{
  WRITE_FULLBOX(mCompositor, size)
  mCompositor->Write(creation_time);
  mCompositor->Write(modification_time);
  mCompositor->Write(track_ID);
  mCompositor->Write(reserved);
  mCompositor->Write(duration);
  mCompositor->WriteArray(reserved2, 2);
  mCompositor->Write(layer);
  mCompositor->Write(alternate_group);
  mCompositor->Write(volume);
  mCompositor->Write(reserved3);
  mCompositor->WriteArray(matrix, 9);
  mCompositor->Write(width);
  mCompositor->Write(height);

  return NS_OK;
}

nsresult
FileTypeBox::Generate(uint32_t* aBoxSize)
{
  if (!mCompositor->HasVideoTrack() && mCompositor->HasAudioTrack()) {
    major_brand = "M4A ";
  } else {
    major_brand = "MP42";
  }
  minor_version = 0;
  compatible_brands.AppendElement("isom");
  compatible_brands.AppendElement("mp42");

  size += major_brand.Length() +
          sizeof(minor_version) +
          compatible_brands.Length() * 4;

  *aBoxSize = size;

  return NS_OK;
}

nsresult
FileTypeBox::Write()
{
  BoxSizeChecker checker(mCompositor, size);
  Box::Write();
  mCompositor->WriteFourCC(major_brand.get());
  mCompositor->Write(minor_version);
  uint32_t len = compatible_brands.Length();
  for (uint32_t i = 0; i < len; i++) {
    mCompositor->WriteFourCC(compatible_brands[i].get());
  }

  return NS_OK;
}

FileTypeBox::FileTypeBox(ISOCompositor* aCompositor)
  : Box(NS_LITERAL_CSTRING("ftyp"), aCompositor)
  , minor_version(0)
{
}

MediaBox::MediaBox(uint32_t aType, ISOCompositor* aCompositor)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("mdia"), aCompositor)
{
  mTrackType = aType;
  boxes.AppendElement(new MediaHeaderBox(aType, aCompositor));
  boxes.AppendElement(new HandlerBox(aType, aCompositor));
  boxes.AppendElement(new MediaInformationBox(aType, aCompositor));
}

nsresult
DefaultContainerImpl::Generate(uint32_t* aBoxSize)
{
  nsresult rv;
  uint32_t box_size;
  uint32_t len = boxes.Length();
  for (uint32_t i = 0; i < len; i++) {
    rv = boxes.ElementAt(i)->Generate(&box_size);
    NS_ENSURE_SUCCESS(rv, rv);
    size += box_size;
  }
  *aBoxSize = size;
  return NS_OK;
}

nsresult
DefaultContainerImpl::Find(const nsACString& aType, MuxerOperation** aOperation)
{
  nsresult rv = Box::Find(aType, aOperation);
  if (NS_SUCCEEDED(rv)) {
    return NS_OK;
  }

  uint32_t len = boxes.Length();
  for (uint32_t i = 0; i < len; i++) {
    rv = boxes.ElementAt(i)->Find(aType, aOperation);
    if (NS_SUCCEEDED(rv)) {
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult
DefaultContainerImpl::Write()
{
  BoxSizeChecker checker(mCompositor, size);
  Box::Write();

  nsresult rv;
  uint32_t len = boxes.Length();
  for (uint32_t i = 0; i < len; i++) {
    rv = boxes.ElementAt(i)->Write();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

DefaultContainerImpl::DefaultContainerImpl(const nsACString& aType,
                                           ISOCompositor* aCompositor)
  : Box(aType, aCompositor)
{
}

nsresult
Box::MetaHelper::Init(ISOCompositor* aCompositor)
{
  aCompositor->GetAudioMetadata(getter_AddRefs(mAudMeta));
  aCompositor->GetVideoMetadata(getter_AddRefs(mVidMeta));
  return NS_OK;
}

nsresult
Box::Write()
{
  mCompositor->Write(size);
  mCompositor->WriteFourCC(boxType.get());
  return NS_OK;
}

nsresult
Box::Find(const nsACString& aType, MuxerOperation** aOperation)
{
  if (boxType == aType) {
    NS_ENSURE_ARG_POINTER(aOperation);
    NS_IF_ADDREF(*aOperation = this);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

Box::Box() : size(8), mCompositor(nullptr)
{}

Box::Box(const nsACString& aType, ISOCompositor* aCompositor)
  : size(8), mCompositor(aCompositor)
{
  MOZ_ASSERT(aType.Length() == 4);
  boxType = aType;
}

FullBox::FullBox() : Box(), version(0), flags(0)
{
  size += sizeof(version);
  size += flags.size() / CHAR_BIT;
}

FullBox::FullBox(const nsACString& aType, uint8_t aVersion, uint32_t aFlags,
                 ISOCompositor* aCompositor)
  : Box(aType, aCompositor)
{
  std::bitset<24> tmp_flags(aFlags);
  version = aVersion;
  flags = tmp_flags;
  size += sizeof(version) + flags.size() / CHAR_BIT;
}

nsresult
FullBox::Write()
{
  Box::Write();
  mCompositor->Write(version);
  mCompositor->WriteBits(flags.to_ulong(), flags.size());
  return NS_OK;
}

TrakBox::TrakBox(uint32_t aTrackType, ISOCompositor* aCompositor)
  : DefaultContainerImpl(NS_LITERAL_CSTRING("trak"), aCompositor)
{
  boxes.AppendElement(new TrackHeaderBox(aTrackType, aCompositor));
  boxes.AppendElement(new MediaBox(aTrackType, aCompositor));
}

SampleEntryBox::SampleEntryBox(const nsACString& aFormat, uint32_t aTrackType,
                               ISOCompositor* aCompositor)
  : Box(aFormat, aCompositor)
  , data_reference_index(0)
  , mTrackType(aTrackType)
{
  data_reference_index = aCompositor->GetTrackID(mTrackType);
  size += sizeof(reserved) +
          sizeof(data_reference_index);
  mMeta.Init(aCompositor);
  memset(reserved, 0, sizeof(reserved));
}

nsresult
SampleEntryBox::Write()
{
  Box::Write();
  mCompositor->Write(reserved, sizeof(reserved));
  mCompositor->Write(data_reference_index);
  return NS_OK;
}

}
