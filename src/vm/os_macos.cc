// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "vm/globals.h"
#if defined(TARGET_OS_MACOS)

#include "vm/os.h"

#include <errno.h>
#include <mach/mach_time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "vm/assert.h"

namespace psoup {

static mach_timebase_info_data_t timebase_info;


void OS::InitOnce() {
  kern_return_t kr = mach_timebase_info(&timebase_info);
  ASSERT(KERN_SUCCESS == kr);
}


int64_t OS::CurrentMonotonicMicros() {
  ASSERT(timebase_info.denom != 0);
  // timebase_info converts absolute time tick units into nanoseconds.
  int64_t result = mach_absolute_time();
  result *= timebase_info.numer;
  result /= timebase_info.denom;
  return result / kNanosecondsPerMicrosecond;
}


int64_t OS::CurrentMonotonicMillis() {
  ASSERT(timebase_info.denom != 0);
  // timebase_info converts absolute time tick units into nanoseconds.
  int64_t result = mach_absolute_time();
  result *= timebase_info.numer;
  result /= timebase_info.denom;
  return result / kNanosecondsPerMillisecond;
}


int OS::NumberOfAvailableProcessors() {
  return sysconf(_SC_NPROCESSORS_ONLN);
}


void OS::SleepMicros(int64_t micros) {
  struct timespec req;  // requested.
  struct timespec rem;  // remainder.
  int64_t seconds = micros / kMicrosecondsPerSecond;
  if (seconds > kMaxInt32) {
    // Avoid truncation of overly large sleep values.
    seconds = kMaxInt32;
  }
  micros = micros - seconds * kMicrosecondsPerSecond;
  int64_t nanos = micros * kNanosecondsPerMicrosecond;
  req.tv_sec = static_cast<int32_t>(seconds);
  req.tv_nsec = static_cast<long>(nanos);  // NOLINT (long used in timespec).
  while (true) {
    int r = nanosleep(&req, &rem);
    if (r == 0) {
      break;
    }
    // We should only ever see an interrupt error.
    ASSERT(errno == EINTR);
    // Copy remainder into requested and repeat.
    req = rem;
  }
}


void OS::DebugBreak() {
  __builtin_trap();
}


static void VFPrint(FILE* stream, const char* format, va_list args) {
  vfprintf(stream, format, args);
  fflush(stream);
}

void OS::Print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VFPrint(stdout, format, args);
  va_end(args);
}


void OS::PrintErr(const char* format, ...) {
  va_list args;
  va_start(args, format);
  VFPrint(stderr, format, args);
  va_end(args);
}


void OS::Abort() {
  abort();
}


void OS::Exit(int code) {
  exit(code);
}

}  // namespace psoup

#endif  // __APPLE__
