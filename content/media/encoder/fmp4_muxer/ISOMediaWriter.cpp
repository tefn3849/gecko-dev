/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ISOMediaWriter.h"
#include "ISOCompositor.h"
#include "ISOMediaBoxes.h"
#include "ISOTrackMetadata.h"

#undef LOG
#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "MediaEncoder", ## args);
#else
#define LOG(args, ...)
#endif

#include <cstdio>
static FILE* debugOut = nullptr;

namespace mozilla {

const static uint32_t FRAG_DURATION = 10;

ISOMediaWriter::ISOMediaWriter(uint32_t aType)
  : ContainerWriter()
  , mState(MUXING_HEAD)
  , mBlobReady(false)
  , mType(aType)
{
  mCompositor = new ISOCompositor();
  mCompositor->SetFragmentDuration(FRAG_DURATION);
}

nsresult
ISOMediaWriter::RunState(uint32_t aTrackType)
{
  nsresult rv;
  switch (mState) {
    case MUXING_HEAD:
    {
      if (!debugOut) {
        debugOut = fopen("/data/local/tmp/fmp4.mp4", "w+");
      }
      rv = mCompositor->GenerateFtyp();
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mCompositor->GenerateMoov();
      NS_ENSURE_SUCCESS(rv, rv);
      mState = MUXING_FRAG;
      break;
    }
    case MUXING_FRAG:
    {
      rv = mCompositor->GenerateMoof(aTrackType);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    case MUXING_MFRA:
    {
      rv = mCompositor->GenerateMfra();
      NS_ENSURE_SUCCESS(rv, rv);
      mState = MUXING_DONE;
      break;
    }
  }
  mBlobReady = true;
  return NS_OK;
}

nsresult
ISOMediaWriter::WriteEncodedTrack(const EncodedFrameContainer& aData,
                                  uint32_t aFlags)
{
  MOZ_ASSERT(mState == MUXING_FRAG);

  Fragmentation* frag = nullptr;
  uint32_t len = aData.GetEncodedFrames().Length();
  for (uint32_t i = 0; i < len; i++) {
    nsRefPtr<EncodedFrame> frame(aData.GetEncodedFrames()[i]);
    EncodedFrame::FrameType type = frame->GetFrameType();
    if (type == EncodedFrame::AUDIO_FRAME ||
        type == EncodedFrame::AAC_CSD) {
      frag = mAudioFragmentation;
    } else if (type == EncodedFrame::I_FRAME ||
               type == EncodedFrame::P_FRAME ||
               type == EncodedFrame::B_FRAME ||
               type == EncodedFrame::AVC_CSD) {
      frag = mVideoFragmentation;
    } else {
      MOZ_ASSERT(0);
      return NS_ERROR_FAILURE;
    }

    frag->AddFrame(frame);
  }

  // Only one FrameType in EncodedFrameContainer so it doesn't need to be
  // inside the for-loop.
  if (frag && (aFlags & END_OF_STREAM)) {
    frag->SetEndOfStream();
  }

  nsresult rv;
  if (mCompositor->HasAudioTrack() &&
      (mAudioFragmentation->HasEnoughData() || mAudioFragmentation->EOS())) {
    rv = RunState(Audio_Track);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  if (mCompositor->HasVideoTrack() &&
      (mVideoFragmentation->HasEnoughData() || mVideoFragmentation->EOS())) {
    rv = RunState(Video_Track);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Jump to MUXING_MFRA if audio and video are EOS.
  if (aFlags & END_OF_STREAM) {
    if (mCompositor->HasAudioTrack() && !mAudioFragmentation->EOS()) {
      return NS_OK;
    }
    if (mCompositor->HasVideoTrack() && !mVideoFragmentation->EOS()) {
      return NS_OK;
    }
    mState = MUXING_MFRA;
  }

  return NS_OK;
}

bool
ISOMediaWriter::IsWritingComplete()
{
  // reach the last one state and the final blob has been blobbed out.
  if (mState == MUXING_DONE && !mBlobReady) {
    return true;
  }

  return false;
}

nsresult
ISOMediaWriter::GetContainerData(nsTArray<nsTArray<uint8_t> >* aOutputBufs,
                                 uint32_t aFlags)
{
  if (mBlobReady) {
    mBlobReady = false;
    aOutputBufs->AppendElement();
    nsresult err = mCompositor->GetBuf(aOutputBufs->LastElement());
    if (err == NS_OK && debugOut) {
      fwrite(aOutputBufs->LastElement().Elements(), aOutputBufs->LastElement().Length(), 1, debugOut);
      if (mState == MUXING_DONE) {
        fclose(debugOut);
        debugOut = nullptr;
      }
    }
    return err;
  }
  return NS_OK;
}

nsresult
ISOMediaWriter::SetMetadata(TrackMetadataBase* aMetadata)
{
  if (aMetadata->GetKind() == TrackMetadataBase::METADATA_AAC ) {
    mCompositor->SetMetadata(aMetadata);
    mAudioFragmentation = new Fragmentation(Audio_Track,
                                            mCompositor->GetFragmentDuration(),
                                            aMetadata);
    mCompositor->SetFragment(mAudioFragmentation);

    return NS_OK;
  }
  if (aMetadata->GetKind() == TrackMetadataBase::METADATA_AVC) {
    mCompositor->SetMetadata(aMetadata);
    mVideoFragmentation = new Fragmentation(Video_Track,
                                            mCompositor->GetFragmentDuration(),
                                            aMetadata);
    mCompositor->SetFragment(mVideoFragmentation);

    // TODO: this supports audio only, so once we get meta, it is safe to move
    // next stage.
    // TODO: to move next stage, it needs both meta and codec specific data.

    RunState(Video_Track);
    return NS_OK;
  }

  LOG("wrong meta data type!");
  MOZ_ASSERT(false);
  return NS_ERROR_FAILURE;
}

}
