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
#include "nsString.h"
#include "prenv.h"
#include "prlog.h"
#include "prthread.h"
#include "prtime.h"

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef MOZ_WIDGET_GONK
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "TaskTracer", args)
#else
#define LOG(args...)
#endif

#ifdef PR_LOGGING
static PRLogModuleInfo* gTaskTracerLog = nullptr;
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
  if (!sTraceInfo.initialized()) {
    sTraceInfo.init();
  }

#ifdef PR_LOGGING
  if (!gTaskTracerLog) {
    //PR_SetEnv("NSPR_LOG_MODULES=TaskTracer:5");
    gTaskTracerLog = PR_NewLogModule("TaskTracer");
  }
#endif
}

static bool sStarted = false;
void
StartTaskTracer()
{
  if (sStarted) {
    return;
  }
  sStarted = true;

/*
#ifdef PR_LOGGING
  if (gTaskTracerLog) {
    char fileName[256];
    snprintf(fileName, 256, "/sdcard/tt-%d.log", getpid());
    PR_SetLogFile(fileName);
  }
#endif
*/
}
bool
IsInitialized()
{
  return sTraceInfo.initialized();
}

TraceInfo*
GetTraceInfo()
{
  if (!IsInitialized()) {
    return nullptr;
  }

  if (!sTraceInfo.get()) {
    sTraceInfo.set(AllocTraceInfo(gettid()));
  }
  return sTraceInfo.get();
}

uint64_t
GenNewUniqueTaskId()
{
  pid_t tid = gettid();
  if (IsInitialized()) {
    uint64_t taskid = ((uint64_t)tid << 32) | ++GetTraceInfo()->mLastUniqueTaskId;
    return taskid;
  } else {
    return 0;
  }
}

void SetCurTraceId(uint64_t aTaskId)
{
  if (IsInitialized()) {
    TraceInfo* info = GetTraceInfo();
    info->mCurTraceTaskId = aTaskId;
  }
}

uint64_t GetCurTraceId()
{
  TraceInfo* info = GetTraceInfo();
  return (info) ? info->mCurTraceTaskId : 0;
}

void SetCurTraceType(SourceEventType aType)
{
  if (IsInitialized()) {
    TraceInfo* info = GetTraceInfo();
    info->mCurTraceTaskType = aType;
  }
}

SourceEventType GetCurTraceType()
{
  TraceInfo* info = GetTraceInfo();
  return (info) ? info->mCurTraceTaskType : SourceEventType::UNKNOWN;
}

void
LogTaskAction(ActionType aActionType, uint64_t aTaskId, uint64_t aSourceEventId,
              SourceEventType aSourceEventType)
{
  if (!IsInitialized() || !aSourceEventId) {
    return;
  }

  // taskId | sourceEventId | sourceEventType | processId | threadId
  // actionType | timestamp
  TT_LOG(PR_LOG_DEBUG, ("%lld %lld %d %ld %ld %d %lld",
                        aTaskId, aSourceEventId, aSourceEventType, getpid(), gettid(),
                        aActionType, PR_Now()));

  LOG("%lld %lld %d %d %d %d %lld",
      aTaskId, aSourceEventId, aSourceEventType, getpid(), gettid(),
      aActionType, PR_Now());
}

void
FreeTraceInfo()
{
  if (!IsInitialized()) {
    return;
  }

  _FreeTraceInfo(gettid());
}

void
CreateSETouch(int aX, int aY)
{
  if (!IsInitialized()) {
    return;
  }

  TraceInfo* info = GetTraceInfo();
  info->mCurTraceTaskId = GenNewUniqueTaskId();
  info->mCurTraceTaskType = SourceEventType::TOUCH;

  TT_LOG(PR_LOG_DEBUG, ("%lld %lld  %d %d %d %d %lld %d %d",
                        info->mCurTraceTaskId, info->mCurTraceTaskId,
                        SourceEventType::TOUCH, getpid(),
                        gettid(), ActionType::ACTION_DISPATCH, PR_Now(), aX, aY));

  LOG("%lld %lld  %d %d %d %d %lld %d %d",
      info->mCurTraceTaskId, info->mCurTraceTaskId,
      SourceEventType::TOUCH, getpid(),
      gettid(), ActionType::ACTION_DISPATCH, PR_Now(), aX, aY);
}

void SaveCurTraceInfo()
{
  if (IsInitialized()) {
    TraceInfo* info = GetTraceInfo();
    info->mSavedTraceTaskId = info->mCurTraceTaskId;
    info->mSavedTraceTaskType = info->mCurTraceTaskType;
  }
}

void RestorePrevTraceInfo()
{
  if (IsInitialized()) {
    TraceInfo* info = GetTraceInfo();
    info->mCurTraceTaskId = info->mSavedTraceTaskId;
    info->mCurTraceTaskType = info->mSavedTraceTaskType;
  }
}

} // namespace tasktracer
} // namespace mozilla
