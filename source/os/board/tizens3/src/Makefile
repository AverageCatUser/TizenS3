############################################################################
# board/tizens3/src/Makefile
#
# Licensed under the Apache License, Version 2.0.
############################################################################

-include $(TOPDIR)/Make.defs

DEPPATH = --dep-path .

ASRCS =
CSRCS = tizens3_boot.c

ifeq ($(CONFIG_FLASH_PARTITION),y)
CSRCS += partitions.c
endif

VPATH += ../../common
DEPPATH += --dep-path ../../common

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS)
OBJS = $(AOBJS) $(COBJS)

ARCH_SRCDIR = $(TOPDIR)/arch/$(CONFIG_ARCH)/src
BOARD_DIR = $(TOPDIR)/board/$(CONFIG_ARCH_BOARD)

CFLAGS += -I$(ARCH_SRCDIR)/chip
CFLAGS += -I$(ARCH_SRCDIR)/common
CFLAGS += -I$(ARCH_SRCDIR)/xtensa
CFLAGS += -I$(BOARD_DIR)/include
CFLAGS += -I$(TOPDIR)/board/common

all: libboard$(LIBEXT)

$(AOBJS): %$(OBJEXT): %.S
	$(call ASSEMBLE, $<, $@)

$(COBJS) $(LINKOBJS): %$(OBJEXT): %.c
	$(call COMPILE, $<, $@)

libboard$(LIBEXT): $(OBJS)
	$(call ARCHIVE, $@, $(OBJS))

.depend: Makefile $(SRCS)
	$(Q) $(MKDEP) $(DEPPATH) $(CC) -- $(CFLAGS) -- $(SRCS) >Make.dep
	$(Q) touch $@

depend: .depend

context:

clean:
	$(call DELFILE, libboard$(LIBEXT))
	$(call CLEAN)

distclean: clean
	$(call DELFILE, Make.dep)
	$(call DELFILE, .depend)

-include Make.dep
