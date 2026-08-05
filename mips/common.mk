BUILD ?= Release

HAS_LINUX_MIPS_GCC = $(shell which mipsel-linux-gnu-gcc > /dev/null 2> /dev/null && echo true || echo false)

ifeq ($(HAS_LINUX_MIPS_GCC),true)
PREFIX ?= mipsel-linux-gnu
FORMAT ?= elf32-tradlittlemips
else
PREFIX ?= mipsel-none-elf
FORMAT ?= elf32-littlemips
endif

ROOTDIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CC  = $(PREFIX)-gcc
CXX = $(PREFIX)-g++

TYPE ?= cpe
LDSCRIPT ?= $(ROOTDIR)/$(TYPE).ld
ifneq ($(strip $(OVERLAYSCRIPT)),)
LDSCRIPT := $(addprefix $(OVERLAYSCRIPT) , -T$(LDSCRIPT))
else
LDSCRIPT := $(addprefix $(ROOTDIR)/default.ld , -T$(LDSCRIPT))
endif

USE_FUNCTION_SECTIONS ?= true
USE_LTO ?= true

ARCHFLAGS = -march=mips1 -mabi=32 -EL -fno-pic -mno-shared -mno-abicalls -mfp32
ARCHFLAGS += -fno-stack-protector -nostdlib -ffreestanding
ifeq ($(USE_FUNCTION_SECTIONS),true)
CPPFLAGS += -ffunction-sections -fdata-sections
endif
CPPFLAGS += -mno-gpopt -fomit-frame-pointer
CPPFLAGS += -fno-builtin -fno-strict-aliasing -Wno-attributes
CPPFLAGS += $(ARCHFLAGS)
CPPFLAGS += -I$(ROOTDIR)/psyq/include

LDFLAGS += -Wl,-Map=$(BINDIR)$(TARGET).map -nostdlib -T$(LDSCRIPT) -static -Wl,--gc-sections
LDFLAGS += $(ARCHFLAGS) -Wl,--oformat=$(FORMAT) -L$(ROOTDIR)/psyq/lib

CPPFLAGS_Release += -Os -DNDEBUG
LDFLAGS_Release += -Os

CPPFLAGS_Debug += -Os
CPPFLAGS_Coverage += -Os
CPPFLAGS_Fast += -O2 -DNDEBUG
LDFLAGS_Fast += -O2 -s

CPPFLAGS += $(CPPFLAGS_$(BUILD))
LDFLAGS_Release += -s
ifeq ($(USE_LTO),true)
CPPFLAGS += -flto
endif
LDFLAGS += $(LDFLAGS_$(BUILD))

OBJDIR ?=
DEPDIR ?= $(OBJDIR)

ifneq ($(findstring :,$(CURDIR)),)
MKDIR_P = cmd /C if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
else
MKDIR_P = mkdir -p "$1"
endif

ifeq ($(strip $(OBJDIR)),)
OBJS += $(addsuffix .o, $(basename $(SRCS)))
else
OBJS += $(addprefix $(OBJDIR)/,$(addsuffix .o, $(basename $(SRCS))))
endif

all: $(BINDIR)$(TARGET).$(TYPE)

$(BINDIR)$(TARGET).$(TYPE): $(BINDIR)$(TARGET).elf
	$(PREFIX)-objcopy $(addprefix -R , $(OVERLAYSECTION)) -O binary $< $@
	$(foreach ovl, $(OVERLAYSECTION), $(PREFIX)-objcopy -j $(ovl) -O binary $< $(BINDIR)Overlay$(ovl);)

$(BINDIR)$(TARGET).elf: $(OBJS)
ifneq ($(strip $(BINDIR)),)
	$(call MKDIR_P,$(BINDIR))
endif
	$(CC) -o $(BINDIR)$(TARGET).elf $(OBJS) $(LDFLAGS)

%.o: %.s
	$(CC) $(ARCHFLAGS) -I$(ROOTDIR) -c -o $@ $<

ifneq ($(strip $(OBJDIR)),)
$(OBJDIR)/%.o: %.c
	$(call MKDIR_P,$(dir $@))
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(patsubst %.o,%.dep,$@) -c -o $@ $<

$(OBJDIR)/%.o: %.cpp
	$(call MKDIR_P,$(dir $@))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $(patsubst %.o,%.dep,$@) -c -o $@ $<

$(OBJDIR)/%.o: %.cc
	$(call MKDIR_P,$(dir $@))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $(patsubst %.o,%.dep,$@) -c -o $@ $<

$(OBJDIR)/%.o: %.s
	$(call MKDIR_P,$(dir $@))
	$(CC) $(ARCHFLAGS) -I$(ROOTDIR) -c -o $@ $<
endif

ifeq ($(strip $(DEPDIR)),)
DEPS := $(patsubst %.cpp, %.dep,$(filter %.cpp,$(SRCS)))
DEPS := $(patsubst %.cc,  %.dep,$(filter %.cc,$(SRCS)))
DEPS +=	$(patsubst %.c,   %.dep,$(filter %.c,$(SRCS)))
DEPS += $(patsubst %.s,   %.dep,$(filter %.s,$(SRCS)))
else
DEPS := $(addprefix $(DEPDIR)/,$(patsubst %.cpp, %.dep,$(filter %.cpp,$(SRCS))))
DEPS := $(addprefix $(DEPDIR)/,$(patsubst %.cc,  %.dep,$(filter %.cc,$(SRCS))))
DEPS +=	$(addprefix $(DEPDIR)/,$(patsubst %.c,   %.dep,$(filter %.c,$(SRCS))))
DEPS += $(addprefix $(DEPDIR)/,$(patsubst %.s,   %.dep,$(filter %.s,$(SRCS))))
endif

# Dependency files are emitted atomically alongside changed objects. Building
# this target therefore updates only objects whose sources or headers changed.
dep: $(OBJS)

clean:
	rm -f $(OBJS) $(BINDIR)Overlay.* $(BINDIR)*.elf $(BINDIR)*.ps-exe $(BINDIR)*.map $(DEPS)

ifeq ($(SKIP_DEPS),)
ifneq ($(MAKECMDGOALS), clean)
ifneq ($(MAKECMDGOALS), deepclean)
-include $(DEPS)
endif
endif
endif

.PHONY: clean dep all
