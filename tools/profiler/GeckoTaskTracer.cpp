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
#include "nsThreadUtils.h"
#include "prenv.h"
#include "prlog.h"
#include "prthread.h"
#include "prtime.h"

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef PR_LOGGING
PRLogModuleInfo* gTaskTracerLog;
#define TT_LOG(type, msg) PR_LOG(gTaskTracerLog, type, msg)
#else
#define TT_LOG(type, msg)
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

namespace mozilla {
namespace tasktracer {

static bool sInitialized = false;
static bool sStarted = false;
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
      memset(info, 0, sizeof(TraceInfo));
      break;
    }
  }
  pthread_mutex_unlock(&sTraceInfoLock);
}

static const char*
GetCurrentThreadName()
{
  if (gettid() == getpid()) {
    return "main";
  } else if (const char *threadName = PR_GetThreadName(PR_GetCurrentThread())) {
    return threadName;
  } else {
    return "unknown";
  }
}

void
InitTaskTracer()
{
  if (sInitialized) {
    return;
  }

  sInitialized = true;

  if (!sTraceInfo.initialized()) {
    sTraceInfo.init();
  }
}

void
StartTaskTracer()
{
  if (!sInitialized || (sInitialized && sStarted)) {
    return;
  }

  sStarted = true;

#ifdef PR_LOGGING
  if (!gTaskTracerLog) {
    gTaskTracerLog = PR_NewLogModule("TaskTracer");
  }
#endif
}

bool
IsStarted()
{
  return sStarted;
}

TraceInfo*
GetTraceInfo()
{
  if (!sTraceInfo.get()) {
    sTraceInfo.set(AllocTraceInfo(gettid()));
  }
  return sTraceInfo.get();
}

uint64_t
GenNewUniqueTaskId()
{
  pid_t tid = gettid();
  uint64_t taskid = ((uint64_t)tid << 32) | ++GetTraceInfo()->mLastUniqueTaskId;
  return taskid;
}

void SetCurTraceId(uint64_t aTaskId)
{
  TraceInfo* info = GetTraceInfo();
  info->mCurTraceTaskId = aTaskId;
}

uint64_t GetCurTraceId()
{
  TraceInfo* info = GetTraceInfo();
  return info->mCurTraceTaskId;
}

void SetCurTraceType(SourceEventType aType)
{
  TraceInfo* info = GetTraceInfo();
  info->mCurTraceTaskType = aType;
}

SourceEventType GetCurTraceType()
{
  TraceInfo* info = GetTraceInfo();
  return info->mCurTraceTaskType;
}

void
LogTaskAction(ActionType aActionType, uint64_t aTaskId, uint64_t aSourceEventId,
              SourceEventType aSourceEventType)
{
  if (!aSourceEventId) {
    return;
  }

  // taskId | sourceEventId | processId | threadId
  // actionType | timestamp | sourceEventType
  TT_LOG(PR_LOG_DEBUG, ("%lld %lld %ld %ld %d %lld %d",
                        aTaskId, aSourceEventId, getpid(), gettid(),
                        aActionType, PR_Now(), aSourceEventType));
}

void
FreeTraceInfo()
{
  if (!sInitialized) {
    return;
  }

  _FreeTraceInfo(gettid());
}

void
CreateSETouch(int aX, int aY)
{
  if (!sStarted) {
    return;
  }

  TraceInfo* info = GetTraceInfo();
  info->mCurTraceTaskId = GenNewUniqueTaskId();
  info->mCurTraceTaskType = SourceEventType::TOUCH;

  TT_LOG(PR_LOG_DEBUG, ("%lld %lld %ld %ld %d %lld %d %d %d",
                        info->mCurTraceTaskId, info->mCurTraceTaskId, getpid(),
                        gettid(), ActionType::ACTION_DISPATCH, PR_Now(),
                        SourceEventType::TOUCH, aX, aY));
}

void SaveCurTraceInfo()
{
  TraceInfo* info = GetTraceInfo();
  info->mSavedTraceTaskId = info->mCurTraceTaskId;
  info->mSavedTraceTaskType = info->mCurTraceTaskType;
}

void RestorePrevTraceInfo()
{
  TraceInfo* info = GetTraceInfo();
  info->mCurTraceTaskId = info->mSavedTraceTaskId;
  info->mCurTraceTaskType = info->mSavedTraceTaskType;
}

} // namespace tasktracer
} // namespace mozilla
