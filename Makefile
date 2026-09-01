DISC ?= 1
TARGET = funkin_disc$(DISC)
BINDIR ?= bin/
OBJDIR ?= build/disc$(DISC)
TYPE = ps-exe
TLOAD_ADDR ?= 0x80010000
RAM_ORIGIN ?= 0x80010000
# Main RAM available after the PS-X EXE load address (0x80010000..0x80200000).
# The linker reserves STACK_SIZE below STACK_ADDR before exposing the heap.
RAM_LENGTH ?= 0x1F0000
STACK_SIZE ?= 0x400 
GFX_OTLEN ?= 6 
GFX_PRIBUFF_LEN ?= 16384
USE_LIBSND ?= 0
# Maximum RAM/perf default: memcard libraries consume RAM/code. Set USE_MEMCARD=1 only if this disc needs saving.
USE_MEMCARD ?= 1
BUILD ?= Release
OPTIMIZE_HOT ?= 1
# Hot gameplay/render code: speed optimized. Non-hot code below uses size optimization.
# Optimize all selected objects for compact MIPS code. On this target -O1 and
# -O2 both reduce the runtime heap substantially compared with -Os.
HOT_CFLAGS ?= -Os -fomit-frame-pointer -fno-common -fno-builtin -fno-strict-aliasing

ifeq ($(BUILD),Debug)
ENABLE_DEBUG ?= 1
else
ENABLE_DEBUG ?= 0
endif

# Real hardware defaults: keep debug/printf/debug movement compiled out unless explicitly enabled.
VOLTEX_ENABLE_DEBUG ?= 0

ifeq ($(DISC),4)
SMALL_BUILD ?= 1
STACK_ADDR ?= 0x801FFFF0
# Fail the link unless the allocator receives at least this full usable span.
MIN_HEAP_SIZE ?= 0x1A0000
else
STACK_ADDR ?= 0x801FFFF0
MIN_HEAP_SIZE ?= 0x1A0000
endif

SRCS = src/main.c \
       src/mutil.c \
       src/random.c \
       src/archive.c \
       src/font.c \
       src/trans.c \
       src/loadscr.c \
       src/menu.c \
       src/stage.c \
	   src/stage_enhanced_draw.c \
       src/songswap.c \
       src/event.c \
       src/events.c \
       src/debug.c \
       src/save.c \
       src/psx.c \
       src/disc_swap_disc$(DISC).c \
       src/str.c \
       src/io.c \
       src/gfx.c \
       src/audio.c \
       src/pad.c \
       src/timer.c \
       src/tween.c \
       src/stage/dummy.c \
       src/stage/bvoid.c \
       src/stage/week1.c \
       src/stage/week2.c \
       src/stage/week3.c \
       src/stage/trio.c \
       src/stage/kitchen.c \
       src/animation.c \
       src/character.c \
       src/character/bf.c \
       src/character/speaker.c \
       src/character/dad.c \
       src/character/spook.c \
       src/character/pico.c \
       src/character/monster.c \
       src/character/gf.c \
       src/character/exep3.c \
       src/character/apple.c \
       src/character/orange.c \
       src/character/jerry.c \
       src/character/logan.c \
       src/character/eyes.c \
       src/character/abot_speaker.c \
       src/character/menuo.c \
       src/character/menup.c \
       src/character/menugf.c \
       src/object.c \
       src/object/combo.c \
       src/object/splash.c \
       src/pause.c \
       src/psn00b/vlc.c \
       src/psn00b/vlc2.s \
       mips/common/crt0/crt0.s 

# Small-build include lists: files always included plus per-disc used extras
EVERYWHERE_LIST = \
       src/main.c \
       src/mutil.c \
       src/random.c \
       src/archive.c \
       src/font.c \
       src/trans.c \
       src/loadscr.c \
       src/stage.c \
	   src/stage_enhanced_draw.c \
       src/songswap.c \
       src/event.c \
       src/events.c \
       src/debug.c \
       src/save.c \
       src/psx.c \
       src/str.c \
       src/io.c \
       src/gfx.c \
       src/audio.c \
       src/pad.c \
       src/timer.c \
       src/tween.c \
       src/animation.c \
       src/character.c \
       src/object.c \
       src/object/combo.c \
       src/object/splash.c \
       src/pause.c \
       src/psn00b/vlc.c \
       src/psn00b/vlc2.s \
       mips/common/crt0/crt0.s \
       src/disc_swap_disc$(DISC).c

# Disc1 extras (used on disc1)
DISC1_INCLUDE = \
       src/menu.c \
       src/stage/dummy.c \
       src/stage/week1.c \
       src/character/bf.c \
       src/character/speaker.c \
       src/character/dad.c \
       src/character/gf.c \
       src/character/menuo.c \
       src/character/menup.c \
       src/character/menugf.c \
       src/character/spook.c \
       src/character/pico.c \
       src/character/monster.c \
       src/stage/week2.c \
       src/stage/week3.c

# Disc2 extras (used on disc2)
DISC2_INCLUDE = \
       src/menu.c \
       src/stage/dummy.c \
       src/stage/bvoid.c \
       src/character/bf.c \
       src/character/speaker.c \
       src/character/dad.c \
       src/character/gf.c \
       src/character/menuo.c \
       src/character/menup.c \
       src/character/menugf.c \
       src/character/exep3.c \
       src/stage/trio.c

# Disc3 extras (used on disc3)
DISC3_INCLUDE = \
       src/menu.c \
       src/stage/dummy.c \
       src/stage/bvoid.c \
       src/stage/week1.c \
       src/character/bf.c \
       src/character/speaker.c \
       src/character/dad.c \
       src/character/gf.c \
       src/character/menuo.c \
       src/character/menup.c \
       src/character/menugf.c \
       src/character/apple.c \
       src/character/orange.c \
       src/character/jerry.c \
       src/character/logan.c \
       src/character/eyes.c \
       src/stage/kitchen.c

ifeq ($(SMALL_BUILD),1)
SRCS := $(filter $(EVERYWHERE_LIST) $(DISC1_INCLUDE) $(DISC2_INCLUDE) $(DISC3_INCLUDE), $(SRCS))
endif
ifneq ($(ENABLE_DEBUG),1)
SRCS := $(filter-out src/debug.c,$(SRCS))
endif

CPPFLAGS += -Wall -Wextra -pedantic -mno-check-zero-division -DDISC$(DISC)_ONLY
CPPFLAGS += -DNDEBUG -DVOLTEX_ENABLE_DEBUG=$(VOLTEX_ENABLE_DEBUG)
CPPFLAGS += -DGFX_OTLEN_DEFAULT=$(GFX_OTLEN) -DGFX_PRIBUFF_LEN_DEFAULT=$(GFX_PRIBUFF_LEN)
ifeq ($(USE_LIBSND),1)
CPPFLAGS += -DUSE_LIBSND
endif
ifeq ($(USE_MEMCARD),1)
CPPFLAGS += -DUSE_MEMCARD
endif
ifeq ($(ENABLE_DEBUG),1)
CPPFLAGS += -DENABLE_DEBUG
else
# Size-optimize everything by default to reduce RAM/code pressure; hot objects get HOT_CFLAGS below.
CFLAGS += -Os -fomit-frame-pointer -fno-common -fno-builtin -fno-strict-aliasing -ffunction-sections -fdata-sections
endif
LDFLAGS += -Wl,--defsym=TLOAD_ADDR=$(TLOAD_ADDR) -Wl,--defsym=RAM_ORIGIN=$(RAM_ORIGIN) -Wl,--defsym=RAM_LENGTH=$(RAM_LENGTH) -Wl,--defsym=STACK_ADDR=$(STACK_ADDR) -Wl,--defsym=__stack_size=$(STACK_SIZE) -Wl,--defsym=MIN_HEAP_SIZE=$(MIN_HEAP_SIZE)
LDFLAGS += -Wl,--start-group
# TODO: remove unused libraries
LDFLAGS += -lapi
LDFLAGS += -lc
LDFLAGS += -lc2
LDFLAGS += -lcd
#LDFLAGS += -lcomb
LDFLAGS += -lds
LDFLAGS += -letc
LDFLAGS += -lgpu
LDFLAGS += -lgcc
#LDFLAGS += -lgs
#LDFLAGS += -lgte
#LDFLAGS += -lgun
#LDFLAGS += -lhmd
#LDFLAGS += -lmath
#LDFLAGS += -lmcx
# Link optional PSX libraries required by some features
LDFLAGS += -lpad
ifeq ($(USE_MEMCARD),1)
LDFLAGS += -lcard
LDFLAGS += -lmcrd
endif
LDFLAGS += -lpress
#LDFLAGS += -lsio
LDFLAGS += -lspu
#LDFLAGS += -ltap
ifeq ($(USE_LIBSND),1)
LDFLAGS += -lsnd
endif
ifeq ($(USE_LTO),true)
LDFLAGS += -flto
endif
LDFLAGS += -Wl,--gc-sections -Wl,--end-group -s

ifeq ($(OPTIMIZE_HOT),1)
# Apply the hot flags to every object selected for the current disc. SRCS has
# already been filtered above, so this does not pull unused disc content in.
HOT_OBJECTS = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(SRCS))))

$(HOT_OBJECTS): CFLAGS += $(HOT_CFLAGS)
endif

include mips/common.mk
