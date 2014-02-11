/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_H
#define GECKO_TASK_TRACER_H

/**
 * TaskTracer provides a way to trace the correlation between tasks, as opposed
 * to sample base profilers, TaskTracer is used for finding out where a task is
 * dispatched from, which source event of this task is.
 *
 * Source Events are usually some kinds of I/O events we're interested in, such
 * as touch event, timer event, network event... etc. When a source event is
 * created, it also generates a chain of Tasks/nsRunnables, dispatched to
 * different threads and processes. TaskTracer records information of each tasks
 * (latency, execution time... etc.), and which source event is this task
 * originally dispatched from.
 */

class Task;
class nsIRunnable;

namespace mozilla {
namespace tasktracer {

enum SourceEventType {
  UNKNOWN = 0,
  TOUCH,
  MOUSE,
  POWER_KEY,
  HOME_KEY,
  TIMER,
  BLUETOOTH,
  UNIXSOCKET,
  WIFI
};

/**
 * Public functions for user to call, usually for the purpose of customizing
 * profiling results.
 */

// Create a source event of aType. Make sure to pair up with DestroySourceEvent.
void CreateSourceEvent(SourceEventType aType);
void DestroySourceEvent();

// Add a label to the currently running task, aFormat is the message to log,
// followed by corresponding parameters.
void AddLabel(const char* aFormat, ...);

/**
 * Internal functions.
 */

// Wrap aTask at where message loops post tasks.
Task *CreateTracedTask(Task *aTask);

// Wrap aRunnable at where threads dispatch events.
nsIRunnable *CreateTracedRunnable(nsIRunnable *aRunnable);

// Free the TraceInfo allocated on its tread local storage.
void FreeTraceInfo();

} // namespace tasktracer
} // namespace mozilla.

#endif
