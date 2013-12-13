/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TracedRunnable.h"

namespace mozilla {
namespace tasktracer {

// Implementing TracedRunnable.h

NS_IMPL_ISUPPORTS1(TracedRunnable, nsIRunnable)

NS_IMETHODIMP
TracedRunnable::Run()
{
    LogTaskAction(ACTION_START, mTaskId, mSourceEvent);

    AttachTracedInfo();
    nsresult rv = mFactualObj->Run();
    ClearTracedInfo();

    LogTaskAction(ACTION_FINISHED, mTaskId, mSourceEvent);
    return rv;
}

TracedRunnable::TracedRunnable(nsIRunnable *aFatualObj)
    : mOriginTaskId(0)
    , mFactualObj(aFatualObj)
{
    mTaskId = GenNewUniqueTaskId();
}

TracedRunnable::~TracedRunnable()
{
}

void
TracedRunnable::InitOriginTaskId()
{
    uint64_t origintaskid = *GetCurrentThreadTaskIdPtr();
    SetOriginTaskId(origintaskid);

    LogAction(ACTION_DISPATCH, mTaskId, mOriginTaskId);
}

void
TracedRunnable::InitSourceEvent()
{
  TracedInfo* info = GetTracedInfo();
  if(!info->currentlyTracedSourceEvent) {
    mozilla::TemporaryRef<SourceEventDummy> source = new SourceEventDummy();
    CreateCurrentlyTracedSourceEvent(source);
  }
  LogTaskAction(ACTION_DISPATCH, mTaskId, mSourceEvent);
}

void
TracedRunnable::AttachTracedInfo()
{
  //TODO: remove this later
  *GetCurrentThreadTaskIdPtr() = GetOriginTaskId();
  SetCurrentlyTracedSourceEvent(mSourceEvent);
}

void
TracedRunnable::ClearTracedInfo()
{
  SetCurrentlyTracedSourceEvent(nullptr);
  mSourceEvent->Release();
}

TracedTask::~TracedTask()
{
  if (mBackedObject) {
    delete mBackedObject;
    mBackedObject = nullptr;
  }
}

void
TracedTask::InitOriginTaskId()
{
  uint64_t originTaskId = *GetCurrentThreadTaskIdPtr();
//    if (originTaskId == 0) {
//        originTaskId = mTaskId;
//    }
  mOriginTaskId = originTaskId;

  LogAction(ACTION_DISPATCH, mTaskId, mOriginTaskId);
}

void
TracedTask::InitSourceEvent()
{
  TracedInfo* info = GetTracedInfo();
  if(!info->currentlyTracedSourceEvent) {
    mozilla::TemporaryRef<SourceEventDummy> source = new SourceEventDummy();
    CreateCurrentlyTracedSourceEvent(source);
  }
  LogTaskAction(ACTION_DISPATCH, mTaskId, mSourceEvent);
}

void
TracedTask::Run()
{
  LogTaskAction(ACTION_START, mTaskId, mSourceEvent);

  AttachTracedInfo();
  mBackedObject->Run();
  ClearTracedInfo();

  LogTaskAction(ACTION_FINISHED, mTaskId, mSourceEvent);
}

void
TracedTask::AttachTracedInfo()
{
  //TODO: remove this later
  *GetCurrentThreadTaskIdPtr() = mOriginTaskId;
  SetCurrentlyTracedSourceEvent(mSourceEvent);
}

void
TracedTask::ClearTracedInfo()
{
  SetCurrentlyTracedSourceEvent(nullptr);
  mSourceEvent->Release();
}

// Implementing GeckoTaskTracer.h

nsIRunnable *
CreateTracedRunnable(nsIRunnable *aRunnable)
{
  TracedRunnable *runnable = new TracedRunnable(aRunnable);
  runnable->InitSourceEvent();
  return runnable;
}

Task *
CreateTracedTask(Task *aTask)
{
  TracedTask *task = new TracedTask(aTask);
  task->InitSourceEvent();
  return task;
}

} // namespace tasktracer
} // namespace mozilla
