AC_PREREQ(2.65)

AC_INIT(nurly-module, 1.2.1, max.kalika+projects@gmail.com)
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE(1.11 no-define foreign)
LT_INIT

AC_CONFIG_SRCDIR(src)
AC_CONFIG_MACRO_DIR(m4)

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([Makefile src/Makefile])

AC_GNU_SOURCE
AC_TYPE_SIZE_T
AC_FUNC_MALLOC
AC_PROG_CC_C99

AC_CHECK_HEADERS([errno.h pthread.h regex.h stdio.h stdarg.h stdlib.h string.h unistd.h time.h], [], [exit 1])
AC_CHECK_HEADERS([curl/curl.h], [], [exit 1])

AC_CHECK_LIB(pthread, pthread_create, [], [exit 1])
AC_CHECK_LIB(curl, curl_global_init, [], [exit 1])

AC_CHECK_FUNCS([gettimeofday strdup regcomp regexec regerror open_memstream fdopen fprintf asprintf vsnprintf], [], [exit 1])

AC_ENABLE_SHARED
AC_DISABLE_STATIC
AC_PREFIX_DEFAULT(/usr)

AC_OUTPUT
