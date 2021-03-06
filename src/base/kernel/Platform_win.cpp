/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2018-2020 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2020 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <algorithm>
#include <winsock2.h>
#include <windows.h>
#include <uv.h>


#include "base/kernel/Platform.h"
#include "base/tools/Chrono.h"
#include "base/io/log/Log.h"
#include "version.h"


#ifdef XMRIG_NVIDIA_PROJECT
#   include "nvidia/cryptonight.h"
#endif


#ifdef XMRIG_AMD_PROJECT
static uint32_t timerResolution = 0;
#endif

static inline OSVERSIONINFOEX winOsVersion()
{
    typedef NTSTATUS (NTAPI *RtlGetVersionFunction)(LPOSVERSIONINFO);
    OSVERSIONINFOEX result = { sizeof(OSVERSIONINFOEX), 0, 0, 0, 0, {'\0'}, 0, 0, 0, 0, 0};

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll ) {
        auto pRtlGetVersion = reinterpret_cast<RtlGetVersionFunction>(GetProcAddress(ntdll, "RtlGetVersion"));

        if (pRtlGetVersion) {
            pRtlGetVersion(reinterpret_cast<LPOSVERSIONINFO>(&result));
        }
    }

    return result;
}


char *xmrig::Platform::createUserAgent()
{
    const auto osver = winOsVersion();
    constexpr const size_t max = 256;

    char *buf = new char[max]();
    int length = snprintf(buf, max, "%s/%s (Windows NT %lu.%lu", APP_NAME, APP_VERSION, osver.dwMajorVersion, osver.dwMinorVersion);

#   if defined(__x86_64__) || defined(_M_AMD64)
    length += snprintf(buf + length, max - length, "; Win64; x64) libuv/%s", uv_version_string());
#   else
    length += snprintf(buf + length, max - length, ") libuv/%s", uv_version_string());
#   endif

#   ifdef XMRIG_NVIDIA_PROJECT
    const int cudaVersion = cuda_get_runtime_version();
    length += snprintf(buf + length, max - length, " CUDA/%d.%d", cudaVersion / 1000, cudaVersion % 100);
#   endif

#   ifdef __GNUC__
    length += snprintf(buf + length, max - length, " gcc/%d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#   elif _MSC_VER
    length += snprintf(buf + length, max - length, " msvc/%d", MSVC_VERSION);
#   endif

    return buf;
}


#ifndef XMRIG_FEATURE_HWLOC
bool xmrig::Platform::setThreadAffinity(uint64_t cpu_id)
{
    if (cpu_id >= 64) {
        LOG_ERR("Unable to set affinity. Windows supports only affinity up to 63.");
    }

    const bool result = (SetThreadAffinityMask(GetCurrentThread(), 1ULL << cpu_id) != 0);
    Sleep(1);
    return result;
}
#endif


uint32_t xmrig::Platform::setTimerResolution(uint32_t resolution)
{
#   ifdef XMRIG_AMD_PROJECT
    TIMECAPS tc;

    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
        return 0;
    }

    timerResolution = std::min<uint32_t>(std::max<uint32_t>(tc.wPeriodMin, resolution), tc.wPeriodMax);

    return timeBeginPeriod(timerResolution) == TIMERR_NOERROR ? timerResolution : 0;
#   else
    return resolution;
#   endif
}


void xmrig::Platform::restoreTimerResolution()
{
#   ifdef XMRIG_AMD_PROJECT
    if (timerResolution) {
        timeEndPeriod(timerResolution);
    }
#   endif
}


void xmrig::Platform::setProcessPriority(int priority)
{
    if (priority == -1) {
        return;
    }

    DWORD prio = IDLE_PRIORITY_CLASS;
    switch (priority)
    {
    case 1:
        prio = BELOW_NORMAL_PRIORITY_CLASS;
        break;

    case 2:
        prio = NORMAL_PRIORITY_CLASS;
        break;

    case 3:
        prio = ABOVE_NORMAL_PRIORITY_CLASS;
        break;

    case 4:
        prio = HIGH_PRIORITY_CLASS;
        break;

    case 5:
        prio = REALTIME_PRIORITY_CLASS;
        break;

    default:
        break;
    }

    SetPriorityClass(GetCurrentProcess(), prio);
}


void xmrig::Platform::setThreadPriority(int priority)
{
    if (priority == -1) {
        return;
    }

    int prio = THREAD_PRIORITY_IDLE;
    switch (priority)
    {
    case 1:
        prio = THREAD_PRIORITY_BELOW_NORMAL;
        break;

    case 2:
        prio = THREAD_PRIORITY_NORMAL;
        break;

    case 3:
        prio = THREAD_PRIORITY_ABOVE_NORMAL;
        break;

    case 4:
        prio = THREAD_PRIORITY_HIGHEST;
        break;

    case 5:
        prio = THREAD_PRIORITY_TIME_CRITICAL;
        break;

    default:
        break;
    }

    SetThreadPriority(GetCurrentThread(), prio);
}

int64_t xmrig::Platform::getThreadSleepTimeToLimitMaxCpuUsage(uint8_t maxCpuUsage)
{
  uint64_t currentSystemTime = Chrono::highResolutionMicroSecs();
  if (currentSystemTime - m_systemTime > MIN_RECALC_THRESHOLD_USEC)
  {
	  FILETIME kernelTime, userTime, creationTime, exitTime;
	  if(GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime, &userTime))
	  {
      ULARGE_INTEGER kTime, uTime;
      kTime.LowPart = kernelTime.dwLowDateTime;
      kTime.HighPart = kernelTime.dwHighDateTime;
      uTime.LowPart = userTime.dwLowDateTime;
      uTime.HighPart = userTime.dwHighDateTime;

      int64_t currentThreadUsageTime = (kTime.QuadPart / 10)
                                     + (uTime.QuadPart / 10);

      if (m_threadUsageTime > 0 || m_systemTime > 0)
      {
        m_threadTimeToSleep = ((currentThreadUsageTime - m_threadUsageTime) * 100 / maxCpuUsage)
                            - (currentSystemTime - m_systemTime - m_threadTimeToSleep);
      }

        m_threadUsageTime = currentThreadUsageTime;
        m_systemTime = currentSystemTime;
	  }

	  // Something went terrible wrong, reset everything
	  if (m_threadTimeToSleep > 10000000 || m_threadTimeToSleep < 0)
	  {
      m_threadTimeToSleep = 0;
      m_threadUsageTime = 0;
      m_systemTime = 0;
	  }

	  return m_threadTimeToSleep;
  }

  return 0;
}