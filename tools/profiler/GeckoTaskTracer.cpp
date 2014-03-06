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

#include "mozilla/dom/ContentChild.h"
#include "mozilla/ThreadLocal.h"
#include "nsThreadUtils.h"
#include "nsString.h"
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

#define MAX_THREAD_NUM 64
#define MAX_USER_LABEL_LEN 512

namespace mozilla {
namespace tasktracer {

static TraceInfo sAllTraceInfo[MAX_THREAD_NUM];
static mozilla::ThreadLocal<TraceInfo*> sTraceInfo;
static pthread_mutex_t sTraceInfoLock = PTHREAD_MUTEX_INITIALIZER;

static TraceInfo*
AllocTraceInfo(int aTid)
{
  pthread_mutex_lock(&sTraceInfoLock);
  for (int i = 0; i < MAX_THREAD_NUM; i++) {
    if (sAllTraceInfo[i].mThreadId == 0) {
      TraceInfo *info = sAllTraceInfo + i;
      info->mThreadId = aTid;
      info->mThreadName = nullptr;
      info->mProcessName = nullptr;
      pthread_mutex_unlock(&sTraceInfoLock);
      return info;
    }
  }

  NS_ABORT();
  return NULL;
}

static void
_FreeTraceInfo(uint64_t aTid)
{
  pthread_mutex_lock(&sTraceInfoLock);
  for (int i = 0; i < MAX_THREAD_NUM; i++) {
    if (sAllTraceInfo[i].mThreadId == aTid) {
      TraceInfo *info = sAllTraceInfo + i;
      delete info->mThreadName;
      delete info->mProcessName;
      memset(info, 0, sizeof(TraceInfo));
      break;
    }
  }
  pthread_mutex_unlock(&sTraceInfoLock);
}

void
InitTaskTracer()
{
  if (!sTraceInfo.initialized()) {
    sTraceInfo.init();
  }
}

bool
IsInitialized()
{
  return sTraceInfo.initialized();
}

TraceInfo*
GetTraceInfo()
{
  NS_ENSURE_TRUE(IsInitialized(), nullptr);

  if (!sTraceInfo.get()) {
    sTraceInfo.set(AllocTraceInfo(gettid()));
  }

  return sTraceInfo.get();
}

uint64_t
GenNewUniqueTaskId()
{
  TraceInfo* info = GetTraceInfo();
  NS_ENSURE_TRUE(info, 0);

  pid_t tid = gettid();
  uint64_t taskid = ((uint64_t)tid << 32) | ++info->mLastUniqueTaskId;
  return taskid;

}

void
SaveCurTraceInfo()
{
  TraceInfo* info = GetTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mSavedCurTraceSourceId = info->mCurTraceSourceId;
  info->mSavedCurTraceSourceType = info->mCurTraceSourceType;
  info->mSavedCurTaskId = info->mCurTaskId;
}

void
RestorePrevTraceInfo()
{
  TraceInfo* info = GetTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = info->mSavedCurTraceSourceId;
  info->mCurTraceSourceType = info->mSavedCurTraceSourceType;
  info->mCurTaskId = info->mSavedCurTaskId;
}

void
SetCurTraceInfo(uint64_t aSourceEventId, uint64_t aParentTaskId,
                uint32_t aSourceEventType)
{
  TraceInfo* info = GetTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  info->mCurTraceSourceId = aSourceEventId;
  info->mCurTaskId = aParentTaskId;
  info->mCurTraceSourceType = static_cast<SourceEventType>(aSourceEventType);
}

void
GetCurTraceInfo(uint64_t* aOutSourceEventId, uint64_t* aOutParentTaskId,
                uint32_t* aOutSourceEventType)
{
  TraceInfo* info = GetTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  *aOutSourceEventId = info->mCurTraceSourceId;
  *aOutParentTaskId = info->mCurTaskId;
  *aOutSourceEventType = static_cast<uint32_t>(info->mCurTraceSourceType);
}

void
SetThreadName(const char* aName)
{
  NS_ENSURE_TRUE_VOID(aName);

  TraceInfo* info = GetTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  if (info->mThreadName) {
    delete info->mThreadName;
    info->mThreadName = nullptr;
  }

  info->mThreadName = strdup(aName);
}

void
SetProcessName(const char* aName)
{
  NS_ENSURE_TRUE_VOID(aName);

  TraceInfo* info = GetTraceInfo();
  NS_ENSURE_TRUE_VOID(info);

  if (info->mProcessName) {
    delete info->mProcessName;
    info->mProcessName = nullptr;
  }

  info->mProcessName = strdup(aName);
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

  TraceInfo* info = GetTraceInfo();

  if (!info->mProcessName) {
    if (XRE_GetProcessType() == GeckoProcessType_Default) {
      info->mProcessName = strdup("b2g");
    } else if (mozilla::dom::ContentChild *cc =
               mozilla::dom::ContentChild::GetSingleton()) {
      nsAutoCString process;
      cc->GetProcessName(process);
      info->mProcessName = strdup(process.get());
    } else {
      info->mProcessName = strdup("");
    }
  }

  if (!info->mThreadName) {
    if (getpid() == gettid()) {
      info->mThreadName = strdup("main");
    } else if (const char* name = PR_GetThreadName(PR_GetCurrentThread())) {
      info->mThreadName = strdup(name);
    } else {
      info->mThreadName = strdup("");
    }
  }

  // Log format:
  // [1 taskId beginTime processId threadId "threadName"]
  TTLOG("%d %lld %lld %d \"%s\" %d \"%s\"",
        ACTION_BEGIN, aTaskId, PR_Now(), getpid(), info->mProcessName,
        gettid(), info->mThreadName);
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

  _FreeTraceInfo(gettid());
}

void
CreateSourceEvent(SourceEventType aType)
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Save the currently traced source event info.
  SaveCurTraceInfo();

  // Create a new unique task id.
  uint64_t newId = GenNewUniqueTaskId();
  TraceInfo* info = GetTraceInfo();
  info->mCurTraceSourceId = newId;
  info->mCurTraceSourceType = aType;
  info->mCurTaskId = newId;

  // Log a fake dispatch and start for this source event.
  LogDispatch(newId, newId,newId, aType);
  LogBegin(newId, newId);
}

void
DestroySourceEvent()
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  // Log a fake end for this source event.
  TraceInfo* info = GetTraceInfo();
  LogEnd(info->mCurTraceSourceId, info->mCurTraceSourceId);

  // Restore the previously saved source event info.
  RestorePrevTraceInfo();
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
  TraceInfo* info = GetTraceInfo();
  TTLOG("%d %lld %lld \"%s\"",
        ACTION_ADD_LABEL, info->mCurTaskId, PR_Now(), buffer);
}

} // namespace tasktracer
} // namespace mozilla
