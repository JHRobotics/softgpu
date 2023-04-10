CC=gcc
WINDRES=windres
CFLAGS=-std=c99 -Wall -Wextra -O2 -march=pentium2 -fdata-sections -ffunction-sections
OBJ_SUFFIX = .o
EXE_NAME = softgpu
EXE_SUFFIX = .exe
LDFLAGS=-static
LIBS=-lsetupapi -lgdi32 -luser32 -ladvapi32 -lsetupapi -lkernel32 -lshell32 -Wl,-subsystem,windows

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
