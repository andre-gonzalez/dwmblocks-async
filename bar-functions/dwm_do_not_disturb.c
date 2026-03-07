/*
 * dwm_do_not_disturb - C replacement for dwm_do_not_disturb shell script
 * Displays DND status icon (with optional timer end-time) in dwmblocks.
 * Zero external forks in the display path. Single D-Bus round-trip.
 *
 * GNU GPLv3
 *
 * Dependencies: dunst, libsystemd (sd-bus)
 * Compile: gcc -Ofast -std=c99 -Wall -Wextra -Wpedantic \
 *          -o dwm_do_not_disturb_c dwm_do_not_disturb.c \
 *          $(pkg-config --cflags --libs libsystemd)
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

static const char *dest       = "org.freedesktop.Notifications";
static const char *obj        = "/org/freedesktop/Notifications";
static const char *prop_iface = "org.freedesktop.DBus.Properties";
static const char *cmd_iface  = "org.dunstproject.cmd0";

/* U+F1F6 in UTF-8 */
#define ICON "\xef\x87\xb6"

#define DND_TIMER_SECS 3600

static void build_path(char *buf, size_t len)
{
	const char *home = getenv("HOME");
	if (!home)
		home = "/tmp";
	snprintf(buf, len, "%s/.cache/do-not-disturb-end", home);
}

static int get_paused(sd_bus *bus)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	int val = 0;

	int r = sd_bus_call_method(bus, dest, obj, prop_iface, "Get",
				   &err, &reply, "ss", cmd_iface, "paused");
	sd_bus_error_free(&err);
	if (r < 0)
		return -1;

	if (sd_bus_message_enter_container(reply, 'v', "b") >= 0) {
		sd_bus_message_read(reply, "b", &val);
		sd_bus_message_exit_container(reply);
	}
	sd_bus_message_unref(reply);
	return val;
}

static int set_paused(sd_bus *bus, int val)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *m = NULL;

	int r = sd_bus_message_new_method_call(bus, &m, dest, obj,
					       prop_iface, "Set");
	if (r < 0)
		return r;

	sd_bus_message_append(m, "ss", cmd_iface, "paused");
	sd_bus_message_open_container(m, 'v', "b");
	sd_bus_message_append(m, "b", val);
	sd_bus_message_close_container(m);

	r = sd_bus_call(bus, m, 0, &err, NULL);
	sd_bus_error_free(&err);
	sd_bus_message_unref(m);
	return r;
}

static void signal_bar(void)
{
	system("pkill -RTMIN+14 dwmblocks");
}

static void handle_click(sd_bus *bus, char btn, const char *path)
{
	switch (btn) {
	case '1': {
		int paused = get_paused(bus);
		if (paused < 0)
			return;
		set_paused(bus, !paused);
		if (paused)
			unlink(path);
		signal_bar();
		break;
	}
	case '3': {
		set_paused(bus, 1);
		FILE *fp = fopen(path, "w");
		if (fp) {
			fprintf(fp, "%ld\n", (long)(time(NULL) + DND_TIMER_SECS));
			fclose(fp);
		}
		signal_bar();
		break;
	}
	}
}

int main(void)
{
	char path[512];
	build_path(path, sizeof(path));

	sd_bus *bus;
	if (sd_bus_open_user(&bus) < 0)
		return 0;

	const char *btn = getenv("BLOCK_BUTTON");
	if (btn && btn[0] != '\0') {
		handle_click(bus, btn[0], path);
		sd_bus_unref(bus);
		return 0;
	}

	int paused = get_paused(bus);
	sd_bus_unref(bus);

	if (paused <= 0)
		return 0;

	FILE *fp = fopen(path, "r");
	if (!fp) {
		puts(ICON);
		return 0;
	}

	long end_ts = 0;
	if (fscanf(fp, "%ld", &end_ts) != 1 || end_ts <= 0) {
		fclose(fp);
		puts(ICON);
		return 0;
	}
	fclose(fp);

	time_t t = (time_t)end_ts;
	struct tm *tm = localtime(&t);
	if (!tm) {
		puts(ICON);
		return 0;
	}

	char timebuf[6];
	strftime(timebuf, sizeof(timebuf), "%H:%M", tm);
	printf(ICON " %s\n", timebuf);

	return 0;
}
