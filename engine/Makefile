# Engine Makefile. Works on Linux and on Windows inside an MSYS2 MinGW64 shell.
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Iinclude
ifeq ($(OS),Windows_NT)
EXE = .exe
endif

all: courtsync$(EXE)

courtsync$(EXE): src/main.c src/scoreboard.c include/scoreboard.h
	$(CC) $(CFLAGS) -o $@ src/main.c src/scoreboard.c

test_runner$(EXE): tests/test_scoreboard.c src/scoreboard.c include/scoreboard.h
	$(CC) $(CFLAGS) -o $@ tests/test_scoreboard.c src/scoreboard.c

test: test_runner$(EXE)
	./test_runner$(EXE)

clean:
	rm -f courtsync courtsync.exe test_runner test_runner.exe state.json events.log

.PHONY: all test clean
