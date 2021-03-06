#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(mcell, 3.3, mcell@salk.edu)
AC_CONFIG_SRCDIR([mcell.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror])

# Checks for programs.
AC_PROG_YACC
AC_PROG_CC
AC_PROG_CC_C99
if test "$GCC" == "yes"; then
  CFLAGS_WARN="-Wall -Wextra -Wshadow -Wno-unused-parameter"
else
  CFLAGS_WARN=""
fi
AC_SUBST(CFLAGS_WARN)
AM_PROG_LEX

# Check for Windows compile
AC_CHECK_HEADERS([windows.h],[windows=yes],[windows=no])
AH_BOTTOM([
#ifdef _WIN32
#include "config-win.h"
#else
#include "config-nix.h"
#endif
])
# Not all of the checks work when configuring for Windows so override
AS_IF([test "x$windows" == "xyes"],
[[
ac_cv_func_realloc_0_nonnull=yes
ac_cv_func_malloc_0_nonnull=yes
ac_cv_have_decl_strerror_r=yes
ac_cv_func_strerror_r=yes
ac_cv_func_strerror_r_char_p=no
ac_cv_func_gethostname=yes
MCELL_LDADD="-lm"
]],[[
MCELL_LDADD="-lm"
]])
AC_SUBST(MCELL_LDADD)

# Unsafe signal handlers
AC_ARG_ENABLE(unsafe-signals,
  [AC_HELP_STRING(
    [--enable-unsafe-signals],
    [allow signal handlers to flush reaction output when MCell dies due to a signal, not available on Windows])
  ])
AS_IF([test "x$enable_unsafe_signals" == "xyes"], [
  AC_DEFINE([MCELL_UNSAFE_SIGHANDLERS], [1], [If MCELL unsafe signal handlers are enabled])
])

# Checks for libraries.

# Checks for header files.
AC_FUNC_ALLOCA
AC_HEADER_ASSERT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h fenv.h float.h inttypes.h libintl.h limits.h malloc.h memory.h netdb.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T
AC_C_BIGENDIAN

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SETVBUF_REVERSED
AC_FUNC_STAT
AC_FUNC_STRERROR_R
AC_FUNC_STRFTIME
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([alarm floor ftruncate gethostname gettimeofday memmove memset mkdir pow sqrt strchr strdup strerror strrchr strtol strtoul])

AC_SYS_LARGEFILE

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
