TARGET    = nandbch
LINUX_DIR = ./linux
OBJECTS   = $(patsubst %.c,%.o,$(wildcard *.c))
OBJECTS   += $(LINUX_DIR)/bch.o

ARCH ?= x86
ifeq (${ARCH},x86)
	CROSS_COMPILE =
	OUT_DIR = out_x86
endif
ifeq (${ARCH},arm)
	CROSS_COMPILE ?= arm-buildroot-linux-uclibcgnueabihf-
	OUT_DIR = out_arm
endif

ifndef V
QUIET_CC    = @echo '  CC       '$@;
QUIET_LINK  = @echo '  LINK     '$@;
QUIET_STRIP = @echo '  STRIP    '$@;
endif

CC      = $(QUIET_CC)$(CROSS_COMPILE)gcc
LD      = $(QUIET_LINK)$(CROSS_COMPILE)gcc
STRIP   = $(QUIET_STRIP)$(CROSS_COMPILE)strip
CFLAGS  = -Wall -Werror -O3 -I./include -I$(LINUX_DIR)
LDFLAGS = -ldl

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $(CFLAGS) ${LDFLAGS} $(OBJECTS) -o $@
	$(STRIP) $@
	@mkdir -p ./$(OUT_DIR)
	@cp $@ ./$(OUT_DIR)

clean:
	-rm -f $(TARGET) *.o $(LINUX_DIR)/*.o *.map

distclean: clean
	-rm -rf out_*
