/* Copyright 2013 Mozilla Foundation and Mozilla contributors
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

#ifndef GONKDISPLAYJB_H
#define GONKDISPLAYJB_H

#include "GonkDisplay.h"
#include "FramebufferSurface.h"
#include "hardware/hwcomposer.h"
#include "hardware/power.h"
#include "ui/Fence.h"
#include "utils/RefBase.h"

#include "nsTArray.h"

namespace mozilla {

class MOZ_EXPORT GonkDisplayJB : public GonkDisplay {
public:
    GonkDisplayJB();
    ~GonkDisplayJB();

    virtual ANativeWindow* GetNativeWindow();

    virtual void SetEnabled(bool enabled);

    virtual void OnEnabled(OnEnabledCallbackType callback);

    virtual void* GetHWCDevice();

    virtual void* GetFBSurface();

    virtual bool SwapBuffers(EGLDisplay dpy, EGLSurface sur);

    // HDMI Testing
    virtual ANativeWindowBuffer* DequeueBuffer(ANativeWindowBuffer** aBuf_hdmi);

    virtual ANativeWindowBuffer* DequeueBuffer();

    // HDMI Testing
    virtual bool QueueBuffer(ANativeWindowBuffer* buf);

    virtual void UpdateFBSurface(EGLDisplay dpy, EGLSurface sur);

    virtual void SetFBReleaseFd(int fd);

    virtual int GetPrevFBAcquireFd();

    bool Post(buffer_handle_t buf, int fence);

    // HDMI Testing
    virtual hwc_display_contents_1_t* GetHDMILayerList() {return mList_hdmi;}

    // HDMI Testing
    hwc_display_contents_1_t* mList_hdmi;

    virtual void AddDisplay(
        const uint32_t aType,
        const android::sp<android::IGraphicBufferProducer>& aProducer = nullptr);

    virtual void RemoveDisplay(const uint32_t aType);

    virtual DisplayDevice* GetDevice(const uint32_t aType);

    virtual ANativeWindow* GetNativeWindow(const uint32_t aType);

private:
    hw_module_t const*        mModule;
    hw_module_t const*        mFBModule;
    hwc_composer_device_1_t*  mHwc;
    framebuffer_device_t*     mFBDevice;
    power_module_t*           mPowerModule;
    android::sp<android::FramebufferSurface> mFBSurface;
    android::sp<ANativeWindow> mSTClient;
    android::sp<android::IGraphicBufferAlloc> mAlloc;
    int mFence;
    hwc_display_contents_1_t* mList;
    uint32_t mWidth;
    uint32_t mHeight;
    OnEnabledCallbackType mEnabledCallback;

    // HDMI Testing
    android::sp<android::FramebufferSurface> mFBSurface_hdmi;
    android::sp<ANativeWindow> mSTClient_hdmi;
    int mFence_hdmi;
    uint32_t mWidth_hdmi;
    uint32_t mHeight_hdmi;

    nsTArray<DisplayDevice> mDevices;
};

}

#endif /* GONKDISPLAYJB_H */
