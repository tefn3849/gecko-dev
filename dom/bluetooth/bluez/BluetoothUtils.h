/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothutils_h__
#define mozilla_dom_bluetooth_bluetoothutils_h__

#include "BluetoothCommon.h"
#include "js/TypeDecls.h"

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
#include "GeckoTaskTracerImpl.h"
using namespace mozilla::tasktracer;
#endif

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;
class BluetoothValue;
class BluetoothReplyRunnable;

bool
SetJsObject(JSContext* aContext,
            const BluetoothValue& aValue,
            JS::Handle<JSObject*> aObj);

nsString
GetObjectPathFromAddress(const nsAString& aAdapterPath,
                         const nsAString& aDeviceAddress);

nsString
GetAddressFromObjectPath(const nsAString& aObjectPath);

bool
BroadcastSystemMessage(const nsAString& aType,
                       const InfallibleTArray<BluetoothNamedValue>& aData);

void
DispatchBluetoothReply(BluetoothReplyRunnable* aRunnable,
                       const BluetoothValue& aValue,
                       const nsAString& aErrorStr);

void
ParseAtCommand(const nsACString& aAtCommand, const int aStart,
               nsTArray<nsCString>& aRetValues);

void
DispatchStatusChangedEvent(const nsAString& aType,
                           const nsAString& aDeviceAddress,
                           bool aStatus);

#ifdef MOZ_TASK_TRACER
inline void
CreateBTSourceEvent(nsString& aIface, nsAString& aName)
{
  SaveCurTraceInfo();

  int32_t offset = aIface.RFindChar('.');
  nsAutoString iface;
  if (offset != kNotFound) {
    iface = Substring(aIface, offset + 1);
  }

  CreateSourceEvent(SourceEventType::BLUETOOTH,
                    NS_ConvertUTF16toUTF8(iface).get(),
                    NS_ConvertUTF16toUTF8(aName).get());

  TraceInfo* info = GetTraceInfo();
  LogDispatch(info->mCurTraceTaskId, 0,
              info->mCurTraceTaskId, SourceEventType::BLUETOOTH);
  LogStart(info->mCurTraceTaskId, info->mCurTraceTaskId);
}

inline void
DestroyBTSourceEvent()
{
  TraceInfo* info = GetTraceInfo();
  LogEnd(info->mCurTraceTaskId, info->mCurTraceTaskId);

  RestorePrevTraceInfo();
}

#endif

END_BLUETOOTH_NAMESPACE

#endif
