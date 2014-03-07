/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "GeckoTaskTracer.h"
#include "GeckoTaskTracerImpl.h"

#include "mozilla/ThreadLocal.h"
#include "mozilla/StaticMutex.h"

#include "nsAutoPtr.h"
#include "nsThreadUtils.h"
#include "nsTArray.h"
#include "nsXULAppAPI.h"
#include "prenv.h"
#include "prlog.h"
#include "prthread.h"
#include "prtime.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define TTLOG(args...) __android_log_print(ANDROID_LOG_INFO, "TaskTracer", args)
#else
#define TTLOG(args...)
#endif

#if defined(__GLIBC__)
// glibc doesn't implement gettid(2).
#include <sys/syscall.h>
static pid_t gettid()
{
  return (pid_t) syscall(SYS_gettid);
}
#endif

#define MAX_USER_LABEL_LEN 512

namespace mozilla {
namespace tasktracer {

static mozilla::ThreadLocal<TraceInfo*> sTraceInfoTLS;
static StaticMutex sMutex;
static nsTArray<nsAutoPtr<TraceInfo> > sTraceInfos;

namespace {

static TraceInfo*
AllocTraceInfo(int aTid)
{
  StaticMutexAutoLock lock(sMutex);

  TraceInfo* aInfo = new TraceInfo();
  aInfo->mThreadId = aTid;
  aInfo->mThreadName.SetIsVoid(true);

  nsAutoPtr<TraceInfo>* info = sTraceInfos.AppendElement(aInfo);

  if (!info->get()) {
    NS_ABORT();
    return nullptr;
  }

  return info->get();
}

static void
FreeTraceInfo(int aTid)
{
  StaticMutexAutoLock lock(sMutex);

  int i = sTraceInfos.Length();
  while (i > 0) {
    i--;
    if (sTraceInfos[i]->mThreadId == aTid) {
      sTraceInfos.RemoveElementAt(i);
      break;
    }
  }

}

static bool
IsInitialized()
{
  return sTraceInfoTLS.initialized();
}

static void
SaveCurTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mSavedCurTraceSourceId = info->mCurTraceSourceId;
  info->mSavedCurTraceSourceType = info->mCurTraceSourceType;
  info->mSavedCurTaskId = info->mCurTaskId;
}

static void
RestoreCurTraceInfo()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = info->mSavedCurTraceSourceId;
  info->mCurTraceSourceType = info->mSavedCurTraceSourceType;
  info->mCurTaskId = info->mSavedCurTaskId;
}

static void
CreateSourceEvent(SourceEventType aType)
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Save the currently traced source event info.
  SaveCurTraceInfo();

  // Create a new unique task id.
  uint64_t newId = GenNewUniqueTaskId();
  TraceInfo* info = GetOrCreateTraceInfo();
  info->mCurTraceSourceId = newId;
  info->mCurTraceSourceType = aType;
  info->mCurTaskId = newId;

  // Log a fake dispatch and start for this source event.
  LogDispatch(newId, newId,newId, aType);
  LogBegin(newId, newId);
}

static void
DestroySourceEvent()
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Log a fake end for this source event.
  TraceInfo* info = GetOrCreateTraceInfo();
  LogEnd(info->mCurTraceSourceId, info->mCurTraceSourceId);

  // Restore the previously saved source event info.
  RestoreCurTraceInfo();
}
} // namespace anonymous

void
InitTaskTracer()
{
  if (!sTraceInfoTLS.initialized()) {
    sTraceInfoTLS.init();
  }
}

TraceInfo*
GetOrCreateTraceInfo()
{
  NS_ENSURE_TRUE(IsInitialized(), nullptr);

  TraceInfo* info = sTraceInfoTLS.get();
  if (!info) {
    info = AllocTraceInfo(gettid());
    sTraceInfoTLS.set(info);
  }

  return info;
}

uint64_t
GenNewUniqueTaskId()
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE(info, 0);

  pid_t tid = gettid();
  uint64_t taskid = ((uint64_t)tid << 32) | ++info->mLastUniqueTaskId;
  return taskid;
}

SaveCurTraceInfoRAII::SaveCurTraceInfoRAII()
{
  SaveCurTraceInfo();
}

SaveCurTraceInfoRAII::~SaveCurTraceInfoRAII()
{
  RestoreCurTraceInfo();
}

void
SetCurTraceInfo(uint64_t aSourceEventId, uint64_t aParentTaskId,
                SourceEventType aSourceEventType)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = aSourceEventId;
  info->mCurTaskId = aParentTaskId;
  info->mCurTraceSourceType = aSourceEventType;
}

void
GetCurTraceInfo(uint64_t* aOutSourceEventId, uint64_t* aOutParentTaskId,
                SourceEventType* aOutSourceEventType)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  *aOutSourceEventId = info->mCurTraceSourceId;
  *aOutParentTaskId = info->mCurTaskId;
  *aOutSourceEventType = info->mCurTraceSourceType;
}

void
SetThreadName(const char* aName, int aType)
{
  NS_ENSURE_TRUE_VOID(aName);

  if (aType == TYPE_PROCESS) {
    NS_ENSURE_TRUE_VOID(NS_IsMainThread());
  }

  TraceInfo* info = GetOrCreateTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mThreadName.Assign(aName);
  info->mThreadName.SetIsVoid(false);
}

void
LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId, uint64_t aSourceEventId,
            SourceEventType aSourceEventType)
{
  NS_ENSURE_TRUE_VOID(IsInitialized() && aSourceEventId);

  // Log format:
  // [0 taskId dispatchTime sourceEventId sourceEventType parentTaskId]
  TTLOG("%d %lld %lld %lld %d %lld",
        ACTION_DISPATCH, aTaskId, PR_Now(), aSourceEventId, aSourceEventType,
        aParentTaskId);
}

void
LogBegin(uint64_t aTaskId, uint64_t aSourceEventId)
{
  NS_ENSURE_TRUE_VOID(IsInitialized() && aSourceEventId);

  TraceInfo* info = GetOrCreateTraceInfo();
  if (info->mThreadName.IsVoid()) {
    info->mThreadName = EmptyCString();
    if (getpid() == gettid()) {
      if (XRE_GetProcessType() == GeckoProcessType_Default) {
        info->mThreadName.AppendASCII("b2g");
      }
    } else if (const char* name = PR_GetThreadName(PR_GetCurrentThread())) {
      info->mThreadName.Append(name);
    }
    info->mThreadName.SetIsVoid(false);
  }

  // Log format:
  // [1 taskId beginTime processId threadId "threadName"]
  TTLOG("%d %lld %lld %d %d \"%s\"",
        ACTION_BEGIN, aTaskId, PR_Now(), getpid(), gettid(),
        info->mThreadName.get());
}

void
LogEnd(uint64_t aTaskId, uint64_t aSourceEventId)
{
  NS_ENSURE_TRUE_VOID(IsInitialized() && aSourceEventId);

  // Log format:
  // [2 taskId endTime]
  TTLOG("%d %lld %lld", ACTION_END, aTaskId, PR_Now());
}

void
LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, int* aVptr)
{
  NS_ENSURE_TRUE_VOID(IsInitialized() && aSourceEventId);

  // Log format:
  // [4 taskId address]
  TTLOG("%d %lld %p", ACTION_GET_VTABLE, aTaskId, aVptr);
}

void
FreeTraceInfo()
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  FreeTraceInfo(gettid());
}

CreateSourceEventRAII::CreateSourceEventRAII(SourceEventType aType)
{
  CreateSourceEvent(aType);
}

CreateSourceEventRAII::~CreateSourceEventRAII()
{
  DestroySourceEvent();
}

void AddLabel(const char* aFormat, ...)
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  va_list args;
  va_start(args, aFormat);
  char buffer[MAX_USER_LABEL_LEN] = {0};
  vsnprintf(buffer, MAX_USER_LABEL_LEN, aFormat, args);
  va_end(args);

  // Log format:
  // [3 taskId "label"]
  TraceInfo* info = GetOrCreateTraceInfo();
  TTLOG("%d %lld %lld \"%s\"",
        ACTION_ADD_LABEL, info->mCurTaskId, PR_Now(), buffer);
}

} // namespace tasktracer
} // namespace mozilla
