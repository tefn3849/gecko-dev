/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ISOMediaWriter_h_
#define ISOMediaWriter_h_

#include "ContainerWriter.h"

namespace mozilla {

class ISOCompositor;
class Fragmentation;
class AACTrackMetadata;
class AVCTrackMetadata;

class ISOMediaWriter : public ContainerWriter
{
public:
  nsresult WriteEncodedTrack(const EncodedFrameContainer &aData,
                             uint32_t aFlags = 0) MOZ_OVERRIDE;

  nsresult GetContainerData(nsTArray<nsTArray<uint8_t> >* aOutputBufs,
                            uint32_t aFlags = 0) MOZ_OVERRIDE;

  nsresult SetMetadata(TrackMetadataBase* aMetadata) MOZ_OVERRIDE;

  bool IsWritingComplete() MOZ_OVERRIDE;

  ISOMediaWriter(uint32_t aType);

protected:
  /**
   * The state of each state will generate one or more blob.
   * Each blob will be a moov, moof, or a mvex box.
   * The generated sequence is:
   *
   *   moov -> moof -> moof -> ... -> moof -> mfra
   *
   * Following is the details of each state.
   *   MUXING_HEAD:
   *     It collects the metadata to generate a moov. The state transits to
   *     MUXING_HEAD after output moov blob.
   *
   *   MUXING_FRAG:
   *     It collects enough audio/video data to generate a fragement blob. This
   *     will be repeated until END_OF_STREAM and then transiting to MUXING_DONE.
   *
   *   MUXING_MFRA:
   *     It generates the mfra box, the final box.
   *
   *   MUXING_DONE:
   *     End of ISOMediaWriter life cycle.
   */
  enum MuxState {
    MUXING_HEAD,
    MUXING_FRAG,
    MUXING_MFRA,
    MUXING_DONE,
  };

private:
  nsresult RunState(uint32_t aTrackType = 0);

  nsAutoPtr<ISOCompositor> mCompositor;
  nsAutoPtr<Fragmentation> mAudioFragmentation;
  nsAutoPtr<Fragmentation> mVideoFragmentation;
  MuxState mState;
  bool mBlobReady;
  uint32_t mType; // Combination of HAS_AUDIO or HAS_VIDEO.
};
}
#endif
