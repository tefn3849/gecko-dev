/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_IMPL_H
#define GECKO_TASK_TRACER_IMPL_H

#include "GeckoTaskTracer.h"

namespace mozilla {
namespace tasktracer {

// Each thread owns a TraceInfo on its tread local storage.
struct TraceInfo
{
  uint64_t mCurTraceSourceId;
  uint64_t mCurTaskId;
  uint64_t mSavedCurTraceSourceId;
  uint64_t mSavedCurTaskId;

  SourceEventType mCurTraceSourceType;
  SourceEventType mSavedCurTraceSourceType;

  uint32_t mThreadId;
  uint32_t mLastUniqueTaskId;

  char* mThreadName;
  char* mProcessName;
};

// Initialize the TaskTracer.
void InitTaskTracer();

// Return true if TaskTracer has successfully initialized and setup.
bool IsInitialized();

// Return the TraceInfo of current thread, allocate a new one if not exit.
TraceInfo* GetTraceInfo();

uint64_t GenNewUniqueTaskId();

// Make sure to pair up with RestorePrevTraceInfo() if called.
void SaveCurTraceInfo();
void RestorePrevTraceInfo();

void SetCurTraceInfo(uint64_t aSourceEventId, uint64_t aParentTaskId,
                     uint32_t aSourceEventType);

void GetCurTraceInfo(uint64_t* aOutSourceEventId, uint64_t* aOutParentTaskId,
                     uint32_t* aOutSourceEventType);

void SetThreadName(const char* aName);

void SetProcessName(const char* aName);

/**
 * Logging functions of different trace actions.
 */
enum ActionType {
  ACTION_DISPATCH = 0,
  ACTION_BEGIN,
  ACTION_END,
  ACTION_ADD_LABEL,
  ACTION_GET_VTABLE
};

void LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId,
                 uint64_t aSourceEventId, SourceEventType aSourceEventType);

void LogBegin(uint64_t aTaskId, uint64_t aSourceEventId);

void LogEnd(uint64_t aTaskId, uint64_t aSourceEventId);

void LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, int* aVptr);

} // namespace mozilla
} // namespace tasktracer

#endif
