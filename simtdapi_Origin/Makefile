APP = llc-simtdapi.so
all: $(APP)

# Comment/uncomment the following line to disable/enable debugging
DEBUG = n
ifeq ($(DEBUG),y)
  # "-O" is needed to expand inlines
  EXTRA_CFLAGS += -O -g -DDEBUG
else
  EXTRA_CFLAGS += -O2
endif

CC := "$(CROSS_COMPILE)gcc"
CC := $(subst $\",,$(CC))

ifeq ($(BOARD),mk2)
endif
ifeq ($(BOARD),mk3)
endif
ifeq ($(BOARD),mk4)
endif
ifeq ($(BOARD),mk5)
endif
ifeq ($(BOARD),sabre)
endif
ifeq ($(BOARD),v2x)
endif
ifeq ($(BOARD),ste)
endif
ifneq (,$(findstring $(BOARD),dgm qnx qnx-arm qnx-x86 sabrelite mq5))
  QNX_BASE ?= $(HOME)/qnx660
  CC = source $(QNX_BASE)/qnx660-env.sh; qcc
  ifneq (,$(findstring $(BOARD),dgm qnx-arm sabrelite mq5))
    QCC_OPT = gcc_ntoarmv7le
    TARGET = armle-v7
  else
    QCC_OPT = gcc_ntox86
    TARGET = x86
  endif
  CFLAGS = -V $(QCC_OPT) -DBOARD_QNX
  INSTALLDIR = $(HOME)/qnx
  COHDA_INCLUDE_DIR = ../../../../qnx/include
else
  # QNX doesn't support -nostartfiles
  CFLAGS = -nostartfiles
endif

COHDA_INCLUDE_DIR ?= ../../../../kernel/include

# Fallback defaults (host)
BOARD ?= $(shell uname --m)
INSTALLDIR ?= ~

CFLAGS += -Wall -Werror -MD -shared -fPIC \
         -fdata-sections -ffunction-sections

EXTRA_CFLAGS += -I. \
                -I../.. \
                -I../../lib \
                -I../../../pktbuf \
                -I$(COHDA_INCLUDE_DIR) \
                -D__LLC__

LDFLAGS += -L$(CURDIR)/../../lib -lLLC -lpcap

LIBS +=

SRCS = llc-simtdapi.c \
	llc-chconfig.c CHOpts.c \
	llc-test-rx.c RxOpts.c RxStats.c \
	llc-test-tx.c TxOpts.c \
	test-common.c

OBJS = $(SRCS:.c=.o)

DEPDIR = .deps
DEPS   = $(SRCS:%.c=$(DEPDIR)/%.d)

$(APP): $(LIBS) $(OBJS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(OBJS) $(LIBS) $(LDFLAGS) -o $@
	@cp $@ ../

%.o: %.c
	-@mkdir --parents $(shell dirname $(DEPDIR)/$*.d)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@
	@echo $@: Makefile >> $*.d
	@mv -f $*.d $(DEPDIR)/$*.d

clean:
	rm -f $(APP) $(OBJS) *.o *.so *.so.map
	rm -rf $(DEPDIR)/*
	rm -rf $(DEPDIR)

install: $(APP)
	sudo install -d $(INSTALLDIR)/opt/cohda/bin/plugin
	sudo install -c $(APP) $(INSTALLDIR)/opt/cohda/bin/plugin

