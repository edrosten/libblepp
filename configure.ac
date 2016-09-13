AC_INIT(libble++, version-0.5)
AC_COPYRIGHT([E. Rosten, 2016, the BlueZ contributors to 2016])
AC_LANG(C++)
AC_PROG_CXX


m4_include([m4/ax_cxx_compile_stdcxx_14.m4])


################################################################################
#
# Useful macros
#

dnl APPEND(var, value)
dnl This appends vale to a shell variable var
define(APPEND, [$1="$$1 $2"])



dnl TEST_AND_SET_CXXFLAG(flag, [program], [run])
dnl
dnl This attempts to compile a and run program with a certain compiler flag.
dnl If no program is given, then the minimal C++ program is compiled, and 
dnl this tests just the validity of the compiler flag. 
dnl
define([TEST_AND_SET_CXXFLAG],[
	if test "$3" == ""
	then
		AC_MSG_CHECKING([if compiler flag $1 works])	
	else
		AC_MSG_CHECKING([$3])	
	fi
	save_CXXFLAGS="$CXXFLAGS"
	APPEND(CXXFLAGS, [$1])

	m4_if([$2],[],[pushdef(prog, [int main(){}])], [pushdef(prog, [$2])])

	m4_if([$4],[run],
		[AC_RUN_IFELSE([AC_LANG_SOURCE([prog])], [cvd_conf_test=1],[cvd_conf_test=0], [cvd_conf_test=0])],
		[AC_COMPILE_IFELSE([AC_LANG_SOURCE([prog])], [cvd_conf_test=1],[cvd_conf_test=0])]
	)
	
	
	popdef([prog])

	if test $cvd_conf_test = 1
	then
		AC_MSG_RESULT(yes)
		ts_success=yes
	else
		AC_MSG_RESULT(no)
		CXXFLAGS="$save_CXXFLAGS"
		ts_success=no
	fi
])


dnl Add flags, but only if the flags weren't specified.
if test "$CXXFLAGS" == "-g -O2"
then
	
	TEST_AND_SET_CXXFLAG(-Wall)
	TEST_AND_SET_CXXFLAG(-Wextra)
	TEST_AND_SET_CXXFLAG(-W)
	TEST_AND_SET_CXXFLAG(-O3)
	TEST_AND_SET_CXXFLAG(-ggdb)
fi

AX_CXX_COMPILE_STDCXX_14


#
# Test for Package Config
#
AC_PATH_PROG(PKG_CONFIG, pkg-config)
if test "x$PKG_CONFIG" = "x"; then
	AC_MSG_WARN([Could not find pkg-config, will not create pc file.])
else
	# we need sed to find the pkg-config lib directory
	AC_CHECK_PROG(SED,sed,sed,AC_MSG_ERROR([You Must install sed]))
	AC_MSG_CHECKING([for pkg-config library dir])
	PKGCONFIG_LIBDIR="`echo $PKG_CONFIG | $SED -e 's~.*/bin/pkg-config$~~'`${libdir}/pkgconfig"

	AC_MSG_RESULT($PKGCONFIG_LIBDIR)
	AC_SUBST(PKGCONFIG_LIBDIR)
	AC_CONFIG_FILES([ libblepp.pc ])

	# This will be put into the pc file
	VERSION=AC_PACKAGE_VERSION()
	AC_SUBST(VERSION)
fi

a=1
AC_CHECK_HEADER(bluetooth/bluetooth.h,  [ ], [a=0])
AC_SEARCH_LIBS(hci_open_dev, bluetooth, [ ], [a=0])

if test x$a == x0
then
	AC_ERROR([bluez bluetooth missing])
fi

AC_CHECK_HEADER(boost/optional.hpp,  [ ], [AC_ERROR([boost::optional missing])])



TEST_AND_SET_CXXFLAG(-fPIC)
dnl TEST_AND_SET_CXXFLAG(-Werror)

AC_OUTPUT(Makefile)

