// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef VM_GLOBALS_H_
#define VM_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Target OS detection.
// for more information on predefined macros:
//   - http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   - with gcc, run: "echo | gcc -E -dM -"
#if defined(__ANDROID__)
#define TARGET_OS_ANDROID 1
#elif defined(__linux__) || defined(__FreeBSD__)
#define TARGET_OS_LINUX 1
#elif defined(__APPLE__)
// Define the flavor of Mac OS we are running on.
#include <TargetConditionals.h>
// TODO(iposva): Rename TARGET_OS_MACOS to TARGET_OS_MAC to inherit
// the value defined in TargetConditionals.h
#define TARGET_OS_MACOS 1
#if TARGET_OS_IPHONE
#define TARGET_OS_IOS 1
#endif

#elif defined(_WIN32)
#define TARGET_OS_WINDOWS 1
#else
#error Automatic target os detection failed.
#endif


#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__)
#define ARCH_IS_64_BIT 1
#elif defined(_M_IX86) || defined(__i386__) \
    || defined(__arm__) || defined(__mips__)
#define ARCH_IS_32_BIT 1
#else
#error unknown arch
#endif


// PSOUP_UNUSED inidicates to the compiler that a variable/typedef is expected
// to be unused and disables the related warning.
#ifdef __GNUC__
#define PSOUP_UNUSED __attribute__((unused))
#else
#define PSOUP_UNUSED
#endif


// Short form printf format specifiers
#define Pd PRIdPTR
#define Pu PRIuPTR
#define Px PRIxPTR
#define Pd64 PRId64
#define Pu64 PRIu64
#define Px64 PRIx64


// Suffixes for 64-bit integer literals.
#ifdef _MSC_VER
#define PSOUP_INT64_C(x) x##I64
#define PSOUP_UINT64_C(x) x##UI64
#else
#define PSOUP_INT64_C(x) x##LL
#define PSOUP_UINT64_C(x) x##ULL
#endif


// The following macro works on both 32 and 64-bit platforms.
// Usage: instead of writing 0x1234567890123456ULL
//      write PSOUP_2PART_UINT64_C(0x12345678,90123456);
#define PSOUP_2PART_UINT64_C(a, b)                                             \
                 (((static_cast<uint64_t>(a) << 32) + 0x##b##u))


// Integer constants.
const int32_t kMinInt32 = 0x80000000;
const int32_t kMaxInt32 = 0x7FFFFFFF;
const uint32_t kMaxUint32 = 0xFFFFFFFF;
const int64_t kMinInt64 = PSOUP_INT64_C(0x8000000000000000);
const int64_t kMaxInt64 = PSOUP_INT64_C(0x7FFFFFFFFFFFFFFF);
const uint64_t kMaxUint64 = PSOUP_2PART_UINT64_C(0xFFFFFFFF, FFFFFFFF);
const int64_t kSignBitDouble = PSOUP_INT64_C(0x8000000000000000);


// Types for native machine words. Guaranteed to be able to hold pointers and
// integers.
typedef intptr_t word;
typedef uintptr_t uword;


// Byte sizes.
const int kWordSize = sizeof(word);
const int kDoubleSize = sizeof(double);  // NOLINT
const int kFloatSize = sizeof(float);  // NOLINT
const int kInt32Size = sizeof(int32_t);  // NOLINT
const int kInt16Size = sizeof(int16_t);  // NOLINT
#ifndef ARCH_IS_64_BIT
const int kWordSizeLog2 = 2;
const uword kUwordMax = kMaxUint32;
#else
const int kWordSizeLog2 = 3;
const uword kUwordMax = kMaxUint64;
#endif

// Bit sizes.
const int kBitsPerByte = 8;
const int kBitsPerWord = kWordSize * kBitsPerByte;
const intptr_t kSmiBits = kBitsPerWord - 2;
const intptr_t kSmiMax = (static_cast<intptr_t>(1) << kSmiBits) - 1;
const intptr_t kSmiMin =  -(static_cast<intptr_t>(1) << kSmiBits);


// System-wide named constants.
const intptr_t KB = 1024;
const intptr_t KBLog2 = 10;
const intptr_t MB = KB * KB;
const intptr_t MBLog2 = KBLog2 + KBLog2;
const intptr_t GB = MB * KB;
const intptr_t GBLog2 = MBLog2 + KBLog2;

// Time constants.
const int kMillisecondsPerSecond = 1000;
const int kMicrosecondsPerMillisecond = 1000;
const int kMicrosecondsPerSecond = (kMicrosecondsPerMillisecond *
                                    kMillisecondsPerSecond);
const int kNanosecondsPerMicrosecond = 1000;
const int kNanosecondsPerMillisecond = (kNanosecondsPerMicrosecond *
                                        kMicrosecondsPerMillisecond);
const int kNanosecondsPerSecond = (kNanosecondsPerMicrosecond *
                                   kMicrosecondsPerSecond);

// A macro to disallow the copy constructor and operator= functions.
// This should be used in the private: declarations for a class.
#if !defined(DISALLOW_COPY_AND_ASSIGN)
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                     \
private:                                                                       \
  TypeName(const TypeName&);                                                   \
  void operator=(const TypeName&)
#endif  // !defined(DISALLOW_COPY_AND_ASSIGN)

// A macro to disallow all the implicit constructors, namely the default
// constructor, copy constructor and operator= functions. This should be
// used in the private: declarations for a class that wants to prevent
// anyone from instantiating it. This is especially useful for classes
// containing only static methods.
#if !defined(DISALLOW_IMPLICIT_CONSTRUCTORS)
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName)                               \
private:                                                                       \
  TypeName();                                                                  \
  DISALLOW_COPY_AND_ASSIGN(TypeName)
#endif  // !defined(DISALLOW_IMPLICIT_CONSTRUCTORS)

// Macro to disallow allocation in the C++ heap. This should be used
// in the private section for a class. Don't use UNREACHABLE here to
// avoid circular dependencies between platform/globals.h and
// platform/assert.h.
#if !defined(DISALLOW_ALLOCATION)
#define DISALLOW_ALLOCATION()                                                  \
public:                                                                        \
  void operator delete(void* pointer) {                                        \
    fprintf(stderr, "unreachable code\n");                                     \
    abort();                                                                   \
  }                                                                            \
private:                                                                       \
  void* operator new(size_t size);
#endif  // !defined(DISALLOW_ALLOCATION)


#if defined(TARGET_OS_LINUX) || defined(TARGET_OS_MACOS)
// Tell the compiler to do printf format string checking if the
// compiler supports it; see the 'format' attribute in
// <http://gcc.gnu.org/onlinedocs/gcc-4.3.0/gcc/Function-Attributes.html>.
//
// N.B.: As the GCC manual states, "[s]ince non-static C++ methods
// have an implicit 'this' argument, the arguments of such methods
// should be counted from two, not one."
#define PRINTF_ATTRIBUTE(string_index, first_to_check) \
  __attribute__((__format__(__printf__, string_index, first_to_check)))
#else
#define PRINTF_ATTRIBUTE(string_index, first_to_check)
#endif

#endif  // VM_GLOBALS_H_
