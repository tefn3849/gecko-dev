/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoTaskTracerImpl.h"
#include "TracedTaskCommon.h"

namespace mozilla {
namespace tasktracer {

TracedTaskCommon::TracedTaskCommon()
  : mSourceEventId(0)
  , mSourceEventType(SourceEventType::UNKNOWN)
{
  Init();
}

void
TracedTaskCommon::Init()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  mTaskId = GenNewUniqueTaskId();

  // TODO: This is a temporary solution to eliminate orphan tasks, once we have
  // enough source events setup, this should go away eventually, or hopefully.
  if (!info->mCurTraceSourceId) {
    info->mCurTraceSourceId = mTaskId;
    info->mCurTraceSourceType = mSourceEventType;
  }

  mSourceEventId = info->mCurTraceSourceId;
  mSourceEventType = info->mCurTraceSourceType;

  LogDispatch(mTaskId, info->mCurTaskId, mSourceEventId, mSourceEventType);
}

void
TracedTaskCommon::SetTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = mSourceEventId;
  info->mCurTraceSourceType = mSourceEventType;
  info->mCurTaskId = mTaskId;
}

void
TracedTaskCommon::ClearTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = 0;
  info->mCurTraceSourceType = SourceEventType::UNKNOWN;
  info->mCurTaskId = 0;
}

/**
 * Implementation of class TracedRunnable.
 */
TracedRunnable::TracedRunnable(nsIRunnable* aOriginalObj)
  : TracedTaskCommon()
  , mOriginalObj(aOriginalObj)
{
  LogVirtualTablePtr(mTaskId, mSourceEventId, *(int**)(aOriginalObj));
}

NS_IMETHODIMP
TracedRunnable::Run()
{
  LogBegin(mTaskId, mSourceEventId);

  SetTraceInfo();
  nsresult rv = mOriginalObj->Run();
  ClearTraceInfo();

  LogEnd(mTaskId, mSourceEventId);
  return rv;
}

/**
 * Implementation of class TracedTask.
 */
TracedTask::TracedTask(Task* aOriginalObj)
  : TracedTaskCommon()
  , mOriginalObj(aOriginalObj)
{
  LogVirtualTablePtr(mTaskId, mSourceEventId, *(int**)(aOriginalObj));
}

void
TracedTask::Run()
{
  LogBegin(mTaskId, mSourceEventId);

  SetTraceInfo();
  mOriginalObj->Run();
  ClearTraceInfo();

  LogEnd(mTaskId, mSourceEventId);
}

/**
 * CreateTracedRunnable() returns a TracedRunnable wrapping the original
 * nsIRunnable object, aRunnable.
 */
already_AddRefed<nsIRunnable>
CreateTracedRunnable(nsIRunnable* aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable = new TracedRunnable(aRunnable);
  return runnable.forget();
}

/**
 * CreateTracedTask() returns a TracedTask wrapping the original Task object,
 * aTask.
 */
Task*
CreateTracedTask(Task* aTask)
{
  Task* task = new TracedTask(aTask);
  return task;
}

} // namespace tasktracer
} // namespace mozilla
