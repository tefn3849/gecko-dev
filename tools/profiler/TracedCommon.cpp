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
  LogTaskAction(ACTION_START, mTaskId, mOriginTaskId, mSEType);

  AttachTracedInfo();
  nsresult rv = mFactualObj->Run();
  ClearTracedInfo();

  LogTaskAction(ACTION_FINISHED, mTaskId, mOriginTaskId, mSEType);
  return rv;
}

TracedRunnable::TracedRunnable(nsIRunnable *aFatualObj)
  : mFactualObj(aFatualObj)
  , mOriginTaskId(0)
  , mSEType(UNKNOWN)
{
  mTaskId = GenNewUniqueTaskId();
  InitSourceEvent();
}

TracedRunnable::~TracedRunnable()
{
}

void
TracedRunnable::InitSourceEvent()
{
  TracedInfo* info = GetTracedInfo();
  mOriginTaskId = info->curTracedTaskId;
  mSEType = info->curTracedTaskType;

  LogTaskAction(ACTION_DISPATCH, mTaskId, mOriginTaskId, mSEType);
}

void
TracedRunnable::AttachTracedInfo()
{
  SetCurTracedId(mOriginTaskId);
  SetCurTracedType(mSEType);
}

void
TracedRunnable::ClearTracedInfo()
{
  SetCurTracedId(0);
  SetCurTracedType(UNKNOWN);
}

TracedTask::TracedTask(Task* aFatualObj)
  : mFactualObj(aFatualObj)
  , mOriginTaskId(0)
  , mSEType(UNKNOWN)
{
  mTaskId = GenNewUniqueTaskId();
  InitSourceEvent();
}

TracedTask::~TracedTask()
{
  if (mFactualObj) {
    delete mFactualObj;
    mFactualObj = nullptr;
  }
}

void
TracedTask::InitSourceEvent()
{
  TracedInfo* info = GetTracedInfo();
  mOriginTaskId = info->curTracedTaskId;
  mSEType = info->curTracedTaskType;

  LogTaskAction(ACTION_DISPATCH, mTaskId, mOriginTaskId, mSEType);
}

void
TracedTask::Run()
{
  LogTaskAction(ACTION_START, mTaskId, mOriginTaskId, mSEType);

  AttachTracedInfo();
  mFactualObj->Run();
  ClearTracedInfo();

  LogTaskAction(ACTION_FINISHED, mTaskId, mOriginTaskId, mSEType);
}

void
TracedTask::AttachTracedInfo()
{
  SetCurTracedId(mOriginTaskId);
  SetCurTracedType(mSEType);
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
  TracedRunnable *runnable = new TracedRunnable(aRunnable);
  return runnable;
}

Task*
CreateTracedTask(Task *aTask)
{
  TracedTask *task = new TracedTask(aTask);
  return task;
}

} // namespace tasktracer
} // namespace mozilla
