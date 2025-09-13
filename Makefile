PREFIX = 
export CC = $(PREFIX)gcc
export CXX = $(PREFIX)g++
export LD = $(PREFIX)g++
export AR = $(PREFIX)gcc-ar
export WINDRES = $(PREFIX)windres
export RM = rm -f

CEXT   = .c
CXXEXT = .cpp
OBJEXT = .o
LIBEXT = .a
RCEXT  = .rc
BINEXT = .exe
SRCDIR = src
DSTDIR = bin

#DEBUG = 1
LTO = 1

CWFLAGS = -Wall -Wextra -Wformat -pedantic -Wshadow -Wconversion -Wparentheses -Wunused -Wno-missing-field-initializers
CDFLAGS = -DNOMINMAX -D_USE_MATH_DEFINES -DWIN32 -D_WIN32_WINNT=0x0601 -D_LARGEFILE64_SOURCE -DUNICODE -D_UNICODE -D__USE_MINGW_ANSI_STDIO=0 -DPSAPI_VERSION=1
ifeq (,$(strip $(DEBUG)))
 CPPFLAGS = -I$(SRCDIR)
 ifeq (,$(strip $(LTO)))
  BASE_CFLAGS = -O2 -mstackrealign -fno-ident -march=core2 -mtune=generic -fno-strict-aliasing -fno-asynchronous-unwind-tables -ftree-vectorize -ffunction-sections -fdata-sections -ffast-math -fgraphite -fno-devirtualize -fomit-frame-pointer -DNDEBUG $(CDFLAGS)
  LDFLAGS = -O2 -s -Wl,--gc-sections -fwhole-program
 else
  BASE_CFLAGS = -O2 -mstackrealign -fno-ident -march=core2 -mtune=generic -fno-strict-aliasing -fno-asynchronous-unwind-tables -ftree-vectorize -ffunction-sections -fdata-sections -flto -flto-partition=none -fgraphite-identity -ffast-math -fgraphite -fno-devirtualize -fomit-frame-pointer -DNDEBUG $(CDFLAGS)
  LDFLAGS = -O2 -s -flto -flto-partition=none -fuse-linker-plugin -ftree-vectorize -fgraphite-identity -fmerge-all-constants -Wl,--gc-sections -fwhole-program
 endif
else
 CPPFLAGS = -I$(SRCDIR)
 BASE_CFLAGS = -Wa,-mbig-obj -Og -g3 -ggdb -gdwarf-3 -mstackrealign -fno-strict-aliasing -fno-devirtualize -fno-omit-frame-pointer -march=core2 -mtune=generic $(CDFLAGS)
 LDFLAGS = -fno-omit-frame-pointer
endif
CFLAGS = -std=c17 $(BASE_CFLAGS)
#CXXFLAGS = -Wcast-qual -Wno-non-virtual-dtor -Wold-style-cast -Wno-unused-parameter -Wno-long-long -Wno-maybe-uninitialized -std=c++17 $(BASE_CFLAGS) -fno-exceptions
LDFLAGS += -static -municode -mwindows -Wl,-u,wWinMain

include src/common.mk
