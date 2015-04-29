/* Copyright 2015 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DISPLAYDEVICE_H
#define DISPLAYDEVICE_H

#include <system/window.h>
#include <utils/StrongPointer.h>
#include "mozilla/Types.h"
#include "nsIDisplayInfo.h"

#define EXPORT MOZ_EXPORT __attribute__ ((weak))

class nsWindow;

namespace android {
class FramebufferSurface;
}

namespace mozilla {
class GonkDisplayJB;

class DisplayInfo : public nsIDisplayInfo {
public:
  enum {
    ADDED = 1 << 0,
    REMOVED = 1 << 1
  };

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDISPLAYINFO

  DisplayInfo(uint32_t aId, int aFlags);

private:
  ~DisplayInfo();

  uint32_t mId;
  int mFlags;
};

class DisplayDevice {
  friend class GonkDisplayJB;
  friend class nsWindow;
public:
  NS_INLINE_DECL_REFCOUNTING(DisplayDevice)
  DisplayDevice(uint32_t aType);

  EXPORT ANativeWindow* GetNativeWindow();
  EXPORT void* GetFBSurface();

  /**
   * Set FramebufferSurface ReleaseFence's file descriptor.
   * ReleaseFence will be signaled after the HWC has finished reading
   * from a buffer.
   */
  EXPORT void SetFBReleaseFd(int fd);

  /**
   * Get FramebufferSurface AcquireFence's file descriptor
   * AcquireFence will be signaled when a buffer's content is available.
   */
  EXPORT int GetPrevFBAcquireFd();

  float xdpi;
  int32_t surfaceformat;

private:
  ~DisplayDevice();

  uint32_t mType;
  uint32_t mWidth;
  uint32_t mHeight;
  int32_t mFence;
  android::sp<android::FramebufferSurface> mFBSurface;
  android::sp<ANativeWindow> mSTClient;
};

}
#endif /* DISPLAYDEVICE_H */
