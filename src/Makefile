TARGET = human.sys
XDF = ../dist/pong.xdf

CROSS = m68k-xelf-
CC = $(CROSS)gcc
LD = $(CROSS)gcc

CFLAGS = -m68000 -O2 -g
LDFLAGS = -Ttext=0x6800 -s -specs=x68knodos.specs

PYTHON ?= python3
CURL ?= curl

BUILD_DIR := build
XDF_ROOT := $(BUILD_DIR)/xdfroot
XDFTOOL := $(BUILD_DIR)/xdftool.py
XDFTOOL_URL := https://cdn.jsdelivr.net/gh/yunkya2/x68kmisc@08f647bad280dc8eac778f3f98358350d10e9471/xdftool/xdftool.py

.PHONY: all clean distclean check-tools check-xdf

all: check-tools $(TARGET) $(XDF)

check-tools:
	@command -v $(CC) >/dev/null 2>&1 || { \
		echo "Error: $(CC) was not found. Install elf2x68k and put m68k-xelf-* in PATH." >&2; \
		exit 1; \
	}
	@command -v $(PYTHON) >/dev/null 2>&1 || { \
		echo "Error: $(PYTHON) is not installed." >&2; \
		exit 1; \
	}
	@command -v $(CURL) >/dev/null 2>&1 || { \
		echo "Error: $(CURL) is not installed." >&2; \
		exit 1; \
	}

$(TARGET): pong.o
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

$(BUILD_DIR):
	mkdir -p $@

$(XDFTOOL): | $(BUILD_DIR)
	$(CURL) -L --fail --show-error -o $@ $(XDFTOOL_URL)
	chmod +x $@

$(XDF): Makefile $(TARGET) $(XDFTOOL)
	mkdir -p $(dir $(XDF))
	rm -rf $(XDF_ROOT)
	mkdir -p $(XDF_ROOT)
	cp $(TARGET) $(XDF_ROOT)/human.sys
	cd $(XDF_ROOT) && \
		$(PYTHON) ../xdftool.py c ../../$(XDF) human.sys

check-xdf: $(XDF) $(XDFTOOL)
	$(PYTHON) $(XDFTOOL) t $(XDF)

clean:
	-rm -f *.o *.elf* $(TARGET) $(XDF)
	-rm -rf $(XDF_ROOT)

distclean: clean
	-rm -rf $(BUILD_DIR)
