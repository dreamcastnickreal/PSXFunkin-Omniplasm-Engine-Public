# psxtools - PSXFunkin all-in-one editor/creator GUI (GTK3)
# Replaces tools/stagegen (python) with a modular C tool.
#
# Modules (add as you grow):
#   editor_stage - stage editor (import weekN.c, edit sprites, export DrawBG)
#   editor_week  - weekN.c/.h creator (charts, chars, setptr)
#   (future) pack_timpak, pack_exepak, ... - the packers merged in

CC      ?= gcc
PKGCFG  := $(shell pkg-config --cflags --libs gtk+-3.0)

CFLAGS  := -O2 -g -Wall -Wextra -std=gnu11 -Isrc $(PKGCFG)
LDLIBS  := $(PKGCFG) -lm

BUILD   := build
SRCS    := $(wildcard src/*.c)
OBJS    := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

# Windows (MSYS2/MinGW): gcc emits psxtools.exe, so track that name or
# `make` relinks every time and `clean` misses the binary.
ifeq ($(OS),Windows_NT)
EXE := .exe
endif

TARGET  := psxtools$(EXE)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDLIBS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
