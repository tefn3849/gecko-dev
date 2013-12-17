/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TracedCommon.h"

namespace mozilla {
namespace tasktracer {

// Implementing TracedRunnable.h

NS_IMPL_ISUPPORTS1(TracedRunnable, nsIRunnable)

NS_IMETHODIMP
TracedRunnable::Run()
{
  LogTaskAction(ACTION_START, mTaskId, mSourceEventId, mSourceEventType);

  AttachTracedInfo();
  nsresult rv = mFactualObj->Run();
  ClearTracedInfo();

  LogTaskAction(ACTION_FINISHED, mTaskId, mSourceEventId, mSourceEventType);
  return rv;
}

TracedRunnable::TracedRunnable(nsIRunnable *aFatualObj)
  : mFactualObj(aFatualObj)
  , mSourceEventId(0)
  , mSourceEventType(UNKNOWN)
{
  mTaskId = GenNewUniqueTaskId();
  SetupSourceEvent();
}

TracedRunnable::~TracedRunnable()
{
}

void
TracedRunnable::SetupSourceEvent()
{
  TracedInfo* info = GetTracedInfo();
  mSourceEventId = info->curTracedTaskId;
  mSourceEventType = info->curTracedTaskType;

  LogTaskAction(ACTION_DISPATCH, mTaskId, mSourceEventId, mSourceEventType);
}

void
TracedRunnable::AttachTracedInfo()
{
  SetCurTracedId(mSourceEventId);
  SetCurTracedType(mSourceEventType);
}

void
TracedRunnable::ClearTracedInfo()
{
  SetCurTracedId(0);
  SetCurTracedType(UNKNOWN);
}

TracedTask::TracedTask(Task* aFatualObj)
  : mFactualObj(aFatualObj)
  , mSourceEventId(0)
  , mSourceEventType(UNKNOWN)
{
  mTaskId = GenNewUniqueTaskId();
  SetupSourceEvent();
}

TracedTask::~TracedTask()
{
  if (mFactualObj) {
    delete mFactualObj;
    mFactualObj = nullptr;
  }
}

void
TracedTask::SetupSourceEvent()
{
  TracedInfo* info = GetTracedInfo();
  mSourceEventId = info->curTracedTaskId;
  mSourceEventType = info->curTracedTaskType;

  LogTaskAction(ACTION_DISPATCH, mTaskId, mSourceEventId, mSourceEventType);
}

void
TracedTask::Run()
{
  LogTaskAction(ACTION_START, mTaskId, mSourceEventId, mSourceEventType);

  AttachTracedInfo();
  mFactualObj->Run();
  ClearTracedInfo();

  LogTaskAction(ACTION_FINISHED, mTaskId, mSourceEventId, mSourceEventType);
}

void
TracedTask::AttachTracedInfo()
{
  SetCurTracedId(mSourceEventId);
  SetCurTracedType(mSourceEventType);
}

void
TracedTask::ClearTracedInfo()
{
  SetCurTracedId(0);
  SetCurTracedType(UNKNOWN);
}

// Implementing GeckoTaskTracer.h

nsIRunnable*
CreateTracedRunnable(nsIRunnable *aRunnable)
{
  nsCOMPtr<TracedRunnable> runnable = new TracedRunnable(aRunnable);
  return runnable.get();
}

Task*
CreateTracedTask(Task *aTask)
{
  TracedTask* task = new TracedTask(aTask);
  return task;
}

} // namespace tasktracer
} // namespace mozilla
