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
  mTaskId = GenNewUniqueTaskId();
  Setup();
}

void
TracedTaskCommon::Setup()
{
  TraceInfo* info = GetTraceInfo();
  // TODO: This is a temporary solution to eliminate orphan tasks, once we have
  // enough source events setup, this should go away eventually, or hopefully.
//  if (!info->mCurTraceTaskId) {
//    info->mCurTraceTaskId = mTaskId;
//    info->mCurTraceTaskType = mSourceEventType;
//  }
  mSourceEventId = info->mCurTraceTaskId;
  mSourceEventType = info->mCurTraceTaskType;
  mParentTaskId = info->mParentTaskId;

  LogDispatch(mTaskId, mParentTaskId, mSourceEventId, mSourceEventType);
}

void
TracedTaskCommon::SetTraceInfo()
{
  TraceInfo* info = GetTraceInfo();
  info->mCurTraceTaskId = mSourceEventId;
  info->mCurTraceTaskType = mSourceEventType;
  info->mParentTaskId = mTaskId;
  info->mCurTaskId = mTaskId;
}

void
TracedTaskCommon::ClearTraceInfo()
{
  TraceInfo* info = GetTraceInfo();
  info->mCurTraceTaskId = 0;
  info->mCurTraceTaskType = SourceEventType::UNKNOWN;
  info->mParentTaskId = 0;
  info->mCurTaskId = 0;
}

/**
 * Implementation of class TracedRunnable.
 */
NS_IMPL_ISUPPORTS1(TracedRunnable, nsIRunnable)

NS_IMETHODIMP
TracedRunnable::Run()
{
  LogStart(mTaskId, mSourceEventId);

  SetTraceInfo();
  nsresult rv = mFactualObj->Run();
  ClearTraceInfo();

  LogEnd(mTaskId, mSourceEventId);
  return rv;
}

/**
 * Implementation of class TracedTask.
 */
TracedTask::~TracedTask()
{
  if (mFactualObj) {
    delete mFactualObj;
    mFactualObj = nullptr;
  }
}

void
TracedTask::Run()
{
  LogStart(mTaskId, mSourceEventId);

  SetTraceInfo();
  mFactualObj->Run();
  ClearTraceInfo();

  LogEnd(mTaskId, mSourceEventId);
}

/**
 * Public functions of GeckoTaskTracer.
 *
 * CreateTracedRunnable() returns a new nsIRunnable pointer wrapped by
 * TracedRunnable, aRunnable is the original runnable object where now stored
 * by this TracedRunnable.
 *
 * CreateTracedTask() returns a new Task pointer wrapped by TracedTask, aTask
 * is the original task object where now stored by this TracedTask.
 */
nsIRunnable*
CreateTracedRunnable(nsIRunnable *aRunnable)
{
  if (IsInitialized()) {
    TracedRunnable* runnable = new TracedRunnable(aRunnable);
    return runnable;
  }

  return aRunnable;
}

Task*
CreateTracedTask(Task *aTask)
{
  if (IsInitialized()) {
    TracedTask* task = new TracedTask(aTask);
    return task;
  }

  return aTask;
}

} // namespace tasktracer
} // namespace mozilla
