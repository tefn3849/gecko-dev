/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_H
#define GECKO_TASK_TRACER_H

/**
 * TaskTracer is a tool for tracing Source Events, Source Events are usually
 * some kinds of platform events we're interested in, such as input event, timer
 * event, network event... etc. When a source event is created, it also generates
 * a chain of Tasks/nsRunnables, and these Tasks/nsRunnables are dispatched to
 * multiple threads or processes. However, the profiler we have in current lack
 * the ability of tracing correlation between these tasks. TaskTracer provides
 * a way to record information of each tasks (latency, execution time... etc.),
 * and which source event is this task originally dispatched from, given
 * developers a clear scope of the correlation between tasks and runnables.
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
  HOME_KEY
};

// Create a traced Task to be run by a message loop.
Task *CreateTracedTask(Task *aTask);

// Create a traced nsIRunnable to be run by an nsThread.
nsIRunnable *CreateTracedRunnable(nsIRunnable *aRunnable);

// Free the TraceInfo allocated on its tread local storage.
void FreeTraceInfo();

// Create a source event of type SourceEventType::TOUCH, where aX and aY the
// touched coordinates on screen.
void CreateSETouch(int aX, int aY);
void CreateSEMouse(int aX, int aY);
void CreateSEKey(SourceEventType aKeyType);

// Save the currently-traced task. Usually used when current thread is already
// tracing a task, but a source event is generated from this point. Create a
// source event for tracking overwrites the tracing task on current thread, so
// save the currently-traced task before overwriting, and restore them later.
void SaveCurTraceInfo();

// Restore the previously saved task info, usually pair up with
// SaveCurTraceInfo().
void RestorePrevTraceInfo();

} // namespace tasktracer
} // namespace mozilla.

#endif
