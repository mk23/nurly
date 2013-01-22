AC_PREREQ(2.68)

AC_INIT(nurly, 1.0.0, max.kalika+projects@gmail.com)
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE(1.11 no-define foreign)
LT_INIT

AC_GNU_SOURCE
AC_PROG_CC_C99

AC_CONFIG_SRCDIR(src)
AC_CONFIG_MACRO_DIR(m4)

AC_CHECK_HEADERS([curl/curl.h])
AC_CHECK_HEADERS([errno.h pthread.h stdio.h stdarg.h stdlib.h string.h unistd.h time.h])

AC_CHECK_LIB(curl, curl_global_init)
AC_CHECK_LIB(pthread, pthread_create)

AC_TYPE_SIZE_T

AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday strdup open_memstream fdopen fprintf asprintf vsnprintf])

AC_ENABLE_SHARED
AC_DISABLE_STATIC

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_FILES([server/nurly/version.py Makefile src/Makefile])
AC_OUTPUT