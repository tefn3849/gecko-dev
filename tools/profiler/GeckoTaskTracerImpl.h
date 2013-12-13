/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GECKO_TASK_TRACER_IMPL_H
#define GECKO_TASK_TRACER_IMPL_H

#include "GeckoTaskTracer.h"
#include "mozilla/RefPtr.h"


#define TASK_TRACE_BUF_SIZE 2048

namespace mozilla {
namespace tasktracer {

class SourceEventBase : public RefCounted<SourceEventBase>
{
public:
  enum SourceEventType {
    UNKNOWN,
    TOUCH,
    MOUSE,
    POWER_KEY,
    HOME_KEY,
  };

  SourceEventBase()
  {
    uint64_t newTaskId = GenNewUniqueTaskId();
    // Install new task Id.
    *GetCurrentThreadTaskIdPtr() = newTaskId;
    mOriginTaskId = newTaskId;
  }

  virtual ~SourceEventBase() {}

  virtual SourceEventType GetType() const = 0;

  uint64_t mOriginTaskId;
};

class SourceEventTouch : public SourceEventBase
{
public:
  SourceEventTouch(int aX, int aY)
    : SourceEventBase()
    , mPositionX(aX)
    , mPositionY(aY)
  {}

  SourceEventType GetType() const MOZ_OVERRIDE { return TOUCH; }

  int mPositionX;
  int mPositionY;

};

class SourceEventDummy : public SourceEventBase
{
public:
  SourceEventDummy() : SourceEventBase() {}

  SourceEventType GetType() const MOZ_OVERRIDE { return UNKNOWN; }
};

enum ActionType {
  ACTION_DISPATCH,
  ACTION_START,
  ACTION_FINISHED
};

/**
 * A snapshot of the current tracing activity.
 */
struct TracedActivity {
  RefPtr<SourceEventBase> sourceEvent;
  ActionType actionType;
  uint64_t tm;                  // Current time in microseconds.
  uint64_t taskId;              // Task id.
  uint64_t originTaskId;        // Origin's task id.
};

/**
 * Each thread owns a TracedInfo on its tread local storage, keeping track of
 * all information of TracedRunnable tasks dispatched from this thread.
 */
struct TracedInfo {
  RefPtr<SourceEventBase> currentlyTracedSourceEvent;
  uint32_t threadId;            // Thread id of its owner thread.
  uint64_t currentTracedTaskId; // Task id of the currently-traced TracedRunnable task.
  uint32_t lastUniqueTaskId;    // A serial number to generate an unique task id for a new TracedRunnable.

  int actNext;
  int actFirst;
  TracedActivity activities[TASK_TRACE_BUF_SIZE];
};

/**
 * Returns the TracedInfo of this thread, allocate a new one if not exit.
 */
TracedInfo* GetTracedInfo();

/**
 * Log the snapshot the current tracing activity.
 */
void LogAction(ActionType aType, uint64_t aTid, uint64_t aOTid);

void LogTaskAction(ActionType aType, uint64_t aTaskId, SourceEventBase* aSourceEvent);

/**
 * Clear the TracedInfo when the Run() of its factual object is done,
 * this should be in pair with SetupTracedInfo().
 */
//void ClearTracedInfo();

void LogSamplerEnter(const char *aInfo);

void LogSamplerExit(const char *aInfo);

void InitRunnableTrace();



} // namespace mozilla
} // namespace tasktracer

#endif
