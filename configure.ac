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

################################################################################
#
# Transport support options
#

AC_ARG_WITH([bluez-support],
	[AS_HELP_STRING([--with-bluez-support], [Build with BlueZ transport support (HCI/L2CAP) @<:@default=yes@:>@])],
	[with_bluez_support=$withval],
	[with_bluez_support=yes])

AC_ARG_WITH([nimble-support],
	[AS_HELP_STRING([--with-nimble-support], [Build with Nimble transport support (/dev/atbm_ioctl) @<:@default=no@:>@])],
	[with_nimble_support=$withval],
	[with_nimble_support=no])

AC_ARG_WITH([server-support],
	[AS_HELP_STRING([--with-server-support], [Build with BLE GATT server support @<:@default=no@:>@])],
	[with_server_support=$withval],
	[with_server_support=no])

AC_ARG_VAR([NIMBLE_ROOT], [Path to Nimble BLE stack root directory (for headers)])
AC_ARG_VAR([NIMBLE_LIBDIR], [Path to Nimble library directory (defaults to NIMBLE_ROOT/lib or NIMBLE_ROOT/build)])

# Validate: require at least one transport
if test "x$with_bluez_support" != "xyes" && test "x$with_nimble_support" != "xyes"; then
	AC_MSG_ERROR([At least one of --with-bluez-support or --with-nimble-support must be enabled])
fi

################################################################################
#
# BlueZ support
#

if test "x$with_bluez_support" = "xyes"; then
	AC_MSG_NOTICE([BlueZ transport: ENABLED])

	bluez_ok=1
	AC_CHECK_HEADER(bluetooth/bluetooth.h, [ ], [bluez_ok=0])
	AC_SEARCH_LIBS(hci_open_dev, bluetooth, [ ], [bluez_ok=0])

	if test x$bluez_ok == x0; then
		AC_MSG_ERROR([BlueZ bluetooth headers/library missing. Install libbluetooth-dev or disable with --without-bluez-support])
	fi

	BLEPP_BLUEZ_SUPPORT=1
	AC_SUBST(BLEPP_BLUEZ_SUPPORT)
else
	AC_MSG_NOTICE([BlueZ transport: DISABLED])
fi

################################################################################
#
# Nimble support
#

if test "x$with_nimble_support" = "xyes"; then
	AC_MSG_NOTICE([Nimble transport: ENABLED])

	# If NIMBLE_ROOT is specified, add its include paths to CPPFLAGS for header check
	if test "x$NIMBLE_ROOT" != "x"; then
		# Check if NIMBLE_ROOT exists
		if test ! -d "$NIMBLE_ROOT"; then
			AC_MSG_ERROR([NIMBLE_ROOT not found at $NIMBLE_ROOT. Please set NIMBLE_ROOT environment variable or pass NIMBLE_ROOT=/path/to/nimble_v42 to configure])
		fi

		# Convert to absolute path
		NIMBLE_ROOT=$(cd "$NIMBLE_ROOT" && pwd)
		AC_MSG_NOTICE([Using Nimble headers from $NIMBLE_ROOT])

		# Add Nimble include paths for header check
		CPPFLAGS="$CPPFLAGS -I$NIMBLE_ROOT/nimble/include -I$NIMBLE_ROOT/nimble/host/include -I$NIMBLE_ROOT/nimble/host/services/gap/include -I$NIMBLE_ROOT/nimble/host/services/gatt/include -I$NIMBLE_ROOT/nimble/host/util/include -I$NIMBLE_ROOT/porting/nimble/include -I$NIMBLE_ROOT/ext/tinycrypt/include"
	fi

	# Check for Nimble headers using standard AC_CHECK_HEADER
	AC_CHECK_HEADER([nimble/ble.h], [], [
		AC_MSG_ERROR([Nimble headers not found. Please set NIMBLE_ROOT to the Nimble BLE stack directory, or ensure nimble/ble.h is in your include path.])
	])

	# Find Nimble library
	# If NIMBLE_LIBDIR is specified, use it directly
	if test "x$NIMBLE_LIBDIR" != "x"; then
		if test ! -d "$NIMBLE_LIBDIR"; then
			AC_MSG_ERROR([NIMBLE_LIBDIR not found at $NIMBLE_LIBDIR])
		fi
		# Convert to absolute path
		NIMBLE_LIBDIR=$(cd "$NIMBLE_LIBDIR" && pwd)
		AC_MSG_NOTICE([Using Nimble library from $NIMBLE_LIBDIR])
	elif test "x$NIMBLE_ROOT" != "x"; then
		# Try to find library in NIMBLE_ROOT/lib or NIMBLE_ROOT/build
		nimble_lib_found=no
		for nimble_libdir in "$NIMBLE_ROOT/lib" "$NIMBLE_ROOT/build"; do
			if test -f "$nimble_libdir/libnimble.so" || test -f "$nimble_libdir/libnimble.a"; then
				NIMBLE_LIBDIR="$nimble_libdir"
				nimble_lib_found=yes
				AC_MSG_NOTICE([Found Nimble library in $NIMBLE_LIBDIR])
				break
			fi
		done

		if test "x$nimble_lib_found" != "xyes"; then
			AC_MSG_ERROR([Nimble library not found. Please build Nimble first or set NIMBLE_LIBDIR.
Expected location: $NIMBLE_ROOT/lib/libnimble.so or $NIMBLE_ROOT/build/libnimble.so])
		fi
	fi

	# If NIMBLE_LIBDIR is set, add to LDFLAGS
	if test "x$NIMBLE_LIBDIR" != "x"; then
		LDFLAGS="$LDFLAGS -L$NIMBLE_LIBDIR -Wl,-rpath,$NIMBLE_LIBDIR"

		# Verify the library file exists
		if test -f "$NIMBLE_LIBDIR/libnimble.so" || test -f "$NIMBLE_LIBDIR/libnimble.a"; then
			AC_MSG_NOTICE([Found Nimble library in $NIMBLE_LIBDIR])
			LIBS="$LIBS -lnimble"
		else
			AC_MSG_ERROR([Nimble library not found in $NIMBLE_LIBDIR. Expected libnimble.so or libnimble.a])
		fi
	else
		# No NIMBLE_LIBDIR specified, try to find nimble in standard paths
		AC_SEARCH_LIBS([ble_gap_disc], [nimble], [], [
			AC_MSG_ERROR([Nimble library not found. Please set NIMBLE_LIBDIR or ensure libnimble is in your library path.])
		])
	fi

	BLEPP_NIMBLE_SUPPORT=1
	AC_SUBST(BLEPP_NIMBLE_SUPPORT)
	AC_SUBST(NIMBLE_ROOT)
	AC_SUBST(NIMBLE_LIBDIR)
else
	AC_MSG_NOTICE([Nimble transport: DISABLED])
fi

################################################################################
#
# Server support
#

if test "x$with_server_support" = "xyes"; then
	AC_MSG_NOTICE([Server support: ENABLED])
	BLEPP_SERVER_SUPPORT=1
	AC_SUBST(BLEPP_SERVER_SUPPORT)
else
	AC_MSG_NOTICE([Server support: DISABLED])
fi

TEST_AND_SET_CXXFLAG(-fPIC)
dnl TEST_AND_SET_CXXFLAG(-Werror)

AC_OUTPUT(Makefile)
