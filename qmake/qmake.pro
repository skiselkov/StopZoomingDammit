# CDDL HEADER START
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#
# CDDL HEADER END
#
# Copyright 2019 Saso Kiselkov. All rights reserved.

# Shared library without any Qt functionality
TEMPLATE = lib
QT -= gui core

CONFIG += plugin debug
CONFIG -= thread exceptions qt rtti release

VERSION = 1.0.0

INCLUDEPATH += $$[LIBACFUTILS]/SDK/CHeaders/XPLM
INCLUDEPATH += $$[LIBACFUTILS]/SDK/CHeaders/Widgets
INCLUDEPATH += $$[LIBACFUTILS]/src

QMAKE_CFLAGS += -std=c99 -O2 -W -Wall -Wextra -Werror -fvisibility=hidden \
    -Wno-unused-local-typedefs -Wunused-result -Wno-unknown-pragmas
QMAKE_CXXFLAGS += -fvisibility=hidden

# Make sure to disable Qmake's own warnings system, because it overrides
# our warning flags. This breaks CTASSERT, which relies on an unused local
# typedef.
QMAKE_CFLAGS_WARN_ON -= -W -Wall
QMAKE_CXXFLAGS_WARN_ON -= -W -Wall

# _GNU_SOURCE needed on Linux for getline()
# DEBUG - used by our ASSERT macro
# _FILE_OFFSET_BITS=64 to get 64-bit ftell and fseek on 32-bit platforms.
DEFINES += _GNU_SOURCE DEBUG _FILE_OFFSET_BITS=64

# Latest X-Plane APIs. No legacy support needed.
DEFINES += XPLM200 XPLM210 XPLM300 XPLM301

# Just a generally good idea not to depend on shipped libgcc.
!macx {
	LIBS += -static-libgcc -static-libstdc++ -Wl,--exclude-libs,ALL
}

QMAKE_CFLAGS += -Wno-missing-field-initializers

win32 {
	CONFIG += dll
	DEFINES += APL=0 IBM=1 LIN=0 _WIN32_WINNT=0x0600
	TARGET = win.xpl
	QMAKE_DEL_FILE = rm -f
	LIBS += -Wl,--exclude-libs,ALL
	QMAKE_LFLAGS_RELEASE =
}

win32:contains(CROSS_COMPILE, x86_64-w64-mingw32-) {
	QMAKE_CFLAGS += $$system("$$[LIBACFUTILS]/pkg-config-deps win-64 \
	    --cflags")

	# This must go first for GCC to properly find dependent symbols
	LIBS += -L$$[LIBACFUTILS]/qmake/win64 -lacfutils
	LIBS += $$system("$$[LIBACFUTILS]/pkg-config-deps win-64 --libs")
	LIBS += -L$$[LIBACFUTILS]/SDK/Libraries/Win -lXPLM_64
	LIBS += -L$$[LIBACFUTILS]/GL_for_Windows/lib -lglu32 -lopengl32
}

linux-g++-64 {
	DEFINES += APL=0 IBM=0 LIN=1
	TARGET = lin.xpl

	QMAKE_CFLAGS += $$system("$$[LIBACFUTILS]/pkg-config-deps linux-64 \
	    --cflags")

	LIBS += -L$$[LIBACFUTILS]/qmake/lin64 -lacfutils
	LIBS += $$system("$$[LIBACFUTILS]/pkg-config-deps linux-64 --libs")
	LIBS += -nodefaultlibs
	LIBS += -Wl,--exclude-libs,ALL
	LIBS += $$system("g++ -print-file-name=libstdc++.a")
	LIBS += -Wl,-Bstatic -lc_nonshared
}

macx {
	QMAKE_CFLAGS += -mmacosx-version-min=10.9
	QMAKE_LFLAGS += -mmacosx-version-min=10.9

	DEFINES += APL=1 IBM=0 LIN=0
	TARGET = mac.xpl
	LIBS += -F$$[LIBACFUTILS]/SDK/Libraries/Mac
	LIBS += -framework XPLM -framework XPWidgets
}

macx-clang {
	QMAKE_CFLAGS += $$system("$$[LIBACFUTILS]/pkg-config-deps mac-64 \
	    --cflags")

	LIBS += -L$$[LIBACFUTILS]/qmake/mac64 -lacfutils
	LIBS += $$system("$$[LIBACFUTILS]/pkg-config-deps mac-64 --libs")
}

HEADERS += ../src/*.h
SOURCES += ../src/*.c
