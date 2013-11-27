/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "nsTArray.h"

#ifndef MuxerOperation_h_
#define MuxerOperation_h_

namespace mozilla {

/**
 * The interface for ISO box. All Boxes inherited this interface.
 * Generate() and Write() are needed to be called to producet a complete box.
 *
 * Generate() will generate all the data structure and its size.
 *
 * Write() will write all data into muxing output stream (ISOCompositor actually)
 * and update the data which can't be known at Generate() (for example, the
 * offset of the video data in mp4 file).
 *
 * ISO base media format is composed of several container boxes and the contained
 * boxes. The container boxes hold a list of MuxerOperation which is implemented
 * by contained boxes. The contained boxes will be called via the list.
 * For example:
 *   MovieBox (container) ---> boxes (array of MuxerOperation)
 *                              |---> MovieHeaderBox (full box)
 *                              |---> TrakBox (container)
 *                              |---> MovieExtendsBox (container)
 */
class MuxerOperation {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MuxerOperation)

  // Generate data of this box and its contained box, and calculate box size.
  virtual nsresult Generate(uint32_t* aBoxSize)
  {
    MOZ_ASSERT(0);
    return NS_ERROR_FAILURE;
  }

  // Write data to stream.
  virtual nsresult Write()
  {
    MOZ_ASSERT(0);
    return NS_ERROR_FAILURE;
  }

  // Find the box type via its name (name is the box type defined in 14496-12;
  // for example, 'moov' is the name of MovieBox).
  // It can only look downstream including itself and the box in the boxes
  // list if exists. It can't look upstream.
  virtual nsresult Find(const nsACString& aType, MuxerOperation** aOperation)
  {
    MOZ_ASSERT(0);
    return NS_ERROR_FAILURE;
  }

  virtual ~MuxerOperation() {}
};

}
#endif
