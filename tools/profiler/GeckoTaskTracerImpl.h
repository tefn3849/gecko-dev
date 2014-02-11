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
  uint64_t mCurTraceSourceId; // SourceEvent Id of currently running task.
  uint64_t mCurTaskId; // Task Id of currently running task.
  uint64_t mSavedCurTraceSourceId;
  uint64_t mSavedCurTaskId;

  SourceEventType mCurTraceSourceType; // Source event type of currently running task.
  SourceEventType mSavedCurTraceSourceType;

  uint32_t mThreadId;

  // A serial number to generate an unique task Id for a new TracedRunnable/TracedTask.
  uint32_t mLastUniqueTaskId;
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

/**
 * Logging functions of different trace actions.
 */
enum ActionType {
  ACTION_DISPATCH = 1,
  ACTION_START,
  ACTION_END,
  ACTION_ADD_LABEL,
  ACTION_GET_VTABLE
};

void LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId,
                 uint64_t aSourceEventId, SourceEventType aSourceEventType);

void LogStart(uint64_t aTaskId, uint64_t aSourceEventId);

void LogEnd(uint64_t aTaskId, uint64_t aSourceEventId);

void LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, int* aVptr);

} // namespace mozilla
} // namespace tasktracer

#endif
