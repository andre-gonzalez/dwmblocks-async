.POSIX:

BIN := dwmblocks
BUILD_DIR := build
SRC_DIR := src
INC_DIR := include

DEBUG := 0
VERBOSE := 0
LIBS := xcb-atom

PREFIX := /usr/local
CFLAGS := -Ofast -I. -I$(INC_DIR) -std=c99
CFLAGS += -DBINARY=\"$(BIN)\" -D_POSIX_C_SOURCE=200809L
CFLAGS += -Wall -Wpedantic -Wextra -Wswitch-enum
CFLAGS += $(shell pkg-config --cflags $(LIBS))
LDLIBS := $(shell pkg-config --libs $(LIBS))

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(subst $(SRC_DIR)/,$(BUILD_DIR)/,$(SRCS:.c=.o))

INSTALL_DIR := $(DESTDIR)$(PREFIX)/bin
SERVICES_DIR := services
SERVICE_NAME := dwmblocks-bluetooth

# Bar-function C replacements
BAR_DIR := bar-functions
BAR_CFLAGS := -Ofast -std=c99 -Wall -Wpedantic -Wextra

BAR_PLAIN := dwm_countdown dwm_systemd_networkd dwm_memory dwm_cpu
BAR_PLAIN_BINS := $(addprefix $(BAR_DIR)/,$(addsuffix _c,$(BAR_PLAIN)))

BAR_SDBUS := dwm_spotify dwm_do_not_disturb
BAR_SDBUS_BINS := $(addprefix $(BAR_DIR)/,$(addsuffix _c,$(BAR_SDBUS)))
BAR_SDBUS_CFLAGS := $(shell pkg-config --cflags libsystemd)
BAR_SDBUS_LDLIBS := $(shell pkg-config --libs libsystemd)

BAR_BINS := $(BAR_PLAIN_BINS) $(BAR_SDBUS_BINS)
BAR_NAMES := $(BAR_PLAIN) $(BAR_SDBUS)

# Shell-only bar scripts (no C replacement)
BAR_SHELL := dwm_currency dwm_vpn dwm_ufw dwm_storage dwm_packages \
             dwm_battery dwm_bluetooth dwm_pulse dwm_backlight dwm_date

# Prettify output
PRINTF := @printf "%-8s %s\n"
ifeq ($(VERBOSE), 0)
	Q := @
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -g
endif

all: $(BUILD_DIR)/$(BIN) bar-functions

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c config.h
	$Qmkdir -p $(@D)
	$(PRINTF) "CC" $@
	$Q$(COMPILE.c) -o $@ $<

$(BUILD_DIR)/$(BIN): $(OBJS)
	$(PRINTF) "LD" $@
	$Q$(LINK.o) $^ $(LDLIBS) -o $@

# Bar-function targets
bar-functions: $(BAR_BINS)

$(BAR_PLAIN_BINS): $(BAR_DIR)/%_c: $(BAR_DIR)/%.c
	$(PRINTF) "CC" $@
	$Q$(CC) $(BAR_CFLAGS) -o $@ $<
	$Qstrip $@
	$Qchmod 755 $@

$(BAR_SDBUS_BINS): $(BAR_DIR)/%_c: $(BAR_DIR)/%.c
	$(PRINTF) "CC" $@
	$Q$(CC) $(BAR_CFLAGS) $(BAR_SDBUS_CFLAGS) -o $@ $< $(BAR_SDBUS_LDLIBS)
	$Qstrip $@
	$Qchmod 755 $@

clean:
	$(PRINTF) "CLEAN" $(BUILD_DIR)
	$Q$(RM) $(BUILD_DIR)/*
	$(PRINTF) "CLEAN" "$(BAR_DIR)/*_c"
	$Q$(RM) $(BAR_BINS)

install: $(BUILD_DIR)/$(BIN) bar-functions
	$(PRINTF) "INSTALL" $(INSTALL_DIR)/$(BIN)
	$Qinstall -D -m 755 $< $(INSTALL_DIR)/$(BIN)
	$Q$(foreach name,$(BAR_NAMES),\
		printf "%-8s %s\n" "SYMLINK" $(INSTALL_DIR)/$(name); \
		ln -sf $(CURDIR)/$(BAR_DIR)/$(name)_c $(INSTALL_DIR)/$(name); \
	)
	$Q$(foreach name,$(BAR_SHELL),\
		printf "%-8s %s\n" "INSTALL" $(INSTALL_DIR)/$(name); \
		install -m 755 $(BAR_DIR)/$(name) $(INSTALL_DIR)/$(name); \
	)
	$(PRINTF) "INSTALL" $(INSTALL_DIR)/$(SERVICE_NAME)
	$Qinstall -m 755 $(SERVICES_DIR)/$(SERVICE_NAME) $(INSTALL_DIR)/$(SERVICE_NAME)
	@echo ""
	@echo "Run 'make enable-service' (without sudo) to install and enable the bluetooth monitor service."

enable-service:
	$(PRINTF) "INSTALL" $(HOME)/.config/systemd/user/$(SERVICE_NAME).service
	$Qinstall -D -m 644 $(SERVICES_DIR)/$(SERVICE_NAME).service $(HOME)/.config/systemd/user/$(SERVICE_NAME).service
	$(PRINTF) "RELOAD" "systemd user daemon"
	$Qsystemctl --user daemon-reload
	$(PRINTF) "ENABLE" $(SERVICE_NAME).service
	$Qsystemctl --user enable --now $(SERVICE_NAME).service

uninstall:
	$(PRINTF) "RM" $(INSTALL_DIR)/$(BIN)
	$Q$(RM) $(INSTALL_DIR)/$(BIN)
	$Q$(foreach name,$(BAR_NAMES),\
		printf "%-8s %s\n" "RM" $(INSTALL_DIR)/$(name); \
		$(RM) $(INSTALL_DIR)/$(name); \
	)
	$Q$(foreach name,$(BAR_SHELL),\
		printf "%-8s %s\n" "RM" $(INSTALL_DIR)/$(name); \
		$(RM) $(INSTALL_DIR)/$(name); \
	)
	$(PRINTF) "RM" $(INSTALL_DIR)/$(SERVICE_NAME)
	$Q$(RM) $(INSTALL_DIR)/$(SERVICE_NAME)
	@echo ""
	@echo "Run 'make disable-service' (without sudo) to stop, disable, and remove the bluetooth monitor service."

disable-service:
	$(PRINTF) "DISABLE" $(SERVICE_NAME).service
	$Q-systemctl --user disable --now $(SERVICE_NAME).service
	$(PRINTF) "RM" $(HOME)/.config/systemd/user/$(SERVICE_NAME).service
	$Q$(RM) $(HOME)/.config/systemd/user/$(SERVICE_NAME).service
	$(PRINTF) "RELOAD" "systemd user daemon"
	$Qsystemctl --user daemon-reload

.PHONY: all bar-functions clean install uninstall enable-service disable-service
