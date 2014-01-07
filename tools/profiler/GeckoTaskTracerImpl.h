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

enum ActionType {
  ACTION_DISPATCH,
  ACTION_START,
  ACTION_FINISHED
};

// Each thread owns a TraceInfo on its tread local storage, keeps track of
// currently-traced task and other information.
struct TraceInfo
{
  // Task Id of the currently-traced task.
  uint64_t mCurTraceTaskId;

  // Holds the value of curTracedTaskId when SaveCurTraceInfo() is called.
  uint64_t mSavedTraceTaskId;

  // Source event type of the currently-traced task.
  SourceEventType mCurTraceTaskType;

  // Holds the value of curTracedTaskType when SaveCurTraceInfo() is called.
  SourceEventType mSavedTraceTaskType;

  // Thread Id of its owner thread.
  uint32_t mThreadId;

  // A serial number to generate an unique task Id for a new
  // TracedRunnable/TracedTask.
  uint32_t mLastUniqueTaskId;
};

// Initialize and setup needed information for TaskTracer.
void InitTaskTracer();
void StartTaskTracer();
bool IsStarted();

// Return the TraceInfo of this thread, allocate a new one if not exit.
TraceInfo* GetTraceInfo();

// Generate an unique task id for a TeacedRunnable/TracedTask base on its
// owner thread id and the last unique task id.
uint64_t GenNewUniqueTaskId();

// Set the id of tracing task on current thread with aTaskId.
void SetCurTraceId(uint64_t aTaskId);

// Return the task id of tracing task on current thread.
uint64_t GetCurTraceId();

// Set the source event type of tracing task on current thread with aType.
// aType could be TOUCH, MOUSE, POWER_KEY...etc.
void SetCurTraceType(SourceEventType aType);

// Return the source event type of tracing task on current thread.
SourceEventType GetCurTraceType();

// Log the snapshot of current tracing activity.
void LogTaskAction(ActionType aActionType, uint64_t aTaskId,
                   uint64_t aSourceEventId, SourceEventType aSourceEventType);

} // namespace mozilla
} // namespace tasktracer

#endif
