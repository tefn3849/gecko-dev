/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ISOMediaBoxes_h_
#define ISOMediaBoxes_h_

#include <bitset>
#include "nsString.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "MuxerOperation.h"

namespace mozilla {

class ISOCompositor;
class AACTrackMetadata;
class AVCTrackMetadata;
class ES_Descriptor;

// This is the base class for all ISO media format boxes.
// It provides the fields of box type(four CC) and size.
class Box : public MuxerOperation {
public:
  // 14496-12 4-2 'Object Structure'.
  uint32_t size;     // size of this box.
  nsCString boxType; // four CC name, all table names are listed in
                     // 14496-12 table 1.

  // MuxerOperation
  // Write size and boxType to muxing output stream.
  nsresult Write() MOZ_OVERRIDE;
  // Check if this box is the one we find.
  nsresult Find(const nsACString& aType, MuxerOperation** aOperation) MOZ_OVERRIDE;

  // A helper class to check box wirten bytes number; it will compare
  // the size generated from Box::Generate() and the actually written length in
  // Box::Write().
  class MetaHelper {
  public:
    nsresult Init(ISOCompositor* aCompositor);
    bool AudioOnly() {
      if (mAudMeta && !mVidMeta) {
        return true;
      }
      return false;
    }
    nsRefPtr<AACTrackMetadata> mAudMeta;
    nsRefPtr<AVCTrackMetadata> mVidMeta;
  };

protected:
  Box();
  Box(const nsACString& aType, ISOCompositor* aCompositor);

protected:
  ISOCompositor* mCompositor;
};

class FullBox : public Box {
public:
  // 14496-12 4.2 'Object Structure'
  uint8_t version;
  std::bitset<24> flags;

  nsresult Write() MOZ_OVERRIDE;

protected:
  FullBox();
  FullBox(const nsACString& aType, uint8_t aVersion, uint32_t aFlags,
          ISOCompositor* aCompositor);
};

// The default implementation of the container box.
// Basically, the container box inherits this class and overrides the
// constructor only.
class DefaultContainerImpl : public Box {
public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;
  nsresult Find(const nsACString& aType, MuxerOperation** aOperation) MOZ_OVERRIDE;

protected:
  DefaultContainerImpl(const nsACString& aType, ISOCompositor* aCompositor);

protected:
  nsTArray<nsRefPtr<MuxerOperation> > boxes;
};

class ESDBox : public FullBox {
public:
  nsAutoPtr<ES_Descriptor> es_descriptor;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  ESDBox(ISOCompositor* aCompositor);
};

class FileTypeBox : public Box {
public:
  nsCString major_brand; // four chars
  uint32_t minor_version;
  nsTArray<nsCString> compatible_brands;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  FileTypeBox(ISOCompositor* aCompositor);
};

class SampleEntryBox : public Box {
public:
  uint8_t reserved[6];
  uint16_t data_reference_index;

public:
  nsresult Write() MOZ_OVERRIDE;

protected:
  SampleEntryBox(const nsACString& aFormat, uint32_t aTrackType,
                 ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
  MetaHelper mMeta;
};

class TrackBox : public Box {
public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;
  uint32_t FirstSampleOffsetInTrackBox() { return mFirstSampleOffset; }

  TrackBox(uint32_t aTrackType, ISOCompositor* aCompositor);

protected:
  uint32_t mFirstSampleOffset;
  uint32_t mTrackType;
};

// flags for TrackRunBox::flags, 14496-12 8.8.8.1.
#define flags_data_offset_present                     0x000001
#define flags_first_sample_flags_present              0x000002
#define flags_sample_duration_present                 0x000100
#define flags_sample_size_present                     0x000200
#define flags_sample_flags_present                    0x000400
#define flags_sample_composition_time_offsets_present 0x000800

class TrackRunBox : public FullBox {
public:
  typedef struct {
    uint32_t sample_duration;
    uint32_t sample_size;
    uint32_t sample_flags;
    uint32_t sample_composition_time_offset;
  } tbl;

  uint32_t sample_count;
  // the following are optional fields
  uint32_t data_offset;
  uint32_t first_sample_flags;
  tbl* sample_info_table;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  TrackRunBox(uint32_t aType, ISOCompositor* aCompositor);
  ~TrackRunBox();

protected:
  uint32_t fillSampleTable();

protected:
  uint32_t mTrackType;
};

// tf_flags in TrackFragmentHeaderBox, 14496-12 8.8.7.1.
#define base_data_offset_present         0x000001
#define sample_description_index_present 0x000002
#define default_sample_duration_present  0x000008
#define default_sample_size_present      0x000010
#define default_sample_flags_present     0x000020
#define duration_is_empty                0x010000
#define default_base_is_moof             0x020000

class TrackFragmentHeaderBox : public FullBox {
public:

  uint32_t track_ID;
  uint64_t base_data_offset;
  uint32_t default_sample_duration;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;
  // The offset of the first sample in file.
  nsresult UpdateBaseDataOffset(uint64_t aOffset);

  TrackFragmentHeaderBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
  MetaHelper mMeta;
};

// TrackFragmentBox cotains TrackFragmentHeaderBox and TrackRunBox.
class TrackFragmentBox : public DefaultContainerImpl {
public:
  TrackFragmentBox(uint32_t aType, ISOCompositor* aCompositor);
};

class MovieFragmentHeaderBox : public FullBox {
public:
  uint32_t sequence_number;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  MovieFragmentHeaderBox(ISOCompositor* aCompositor);
};

// MovieFragmentBox contains MovieFragmentHeaderBox and TrackFragmentBox.
class MovieFragmentBox : public DefaultContainerImpl {
public:
  MovieFragmentBox(uint32_t aType, ISOCompositor* aCompositor);
};

class TrackExtendsBox : public FullBox {
public:
  uint32_t track_ID;
  uint32_t default_sample_description_index;
  uint32_t default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  TrackExtendsBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
  MetaHelper mMeta;
};

// MovieExtendsBox contains TrackExtendsBox.
class MovieExtendsBox : public DefaultContainerImpl {
public:
  MovieExtendsBox(ISOCompositor* aCompositor);
  MetaHelper mMeta;
};

class ChunkOffsetBox : public FullBox {
public:
  typedef struct {
    uint32_t chunk_offset;
  } tbl;

  uint32_t entry_count;
  tbl* sample_tbl;
public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  ChunkOffsetBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
};

class SampleToChunkBox : public FullBox {
public:
  typedef struct {
    uint32_t first_chunk;
    uint32_t sample_per_chunk;
    uint32_t sample_description_index;
  } tbl;

  uint32_t entry_count;
  tbl* sample_tbl;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  SampleToChunkBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
};

class TimeToSampleBox : public FullBox {
public:
  typedef struct {
    uint32_t sample_count;
    uint32_t sample_delta;
  } tbl;

  uint32_t entry_count;
  tbl* sample_tbl;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  TimeToSampleBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
};

class AVCConfigurationBox : public Box {
public:
  // avcConfig is CodecSpecificData from 14496-15 '5.3.4.1 Sample description
  // name and format'.
  // These data are generated from encoder itself and we encapusulated the generated
  // bitstream into box directly.
  nsTArray<uint8_t> avcConfig;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  AVCConfigurationBox(ISOCompositor* aCompositor);
};

class AVCSampleEntry : public SampleEntryBox {
public:
  uint8_t reserved[16];
  uint16_t width;
  uint16_t height;
  uint8_t reserved2[14];
  uint8_t compressorName[32];
  uint8_t reserved3[4];

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  AVCSampleEntry(ISOCompositor* aCompositor);

protected:
  MetaHelper mMeta;
};

class MP4AudioSampleEntry : public SampleEntryBox {
public:
  uint16_t sound_version;
  uint8_t reserved2[6];
  uint16_t channels;
  uint16_t sample_size;
  uint16_t compressionId;
  uint16_t packet_size;
  uint32_t timeScale;
  nsAutoPtr<ESDBox> es;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  MP4AudioSampleEntry(ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
  MetaHelper mMeta;
};

class SampleDescriptionBox : public FullBox {
public:
  uint32_t entry_count;
  nsRefPtr<SampleEntryBox> sample_entry_box;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  SampleDescriptionBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
};

class SampleSizeBox : public FullBox {
public:
  uint32_t sample_size;
  uint32_t sample_count;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  SampleSizeBox(ISOCompositor* aCompositor);
};

// SampleTableBox contains SampleDescriptionBox,
//                         TimeToSampleBox,
//                         SampleToChunkBox,
//                         SampleSizeBox and
//                         ChunkOffsetBox.
class SampleTableBox : public DefaultContainerImpl {
public:
  SampleTableBox(uint32_t aType, ISOCompositor* aCompositor);
};

class DataEntryUrlBox : public FullBox {
public:
  // flags in DataEntryUrlBox::flags
  const static uint16_t flags_media_at_the_same_file = 0x0001;

  nsCString location;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  DataEntryUrlBox();
  DataEntryUrlBox(ISOCompositor* aCompositor);
  DataEntryUrlBox(const DataEntryUrlBox& aBox);
};

class DataReferenceBox : public FullBox {
public:
  uint32_t entry_count;
  nsTArray<nsAutoPtr<DataEntryUrlBox> > urls;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  DataReferenceBox(ISOCompositor* aCompositor);
};

// DataInformationBox contains DataReferenceBox.
class DataInformationBox : public DefaultContainerImpl {
public:
  DataInformationBox(ISOCompositor* aCompositor);
};

class VideoMediaHeaderBox : public FullBox {
public:
  uint16_t graphicsmode;
  uint16_t opcolor[3];

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  VideoMediaHeaderBox(ISOCompositor* aCompositor);
};

class SoundMediaHeaderBox : public FullBox {
public:
  uint16_t balance;
  uint16_t reserved;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  SoundMediaHeaderBox(ISOCompositor* aCompositor);
};

// MediaInformationBox contains SoundMediaHeaderBox, DataInformationBox and
// SampleTableBox.
class MediaInformationBox : public DefaultContainerImpl {
public:
  MediaInformationBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
};

// flags for TrackHeaderBox::flags.
#define flags_track_enabled    0x000001
#define flags_track_in_movie   0x000002
#define flags_track_in_preview 0x000004

class TrackHeaderBox : public FullBox {
public:
  // version = 0
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t track_ID;
  uint32_t reserved;
  uint32_t duration;

  uint32_t reserved2[2];
  uint16_t layer;
  uint16_t alternate_group;
  uint16_t volume;
  uint16_t reserved3;
  uint32_t matrix[9];
  uint32_t width;
  uint32_t height;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  TrackHeaderBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
  MetaHelper mMeta;
};

class HandlerBox : public FullBox {
public:
  uint32_t pre_defined;
  uint32_t handler_type;
  uint32_t reserved[3];
  nsCString name;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  HandlerBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
};

class MediaHeaderBox : public FullBox {
public:
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t timescale;
  uint32_t duration;
  std::bitset<1> pad;
  std::bitset<5> lang1;
  std::bitset<5> lang2;
  std::bitset<5> lang3;
  uint16_t pre_defined;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  MediaHeaderBox(uint32_t aType, ISOCompositor* aCompositor);
  uint32_t GetTimeScale();

protected:
  uint32_t mTrackType;
  MetaHelper mMeta;
};

// MediaBox contains MediaHeaderBox, HandlerBox, and MediaInformationBox.
class MediaBox : public DefaultContainerImpl {
public:
  MediaBox(uint32_t aType, ISOCompositor* aCompositor);

protected:
  uint32_t mTrackType;
};

// TrakBox contains TrackHeaderBox and MediaBox.
class TrakBox : public DefaultContainerImpl {
public:
  TrakBox(uint32_t aTrackType, ISOCompositor* aCompositor);
};

class MovieHeaderBox : public FullBox {
public:
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t timescale;
  uint32_t duration;
  uint32_t rate;
  uint16_t volume;
  uint16_t reserved16;
  uint32_t reserved32[2];
  uint32_t matrix[9];
  uint32_t pre_defined[6];
  uint32_t next_track_ID;

public:
  nsresult Generate(uint32_t* aBoxSize) MOZ_OVERRIDE;
  nsresult Write() MOZ_OVERRIDE;

  MovieHeaderBox(ISOCompositor* aCompositor);
  uint32_t GetTimeScale();

protected:
  MetaHelper mMeta;
};

// MovieBox contains MovieHeaderBox, TrakBox and MovieExtendsBox.
class MovieBox : public DefaultContainerImpl {
public:
  MovieBox(ISOCompositor* aCompositor);
};

}
#endif // ISOMediaBoxes_h_
