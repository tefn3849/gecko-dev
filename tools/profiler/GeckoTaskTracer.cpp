/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoTaskTracer.h"
#include "GeckoTaskTracerImpl.h"

#include "mozilla/StaticMutex.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/unused.h"

#include "nsClassHashtable.h"
#include "nsThreadUtils.h"

#ifdef MOZILLA_INTERNAL_API
#include "nsString.h"
#else
#include "nsStringAPI.h"
#endif
#include "nsTArray.h"
#include "prtime.h"

#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#if defined(__GLIBC__)
// glibc doesn't implement gettid(2).
#include <sys/syscall.h>
static pid_t gettid()
{
  return (pid_t) syscall(SYS_gettid);
}
#endif

namespace mozilla {
namespace tasktracer {

static ThreadLocal<TraceInfo*>* sTraceInfoTLS = nullptr;
static StaticMutex sMutex;
static nsClassHashtable<nsUint32HashKey, TraceInfo>* sTraceInfos = nullptr;
static nsTArray<nsCString>* sLogs = nullptr; // WARNING: This is not thread-safe!
static bool sIsLoggingStarted = false;
static int sSavedPid = 0;

namespace {

static TraceInfo*
AllocTraceInfo(int aTid)
{
  StaticMutexAutoLock lock(sMutex);

  sTraceInfos->Put(aTid, new TraceInfo(sIsLoggingStarted));

  return sTraceInfos->Get(aTid);
}

static void
FreeTraceInfo(int aTid)
{
  StaticMutexAutoLock lock(sMutex);
  sTraceInfos->Remove(aTid);
}

static bool
IsInitialized()
{
  return (sTraceInfoTLS ? sTraceInfoTLS->initialized() : false);
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

static PLDHashOperator
SetStartLogging(const uint32_t& aThreadId,
                nsAutoPtr<TraceInfo>& aTraceInfo,
                void* aResult)
{
  aTraceInfo->mStartLogging = !aTraceInfo->mStartLogging;
  bool* result = static_cast<bool*>(aResult);
  *result = aTraceInfo->mStartLogging;

  return PL_DHASH_NEXT;
}

static void
CleanUp()
{
  if (sTraceInfos) {
    delete sTraceInfos;
    sTraceInfos = nullptr;
  }
  if (sTraceInfoTLS) {
    delete sTraceInfoTLS;
    sTraceInfoTLS = nullptr;
  }
  if (sLogs) {
    delete sLogs;
    sLogs = nullptr;
  }
}

} // namespace anonymous

void
InitTaskTracer()
{
  if (sSavedPid != getpid()) {
    CleanUp();
  }

  if (sTraceInfos || sTraceInfoTLS) {
    return;
  }

  sSavedPid = getpid();

  sTraceInfos = new nsClassHashtable<nsUint32HashKey, TraceInfo>();
  sTraceInfoTLS = new ThreadLocal<TraceInfo*>();
  sLogs = new nsTArray<nsCString>();

  if (!sTraceInfoTLS->initialized()) {
    unused << sTraceInfoTLS->init();
  }
}

void
ShutdownTaskTracer()
{
  CleanUp();
}

TraceInfo*
GetOrCreateTraceInfo()
{
  NS_ENSURE_TRUE(IsInitialized(), nullptr);

  TraceInfo* info = sTraceInfoTLS->get();
  if (!info) {
    info = AllocTraceInfo(gettid());
    sTraceInfoTLS->set(info);
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

AutoSaveCurTraceInfo::AutoSaveCurTraceInfo()
{
  SaveCurTraceInfo();
}

AutoSaveCurTraceInfo::~AutoSaveCurTraceInfo()
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
LogDispatch(uint64_t aTaskId, uint64_t aParentTaskId, uint64_t aSourceEventId,
            SourceEventType aSourceEventType)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!(info && info->mStartLogging && aSourceEventId)) {
    return;
  }

  // Log format:
  // [0 taskId dispatchTime sourceEventId sourceEventType parentTaskId]
  nsCString* log = sLogs->AppendElement();
  if (log) {
    log->AppendPrintf("%d %lld %lld %lld %d %lld",
                      ACTION_DISPATCH, aTaskId, PR_Now(), aSourceEventId,
                      aSourceEventType, aParentTaskId);
  }
}

void
LogBegin(uint64_t aTaskId, uint64_t aSourceEventId)
{
  TraceInfo* info = GetOrCreateTraceInfo();

  if (!(info && info->mStartLogging && aSourceEventId)) {
    return;
  }

  // Log format:
  // [1 taskId beginTime processId threadId]
  nsCString* log = sLogs->AppendElement();
  if (log) {
    log->AppendPrintf("%d %lld %lld %d %d",
                      ACTION_BEGIN, aTaskId, PR_Now(), getpid(), gettid());
  }
}

void
LogEnd(uint64_t aTaskId, uint64_t aSourceEventId)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!(info && info->mStartLogging && aSourceEventId)) {
    return;
  }

  // Log format:
  // [2 taskId endTime]
  nsCString* log = sLogs->AppendElement();
  if (log) {
    log->AppendPrintf("%d %lld %lld", ACTION_END, aTaskId, PR_Now());
  }
}

void
LogVirtualTablePtr(uint64_t aTaskId, uint64_t aSourceEventId, int* aVptr)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!(info && info->mStartLogging && aSourceEventId)) {
    return;
  }

  // Log format:
  // [4 taskId address]
  nsCString* log = sLogs->AppendElement();
  if (log) {
    log->AppendPrintf("%d %lld %p", ACTION_GET_VTABLE, aTaskId, aVptr);
  }
}

void
FreeTraceInfo()
{
  NS_ENSURE_TRUE_VOID(IsInitialized());

  FreeTraceInfo(gettid());
}

AutoSourceEvent::AutoSourceEvent(SourceEventType aType)
{
  CreateSourceEvent(aType);
}

AutoSourceEvent::~AutoSourceEvent()
{
  DestroySourceEvent();
}

void
AddLabel(const char* aFormat, ...)
{
  TraceInfo* info = GetOrCreateTraceInfo();
  if (!(info && info->mStartLogging)) {
    return;
  }

  va_list args;
  va_start(args, aFormat);
  nsAutoCString buffer;
  buffer.AppendPrintf(aFormat, args);
  va_end(args);

  // Log format:
  // [3 taskId "label"]
  nsCString* log = sLogs->AppendElement();
  if (log) {
    log->AppendPrintf("%d %lld %lld \"%s\"", ACTION_ADD_LABEL, info->mCurTaskId,
                      PR_Now(), buffer.get());
  }
}

// Functions that are used by GeckoProfiler.

bool
ToggleLogging()
{
  bool result = false;

  {
    StaticMutexAutoLock lock(sMutex);
    sTraceInfos->Enumerate(SetStartLogging, &result);
    sIsLoggingStarted = result;
  }

  return result;
}

nsTArray<nsCString>*
GetLoggedData(TimeStamp aStartTime)
{
  return sLogs;
}

} // namespace tasktracer
} // namespace mozilla
