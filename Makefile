CC=gcc
WINDRES=windres
CFLAGS=-std=c99 -Wall -Wextra -O2 -march=pentium2 -fdata-sections -ffunction-sections
OBJ_SUFFIX = .o
EXE_NAME = softgpu
EXE_SUFFIX = .exe
LDFLAGS=-static
LIBS=-lsetupapi -lgdi32 -luser32 -ladvapi32 -lsetupapi -lkernel32 -lshell32 -Wl,-subsystem,windows
#LIBS=-lsetupapi -lgdi32 -luser32 -ladvapi32 -lsetupapi -lkernel32 -lshell32 -Wl,-subsystem,console

SOFTGPU_PATCH=2023

ifdef EXTRA_INFO
CFLAGS += -DEXTRA_INFO="\"$(EXTRA_INFO)\""
endif

NULLOUT=$(if $(filter $(OS),Windows_NT),NUL,/dev/null)

GIT      ?= git
GIT_IS   := $(shell $(GIT) rev-parse --is-inside-work-tree 2> $(NULLOUT))
ifeq ($(GIT_IS),true)
  VERSION_BUILD := $(shell $(GIT) rev-list --count main)
endif

NOCRT=1

all: $(EXE_NAME)$(EXE_SUFFIX)
.PHONY: all clean

SOURCES = \
  actions.c \
  filecopy.c \
  softgpu.c \
  winini.c \
  windrv.c \
  winreg.c \
  setuperr.c \
  resource/softgpu.rc
  

ifdef NOCRT
  SOURCES += \
    nocrt/nocrt.c \
    nocrt/nocrt_exe.c \
    nocrt/nocrt_file_win.c \
    nocrt/nocrt_mem_win.c \
    nocrt/nocrt_math.c

  LDFLAGS += -nostdlib -nodefaultlibs -lgcc
  CFLAGS  += -Inocrt -DNOCRT -DNOCRT_FILE -DNOCRT_FLOAT -DNOCRT_MEM -ffreestanding -nostdlib
endif

CFLAGS    += -DSOFTGPU_PATCH=$(SOFTGPU_PATCH)
RES_FLAGS += -DSOFTGPU_PATCH=$(SOFTGPU_PATCH)

ifdef VERSION_BUILD
  CFLAGS    += -DSOFTGPU_BUILD=$(VERSION_BUILD)
  RES_FLAGS += -DSOFTGPU_BUILD=$(VERSION_BUILD)
endif

%.c.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

%.rc.res: %.rc
	$(WINDRES) $(RES_FLAGS) --input $< --output $@ --output-format=coff

OBJS := $(SOURCES:.c=.c$(OBJ_SUFFIX))
OBJS := $(OBJS:.rc=.rc.res)

$(EXE_NAME)$(EXE_SUFFIX): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@ $(LIBS)

clean:
	-$(RM) $(OBJS)
	-$(RM) $(EXE_NAME)$(EXE_SUFFIX)
