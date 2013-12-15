/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRACED_RUNNABLE_H
#define TRACED_RUNNABLE_H

#include "base/task.h"

#include "nsIRunnable.h"
#include "nsAutoPtr.h"
#include "GeckoTaskTracer.h"
#include "GeckoTaskTracerImpl.h"

namespace mozilla {
namespace tasktracer {

class TracedRunnable : public nsIRunnable {
public:

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  TracedRunnable(nsIRunnable *aFactualObj);
  virtual ~TracedRunnable();

  // Allocates a TracedInfo for the current thread on its thread local storage
  // if not exist, and sets the origin taskId of this runnable to the currently-
  // traced taskID from the TracedInfo of current thread.
  void InitSourceEvent();

  // Returns the original runnable object wrapped by this TracedRunnable.
  already_AddRefed<nsIRunnable> GetFatualObject() {
    nsCOMPtr<nsIRunnable> factualObj = mFactualObj;
    return factualObj.forget();
  }

private:
  // Before calling the Run() of its factual object, sets the currently-traced
  // taskID from the TracedInfo of current thread, to its origin's taskId.
  // Setup other information is needed.
  // Should call ClearTracedInfo() to reset them when done.
  void AttachTracedInfo();
  void ClearTracedInfo();

  // Its own taskID, an unique number base on the tid of current thread and
  // a last unique taskID from the TracedInfo of current thread.
  uint64_t mTaskId;

  // The origin taskId, it's being set to the currently-traced taskID from the
  // TracedInfo of current thread in the call of InitOriginTaskId().
  RefPtr<SourceEventBase> mSourceEvent;

  // The factual runnable object wrapped by this TracedRunnable wrapper.
  nsCOMPtr<nsIRunnable> mFactualObj;
};

class TracedTask : public Task
{
public:
  TracedTask(Task *aFactualObj);
  virtual ~TracedTask();

  virtual void Run();

  void InitSourceEvent();

private:
  void AttachTracedInfo();
  void ClearTracedInfo();

  uint64_t mTaskId;
  RefPtr<SourceEventBase> mSourceEvent;
  Task *mFactualObj;
};

} // namespace tasktracer
} // namespace mozilla

#endif
