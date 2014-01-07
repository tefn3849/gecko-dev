/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRACED_TASK_COMMON_H
#define TRACED_TASK_COMMON_H

#include "base/task.h"
#include "GeckoTaskTracer.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"

namespace mozilla {
namespace tasktracer {

class TracedTaskCommon
{
public:
  TracedTaskCommon();
  virtual ~TracedTaskCommon() {}

protected:
  // Allocates a TraceInfo for the current thread on its thread local storage
  // if not exist; Sets the source event Id and source event type of this
  // runnable to the currently-traced task Id and task type from the TraceInfo
  // of current thread.
  void SetupSourceEvent();

  // Before calling the Run() of its factual object, sets the currently-traced
  // task Id from the TraceInfo of current thread, to its source event Id.
  // Setup other information if needed. Should call ClearTraceInfo() to reset
  // them when done.
  void AttachTraceInfo();
  void ClearTraceInfo();

  // Its own task Id, an unique number base on its thread Id and a last unique
  // task Id stored in its TraceInfo.
  uint64_t mTaskId;

  // The source event Id, and is set to the currently-traced task Id from its
  // TraceInfo in the call of SetupSourceEvent().
  uint64_t mSourceEventId;

  // The source event type, and is set to the currently-traced task type from
  // its TraceInfo in the call of SetupSourceEvent().
  SourceEventType mSourceEventType;
};

class TracedRunnable : public TracedTaskCommon
                     , public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  TracedRunnable(nsIRunnable *aFactualObj)
    : TracedTaskCommon()
    , mFactualObj(aFactualObj)
  {}

  virtual ~TracedRunnable() {}

private:
  // The factual runnable object wrapped by this TracedRunnable wrapper.
  nsCOMPtr<nsIRunnable> mFactualObj;
};

class TracedTask : public TracedTaskCommon
                 , public Task
{
public:
  TracedTask(Task *aFactualObj)
    : TracedTaskCommon()
    , mFactualObj(aFactualObj)
  {}

  ~TracedTask();

  virtual void Run();

private:
  // The factual task object wrapped by this TracedTask wrapper.
  Task *mFactualObj;
};

} // namespace tasktracer
} // namespace mozilla

#endif
