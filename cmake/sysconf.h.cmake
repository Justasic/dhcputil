#pragma once

#cmakedefine NO_BACKWARD 1
#cmakedefine GETTEXT_FOUND 1

#define VERSION_MAJOR        @PROJECT_VERSION_MAJOR@
#define VERSION_MINOR        @PROJECT_VERSION_MINOR@
#define VERSION_PATCH        @PROJECT_VERSION_PATCH@
#define VERSION              "@VERSION_SIMPLE@"
#define VERSION_GIT          "@GIT_REVISION_SHORT@"
#define VERSION_FULL         "@VERSION_FULL@"
#define GIT_SHA_SUM          "@GIT_REVISION_LONG@"
#define COMPILED_NAME        "@PROJECT_NAME@"
#define COMPILED_DESC        "@CMAKE_PROJECT_DESCRIPTION@"
#define COMPILED_ARCH        "@CMAKE_SYSTEM_PROCESSOR@"
#define C_COMPILER_NAME      "@CMAKE_C_COMPILER_ID@"
#define C_COMPILER_VERSION   "@CMAKE_C_COMPILER_VERSION@"
#define CXX_COMPILER_VERSION "@CMAKE_CXX_COMPILER_VERSION@"
#define CXX_COMPILER_NAME    "@CMAKE_CXX_COMPILER_ID@"
#define C_COMPILER_TRIPLE    "@C_COMPILER_TRIPLE@"
#define CXX_COMPILER_TRIPLE  "@CXX_COMPILER_TRIPLE@"
#define CMAKE_INSTALL_PREFIX "@CMAKE_INSTALL_PREFIX@"
#define CMAKE_SOURCE_DIR     "@CMAKE_SOURCE_DIR@"
#define CMAKE_BINARY_DIR     "@CMAKE_BINARY_DIR@"
#define CMAKE_BUILD_TYPE     "@CMAKE_BUILD_TYPE@"
#define DEFAULT_CONFIG_LOCATION "@DEFAULT_CONFIG_LOCATION@"

// Define some useful visual things for us
#define HOTPATH __attribute__ ((hot))

#ifndef NDEBUG
# define CFLAGS "@CMAKE_C_FLAGS@"
# define CXXFLAGS "@CMAKE_CXX_FLAGS@"
# define LDFLAGS "@CMAKE_EXE_LINKER_FLAGS@"
#else
# define CFLAGS ""
# define CXXFLAGS ""
# define LDFLAGS ""
#endif

#cmakedefine HAVE_UINT8_T 1
#cmakedefine HAVE_U_INT8_T 1
#cmakedefine HAVE_INT16_T 1
#cmakedefine HAVE_UINT16_T 1
#cmakedefine HAVE_U_INT16_T 1
#cmakedefine HAVE_INT32_T 1
#cmakedefine HAVE_UINT32_T 1
#cmakedefine HAVE_U_INT32_T 1

//#ifdef HAVE_SETJMP_H
//# include <setjmp.h>
//#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif

#ifdef HAVE_UINT8_T
typedef uint8_t uint8;
#else
# ifdef HAVE_U_INT8_T
typedef u_int8_t uint8;
# else
#  ifdef _WIN32
typedef unsigned __int8 uint8;
#  else
typedef unsigned short uint8;
#  endif
# endif
#endif

#ifdef HAVE_INT16_T
typedef int16_t int16;
#else
# ifdef _WIN32
typedef signed __int16 int16;
# else
typedef int int16;
# endif
#endif

#ifdef HAVE_UINT16_T
typedef uint16_t uint16;
#else
# ifdef HAVE_U_INT16_T
typedef u_int16_t uint16;
# else
#  ifdef _WIN32
typedef unsigned __int16 uint16;
#  else
typedef unsigned int uint16;
#  endif
# endif
#endif

#ifdef HAVE_INT32_T
typedef int32_t int32;
#else
# ifdef _WIN32
typedef signed __int32 int32;
# else
typedef long int32;
# endif
#endif

#ifdef HAVE_UINT32_T
typedef uint32_t uint32;
#else
# ifdef HAVE_U_INT32_T
typedef u_int32_t uint32;
# else
#  ifdef _WIN32
typedef unsigned __int32 uint32;
#  else
typedef unsigned long uint32;
#  endif
# endif
#endif

#include <version>

// Define std::unreachable if it's not yet defined.
#ifndef __cpp_lib_unreachable
namespace std 
{
    [[noreturn]] inline void unreachable()
    {
        // Uses compiler specific extensions if possible.
        // Even if no extension is used, undefined behavior is still raised by
        // an empty function body and the noreturn attribute.
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
        __assume(false);
#else // GCC, Clang
        __builtin_unreachable();
#endif
    }
}
#endif
