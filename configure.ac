AC_INIT([pdd], [1.0.0], [juancortez0128@gmail.com])
AM_INIT_AUTOMAKE
AC_PROG_CC
AC_CONFIG_FILES([Makefile])
AC_CHECK_HEADERS(pthread.h,, [AC_MSG_ERROR([pthread.h required])])
AC_CHECK_LIB(pthread, pthread_create, [LIBS="$LIBS -lpthread"])
AC_OUTPUT
