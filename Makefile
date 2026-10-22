# ============================================================================
# EATHON COMPILER & DATABASE ENGINE BUILD CONFIGURATION
# Supports Linux, macOS, and Windows (MinGW-w64 / LLVM Clang)
# ============================================================================

CC       = gcc
WINCC    = x86_64-w64-mingw32-gcc
CFLAGS   = -O2 -Wall -Wextra -Wno-format-truncation -Wno-stringop-truncation -std=c99 -Icompiler/include
BINDIR   = bin
SRCDIR   = compiler/src
JNISRC   = android/app/src/main/cpp/eathon_jni.c

all: $(BINDIR)/eathonc $(BINDIR)/eatdb $(BINDIR)/libeathon_jni.so windows

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/eathonc: $(SRCDIR)/eathonc.c compiler/include/eathon_platform.h | $(BINDIR)
	$(CC) $(CFLAGS) $(SRCDIR)/eathonc.c -o $(BINDIR)/eathonc

$(BINDIR)/eatdb: $(SRCDIR)/eatdb.c compiler/include/eathon_platform.h | $(BINDIR)
	$(CC) $(CFLAGS) $(SRCDIR)/eatdb.c -o $(BINDIR)/eatdb

$(BINDIR)/libeathon_jni.so: $(JNISRC) $(SRCDIR)/eathonc.c $(SRCDIR)/eatdb.c compiler/include/eathon_platform.h | $(BINDIR)
	$(CC) $(CFLAGS) -fPIC -shared -DEATHON_LIB_MODE $(JNISRC) $(SRCDIR)/eathonc.c $(SRCDIR)/eatdb.c -o $(BINDIR)/libeathon_jni.so

windows: $(BINDIR)/eathonc.exe $(BINDIR)/eatdb.exe

$(BINDIR)/eathonc.exe: $(SRCDIR)/eathonc.c compiler/include/eathon_platform.h | $(BINDIR)
	@if command -v $(WINCC) >/dev/null 2>&1; then \
		$(WINCC) $(CFLAGS) -static $(SRCDIR)/eathonc.c -o $(BINDIR)/eathonc.exe; \
	fi

$(BINDIR)/eatdb.exe: $(SRCDIR)/eatdb.c compiler/include/eathon_platform.h | $(BINDIR)
	@if command -v $(WINCC) >/dev/null 2>&1; then \
		$(WINCC) $(CFLAGS) -static $(SRCDIR)/eatdb.c -o $(BINDIR)/eatdb.exe; \
	fi

test: all
	node tests/run_tests.mjs

clean:
	rm -rf $(BINDIR)/eathonc $(BINDIR)/eatdb $(BINDIR)/libeathon_jni.so $(BINDIR)/eathonc.exe $(BINDIR)/eatdb.exe


