/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_IMPL_H
#define GECKO_TASK_TRACER_IMPL_H

#include "GeckoTaskTracer.h"


#define TASK_TRACE_BUF_SIZE 2048

namespace mozilla {
namespace tasktracer {

enum ActionType {
  ACTION_DISPATCH,
  ACTION_START,
  ACTION_FINISHED
};

/**
 * Each thread owns a TracedInfo on its tread local storage, keeps track of
 * currently-traced task and other information.
 */
struct TracedInfo {
  uint64_t curTracedTaskId; // Task id of the currently-traced task.
  uint64_t savedTracedTaskId;
  SourceEventType curTracedTaskType; // Source event type of the currently-traced task.
  SourceEventType savedTracedTaskType;
  uint32_t threadId;            // Thread id of its owner thread.
  uint32_t lastUniqueTaskId;    // A serial number to generate an unique task id for a new TracedRunnable.
};

/**
 * Initialize and setup needed information for TaskTracer.
 */
void InitTaskTracer();

/**
 * Return the TracedInfo of this thread, allocate a new one if not exit.
 */
TracedInfo* GetTracedInfo();

/**
 * Log the snapshot of current tracing activity.
 */
void LogTaskAction(ActionType aActionType, uint64_t aTaskId,
                   uint64_t aSourceEventId, SourceEventType aSourceEventType);

} // namespace mozilla
} // namespace tasktracer

#endif
