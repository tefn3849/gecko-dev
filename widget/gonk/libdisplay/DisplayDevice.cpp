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
#include "DisplayDevice.h"
#include "GonkDisplay.h"
#include "FramebufferSurface.h"
#include "BootAnimation.h"

using namespace android;

namespace mozilla {

NS_IMPL_ISUPPORTS(DisplayInfo, nsIDisplayInfo, nsISupports)

DisplayInfo::DisplayInfo(uint32_t aId, int aFlags)
  : mId(aId)
  , mFlags(aFlags)
{
}

DisplayInfo::~DisplayInfo()
{
}

//
// nsIDisplayInfo implementation.
//
NS_IMETHODIMP DisplayInfo::GetId(int32_t *aId)
{
  *aId = mId;
  return NS_OK;
}

NS_IMETHODIMP DisplayInfo::GetConnected(bool *aConnected)
{
	*aConnected = mFlags & DisplayInfo::ADDED;
	return NS_OK;
}

DisplayDevice::DisplayDevice(uint32_t aType)
  : mType(aType)
{
}

DisplayDevice::~DisplayDevice()
{
}

ANativeWindow*
DisplayDevice::GetNativeWindow()
{
  if (mType == GonkDisplay::DISPLAY_PRIMARY) {
    StopBootAnimation();
  }

  return mSTClient.get();
}

void*
DisplayDevice::GetFBSurface()
{
    return mFBSurface.get();
}

void
DisplayDevice::SetFBReleaseFd(int fd)
{
    mFBSurface->setReleaseFenceFd(fd);
}

int
DisplayDevice::GetPrevFBAcquireFd()
{
    return mFBSurface->GetPrevFBAcquireFd();
}

}
