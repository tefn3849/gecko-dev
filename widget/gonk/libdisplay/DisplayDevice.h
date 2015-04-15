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
#include "nsIDisplayDevice.h"

class nsWindow;

namespace android {
class FramebufferSurface;
}

namespace mozilla {
class GonkDisplayJB;

class DisplayDevice : public nsIDisplayDevice {
  friend class GonkDisplayJB;
  friend class nsWindow;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDISPLAYDEVICE

  DisplayDevice();
  DisplayDevice(uint32_t aType);
  DisplayDevice(const DisplayDevice& aDisplayDevice);
  ~DisplayDevice();

private:
  uint32_t mType;
  uint32_t mWidth;
  uint32_t mHeight;
  int32_t mFence;
  int32_t mSurfaceformat;
  float mXdpi;
  android::sp<android::FramebufferSurface> mFBSurface;
  android::sp<ANativeWindow> mSTClient;
  bool mConnected;
};

}
#endif /* DISPLAYDEVICE_H */
