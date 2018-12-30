AC_INIT(fusecompress, 0.9.2)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_LN_S
AM_PROG_CC_C_O
CFLAGS="$CFLAGS -Wall -g"
LIBS=

AC_MSG_CHECKING([checking for Mac OS X])
if test `uname -s` == Darwin ; then
	AC_MSG_RESULT([yes])
	AC_DEFINE(CONFIG_OSX,1,[Compiling on Mac OS X])
	AM_CONDITIONAL(CONFIG_OSX, true)
	CPPFLAGS="$CPPFLAGS -I/opt/local/include"
	LDFLAGS="$LDFLAGS -L/opt/local/lib"
else
	AC_MSG_RESULT([no])
	AM_CONDITIONAL(CONFIG_OSX, false)
fi

# Checks for header files.
AC_CHECK_HEADERS([sys/mount.h sys/fsuid.h fcntl.h limits.h stdint.h stdlib.h string.h sys/statfs.h sys/param.h sys/statvfs.h sys/time.h syslog.h unistd.h utime.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([setfsuid setfsgid fchdir fdatasync ftruncate lchown memset mkdir rmdir strchr strdup strerror strstr strtol utime])

AC_C_BIGENDIAN(AC_DEFINE(FC_BIG_ENDIAN,1,[big-endian]),AC_DEFINE(FC_LITTLE_ENDIAN,1,[little-endian]),AC_MSG_ERROR([unknown endianness]))

AC_ARG_ENABLE(zlib,
        [  --disable-gz            Don't compile DEFLATE compression module])
AC_ARG_ENABLE(bzip2,
        [  --disable-bz2           Don't compile BZip2 compression module])
AC_ARG_ENABLE(lzma,
        [  --disable-lzma          Don't compile LZMA compression module])
AC_ARG_ENABLE(lzo,
        [  --disable-lzo           Don't compile LZO compression module])
AC_ARG_ENABLE(dedup,
        [  --enable-dedup          Enable deduplication module (EXPERIMENTAL)])
AC_ARG_ENABLE(debug,
        [  --enable-debug          Enable debug output])
        
have_zlib=no
have_bzip2=no
have_lzma=no
have_lzo=no
have_mhash=no
if test x$enable_zlib != xno ; then
	AC_CHECK_HEADER(zlib.h,have_zlib_hdr=yes)
        AC_CHECK_LIB(z,gzwrite,have_zlib_lib=yes)
        if test x$have_zlib_hdr == xyes -a x$have_zlib_lib == xyes ; then
           have_zlib=yes
           LIBS="$LIBS -lz"
           AC_DEFINE(HAVE_ZLIB,1,[Compiling with DEFLATE support])
        fi
fi
if test x$enable_bzip2 != xno ; then
	AC_CHECK_HEADER(bzlib.h,have_bzip2_hdr=yes)
        AC_CHECK_LIB(bz2,BZ2_bzwrite,have_bzip2_lib=yes)
        if test x$have_bzip2_hdr == xyes -a x$have_bzip2_lib == xyes ; then
           have_bzip2=yes
           LIBS="$LIBS -lbz2"
           AC_DEFINE(HAVE_BZIP2,1,[Compiling with BZip2 support])
        fi
fi
if test x$enable_lzma != xno ; then
	AC_CHECK_HEADER(lzma.h,have_lzma_hdr=yes)
        AC_CHECK_LIB(lzma,lzma_code, have_lzma_lib=yes)
        if test x$have_lzma_hdr == xyes -a x$have_lzma_lib == xyes ; then
           have_lzma=yes
           LIBS="$LIBS -llzma"
           AC_DEFINE(HAVE_LZMA,1,[Compiling with LZMA support])
        fi
fi
if test x$enable_lzo != xno ; then
	AC_CHECK_HEADER(lzo/lzo1x.h,have_lzo_hdr=yes)
        AC_CHECK_LIB(lzo2,lzo1x_1_compress,have_lzo_lib=yes)
        if test x$have_lzo_hdr == xyes -a x$have_lzo_lib == xyes ; then
           have_lzo=yes
           LIBS="$LIBS -llzo2"
           AC_DEFINE(HAVE_LZO2,1,[Compiling with LZO support])
        fi
fi
if test x$enable_dedup == xyes ; then
	AC_CHECK_HEADER(mhash.h,have_mhash_hdr=yes,AC_MSG_ERROR([mhash header files not found]))
	AC_CHECK_LIB(mhash,mhash_init,have_mhash_lib=yes,AC_MSG_ERROR([no suitable libmhash found]))
	if test x$have_mhash_hdr == xyes -a x$have_mhash_lib == xyes ; then
	   have_mhash=yes
	   LIBS="$LIBS -lmhash"
	   AC_DEFINE(HAVE_MHASH,1,[Compiling with MHASH support])
	fi
fi
AM_CONDITIONAL(DEBUG, test x$enable_debug == xyes)
AM_CONDITIONAL(HAVE_ZLIB, test x$have_zlib == xyes)
AM_CONDITIONAL(HAVE_BZIP2, test x$have_bzip2 == xyes)
AM_CONDITIONAL(HAVE_LZMA, test x$have_lzma == xyes)
AM_CONDITIONAL(HAVE_LZO2, test x$have_lzo == xyes)
AM_CONDITIONAL(HAVE_MHASH, test x$have_mhash == xyes)
AM_CONDITIONAL(DEDUP, test x$enable_dedup == xyes)

export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
PKG_CHECK_MODULES(fuse, [fuse >= 2.2])
oldlibs="$LIBS"
LIBS="$LIBS $fuse_LIBS"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
