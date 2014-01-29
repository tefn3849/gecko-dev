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

#ifdef PR_LOGGING
static PRLogModuleInfo* gTaskTracerLog = nullptr;
//#define TTLOG(msg) PR_LOG(gTaskTracerLog, PR_LOG_DEBUG, msg)
#else
//#define TTLOG(msg)
#endif

//static void
//TTLOG(const char * aFormat, ...)
//{
//  nsCString fmt;
//  fmt.Append(NS_LITERAL_CSTRING("TaskTracer: "));
//  fmt.AppendASCII(aFormat);
//  fmt.Append(NS_LITERAL_CSTRING("\n"));
//  va_list args;
//  va_start(args, fmt.get());
//  vfprintf(stderr, fmt.get(), args);
//  va_end(args);
//}

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

void
LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId, uint64_t aSourceEventId,
            SourceEventType aSourceEventType)
{
  if (!IsInitialized() || !aSourceEventId) {
    return;
  }

  // Log format for Dispatch action:
  // -------
  // actionType taskId dispatchTime sourceEventId sourceEventType parentTaskId
  TTLOG("%d %lld %lld %lld %d %lld",
        ACTION_DISPATCH, aTaskId, PR_Now(), aSourceEventId, aSourceEventType,
        aParentTaskId);
}

void
LogStart(uint64_t aTaskId, uint64_t aSourceEventId)
{
  if (!IsInitialized() || !aSourceEventId) {
    return;
  }

  // Log format for Start action:
  // -------
  // actionType taskId startTime processId threadId
  TTLOG("%d %lld %lld %d %d",
        ACTION_START, aTaskId, PR_Now(), getpid(), gettid());
}

void
LogEnd(uint64_t aTaskId, uint64_t aSourceEventId)
{
  if (!IsInitialized() || !aSourceEventId) {
    return;
  }

  // Log format for End action:
  // -------
  // actionType taskId endTime
  TTLOG("%d %lld %lld", ACTION_END, aTaskId, PR_Now());
}

void
LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, int* aVptr)
{
  if (!IsInitialized()|| !aSourceEventId) {
    return;
  }

  // Log format for address of virtual table of its factual object:
  // -------
  // actionType taskId vPtr_of_factual_obj
  TTLOG("%d %lld %p", ACTION_GET_VTABLE, aTaskId, aVptr);
}

void
FreeTraceInfo()
{
  if (!IsInitialized()) {
    return;
  }

  _FreeTraceInfo(gettid());
}

static uint64_t
InitSourceEvent(SourceEventType aType)
{
  if (!IsInitialized()) {
    return 0;
  }

  TraceInfo* info = GetTraceInfo();
  info->mCurTraceTaskId = GenNewUniqueTaskId();
  info->mCurTraceTaskType = aType;
  info->mParentTaskId = info->mCurTraceTaskId;
  info->mCurTaskId = info->mCurTraceTaskId;
  return info->mCurTraceTaskId;
}

void
CreateSourceEvent(SourceEventType aType, int aX, int aY)
{
  uint64_t soueceEventId = InitSourceEvent(aType);

  // Log format for creating source event with custom info:
  // -------
  // actionType sourceEventId createTime x y
  // -------
  TTLOG("%d %lld %lld %d %d",
        ACTION_CREATE, soueceEventId, PR_Now(), aX, aY);
}

void
CreateSourceEvent(SourceEventType aType)
{
  uint64_t soueceEventId = InitSourceEvent(aType);

  // -------
  // actionType sourceEventId createTime
  // -------
  TTLOG("%d %lld %lld",
        ACTION_CREATE, soueceEventId, PR_Now());
}

void AddLabel(const char * aFormat, ...)
{
  if (!IsInitialized()) {
    return;
  }

  va_list args;
  va_start(args, aFormat);
  char buffer[MAX_USER_LABEL_LEN];
  vsnprintf(buffer, MAX_USER_LABEL_LEN, aFormat, args);
  va_end(args);

  // -------
  // actionType curTaskId curTime message
  // -------
  TraceInfo* info = GetTraceInfo();
  TTLOG("%d %lld %lld \"%s\"",
        ACTION_ADD_LABEL, info->mCurTaskId, PR_Now(), buffer);
}

void
SaveCurTraceInfo()
{
  if (IsInitialized()) {
    TraceInfo* info = GetTraceInfo();
    info->mSavedCurTraceTaskId = info->mCurTraceTaskId;
    info->mSavedParentTaskId = info->mParentTaskId;
    info->mSavedCurTraceTaskType = info->mCurTraceTaskType;
  }
}

void
RestorePrevTraceInfo()
{
  if (IsInitialized()) {
    TraceInfo* info = GetTraceInfo();
    info->mCurTraceTaskId = info->mSavedCurTraceTaskId;
    info->mParentTaskId = info->mSavedParentTaskId;
    info->mCurTraceTaskType = info->mSavedCurTraceTaskType;
  }
}

void
SetCurTraceInfo(uint64_t aTaskId, uint64_t aParentTaskId, uint32_t aType)
{
  if (IsInitialized()) {
    TraceInfo* info = GetTraceInfo();
    info->mCurTraceTaskId = aTaskId;
    info->mParentTaskId = aParentTaskId;
    info->mCurTraceTaskType = static_cast<SourceEventType>(aType);
  }
}

void
GetCurTraceInfo(uint64_t* aOutputTaskId, uint64_t* aOutputParentTaskId,
                uint32_t* aOutputType)
{
  if (IsInitialized()) {
    TraceInfo* info = GetTraceInfo();
    *aOutputTaskId = info->mCurTraceTaskId;
    *aOutputParentTaskId = info->mParentTaskId;
    *aOutputType = static_cast<uint32_t>(info->mCurTraceTaskType);
  }
}

bool
IsCurTracTaskValid()
{
  TraceInfo* info = GetTraceInfo();
  return (info && info->mCurTraceTaskId);
}

} // namespace tasktracer
} // namespace mozilla
