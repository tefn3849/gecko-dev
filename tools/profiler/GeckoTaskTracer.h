/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_H
#define GECKO_TASK_TRACER_H

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
};

/**
 * Create a traced Task to be run by a message loop.
 */
Task *CreateTracedTask(Task *aTask);

/**
 * Create a traced nsIRunnable to be run by an nsThread.
 */
nsIRunnable *CreateTracedRunnable(nsIRunnable *aRunnable);

/**
 * Free the TracedInfo allocated on its tread local storage.
 */
void FreeTracedInfo();

/**
 * Generate an unique task id for a TeacedRunnable/TracedTask base on its
 * owner thread id and the last unique task id.
 */
uint64_t GenNewUniqueTaskId();

/**
 * Create a source event of type SourceEventType::TOUCH, where aX and aY the
 * touched coordinates on screen.
 */
void CreateSETouch(int aX, int aY);

/**
 * Set the id of tracing task on current thread with aTaskId.
 */
void SetCurTracedId(uint64_t aTaskId);

/**
 * Return the task id of tracing task on current thread.
 */
uint64_t GetCurTracedId();

/**
 * Set the source event type of tracing task on current thread with aType.
 * aType could be TOUCH, MOUSE, POWER_KEY...etc.
 */
void SetCurTracedType(SourceEventType aType);

/**
 * Return the source event type of tracing task on current thread.
 */
SourceEventType GetCurTracedType();

/**
 * Save the currently-traced task. Usually used when current thread is already
 * tracing a task, but a source event is generated from this point. Create a
 * source event for tracking overwrites the tracing task on current thread, so
 * save the currently-traced task before overwriting, and restore them later.
 */
void SaveCurTracedInfo();

/**
 * Restore the previously saved task info, usually pair up with
 * SaveCurTracedInfo().
 */
void RestorePrevTracedInfo();

} // namespace tasktracer
} // namespace mozilla.

#endif
