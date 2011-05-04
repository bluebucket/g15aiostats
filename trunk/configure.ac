#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#AC_PREREQ([2.64])
AC_INIT(g15aiostats, 0.24, kbranch@kbranch.net)
AC_CONFIG_SRCDIR([src/common.h])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([config.h])
AC_PREFIX_DEFAULT(/usr)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([boost_regex], [main], ,
	AC_MSG_ERROR([Unable to find the boost_regex library]))
AC_CHECK_LIB([g15daemon_client], [main])
AC_CHECK_LIB([g15render], [main])
AC_CHECK_LIB([sensors], [main])
AC_CHECK_LIB([gtop-2.0], [main])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday localtime_r])

AC_CONFIG_FILES([Makefile
                 man1/Makefile
                 src/Makefile])
#AC_CONFIG_SUBDIRS([release/g15aiostats-0.21])
AC_OUTPUT
