/*
 * dwm_kdeconnect - phone battery from KDE Connect, for dwmblocks
 *
 * Display path: prints the charge of the first reachable paired device that
 * reports a battery, but only below THRESHOLD_HIDE. Prints a link-off icon
 * whenever the link is not usable (daemon down, nothing reachable, no battery
 * data), so a broken link is always visible. Never activates kdeconnectd:
 * the bus name is D-Bus activatable, so ownership is checked first.
 *
 * Watch path (--watch): blocks on the session bus and pokes dwmblocks whenever
 * the battery, the device visibility, or the daemon itself changes.
 *
 * GNU GPLv3
 *
 * Dependencies: kdeconnect, libsystemd (sd-bus), libnotify (notify-send)
 * Compile: gcc -Ofast -std=c99 -Wall -Wextra -Wpedantic \
 *          -o dwm_kdeconnect_c dwm_kdeconnect.c \
 *          $(pkg-config --cflags --libs libsystemd)
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

static const char *dest         = "org.kde.kdeconnect";
static const char *daemon_obj   = "/modules/kdeconnect";
static const char *daemon_iface = "org.kde.kdeconnect.daemon";
static const char *device_iface = "org.kde.kdeconnect.device";
static const char *bat_iface    = "org.kde.kdeconnect.device.battery";
static const char *fmp_iface    = "org.kde.kdeconnect.device.findmyphone";

/* U+F011C nf-md-cellphone, U+F0122 nf-md-cellphone_link_off */
#define ICON     "\xf3\xb0\x84\x9c"
#define ICON_OFF "\xf3\xb0\x84\xa2"

/* Hide the reading above this, warn below the next one, rearm above the last */
#define THRESHOLD_HIDE 70
#define THRESHOLD_LOW  50
#define THRESHOLD_REARM 55

#define REFRESH_CMD "pkill -RTMIN+17 dwmblocks"

typedef struct {
	char id[128];
	char path[192];
	int charge;
	int charging;
} device;

static void signal_bar(void)
{
	(void)system(REFRESH_CMD);
}

/* Run a command detached, without a shell, so device names stay inert data. */
static void spawn(const char *file, char *const argv[])
{
	pid_t pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			execvp(file, argv);
			_exit(127);
		}
		_exit(0);
	} else if (pid > 0) {
		(void)waitpid(pid, NULL, 0);
	}
}

static void notify(const char *urgency, const char *title, const char *body)
{
	char *argv[] = {"notify-send", "-u", (char *)urgency, (char *)title,
			(char *)body, NULL};
	spawn("notify-send", argv);
}

static void low_flag_path(char *buf, size_t len)
{
	const char *home = getenv("HOME");
	if (!home)
		home = "/tmp";
	snprintf(buf, len, "%s/.cache/dwm_kdeconnect_low", home);
}

/* The bus name is activatable; asking the broker avoids starting the daemon. */
static int daemon_running(sd_bus *bus)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	int owned = 0;

	int r = sd_bus_call_method(bus, "org.freedesktop.DBus",
				   "/org/freedesktop/DBus",
				   "org.freedesktop.DBus", "NameHasOwner",
				   &err, &reply, "s", dest);
	if (r >= 0)
		sd_bus_message_read(reply, "b", &owned);

	sd_bus_error_free(&err);
	sd_bus_message_unref(reply);
	return owned;
}

/* Reachable (and optionally only paired) device ids, in daemon order. */
static char **device_ids(sd_bus *bus, int only_reachable)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	char **ids = NULL;

	int r = sd_bus_call_method(bus, dest, daemon_obj, daemon_iface,
				   "devices", &err, &reply, "bb",
				   only_reachable, 1);
	if (r >= 0 && sd_bus_message_read_strv(reply, &ids) < 0)
		ids = NULL;

	sd_bus_error_free(&err);
	sd_bus_message_unref(reply);
	return ids;
}

static void free_strv(char **strv)
{
	if (!strv)
		return;
	for (char **s = strv; *s; ++s)
		free(*s);
	free(strv);
}

static int device_string(sd_bus *bus, const char *path, const char *iface,
			 const char *prop, char *buf, size_t len)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	char *val = NULL;

	int r = sd_bus_get_property_string(bus, dest, path, iface, prop, &err,
					   &val);
	if (r >= 0 && val)
		snprintf(buf, len, "%s", val);

	free(val);
	sd_bus_error_free(&err);
	return r;
}

/* First reachable device that actually reports a battery. */
static int find_device(sd_bus *bus, device *dev)
{
	char **ids = device_ids(bus, 1);
	int found = 0;

	for (char **id = ids; ids && *id && !found; ++id) {
		sd_bus_error err = SD_BUS_ERROR_NULL;
		char path[192];
		int has_battery = 0;

		snprintf(path, sizeof(path), "%s/devices/%s/battery",
			 daemon_obj, *id);

		if (sd_bus_get_property_trivial(bus, dest, path, bat_iface,
						"hasBattery", &err, 'b',
						&has_battery) < 0 ||
		    !has_battery) {
			sd_bus_error_free(&err);
			continue;
		}
		sd_bus_error_free(&err);

		dev->charge = -1;
		dev->charging = 0;
		sd_bus_get_property_trivial(bus, dest, path, bat_iface,
					    "charge", &err, 'i', &dev->charge);
		sd_bus_error_free(&err);
		sd_bus_get_property_trivial(bus, dest, path, bat_iface,
					    "isCharging", &err, 'b',
					    &dev->charging);
		sd_bus_error_free(&err);

		if (dev->charge < 0)
			continue;

		snprintf(dev->id, sizeof(dev->id), "%s", *id);
		snprintf(dev->path, sizeof(dev->path), "%s/devices/%s",
			 daemon_obj, *id);
		found = 1;
	}

	free_strv(ids);
	return found;
}

/*
 * Warn once per discharge cycle: fire below THRESHOLD_LOW while discharging,
 * rearm only above THRESHOLD_REARM so a phone idling at the boundary is quiet.
 */
static void check_low(sd_bus *bus, const device *dev)
{
	char flag[512];
	low_flag_path(flag, sizeof(flag));

	int armed = access(flag, F_OK) != 0;

	if (armed && !dev->charging && dev->charge < THRESHOLD_LOW) {
		char name[128] = "Phone";
		char body[192];

		device_string(bus, dev->path, device_iface, "name", name,
			      sizeof(name));
		snprintf(body, sizeof(body), "%s at %d%%", name, dev->charge);
		notify("critical", "Phone battery low", body);

		FILE *fp = fopen(flag, "w");
		if (fp)
			fclose(fp);
	} else if (!armed && dev->charge >= THRESHOLD_REARM) {
		unlink(flag);
	}
}

static void notify_details(sd_bus *bus, const device *dev, int have_device)
{
	char body[512];

	if (have_device) {
		sd_bus_error err = SD_BUS_ERROR_NULL;
		char name[128] = "Phone";
		char **addrs = NULL, **providers = NULL;
		char link[192] = "";

		device_string(bus, dev->path, device_iface, "name", name,
			      sizeof(name));

		sd_bus_get_property_strv(bus, dest, dev->path, device_iface,
					 "activeProviderNames", &err,
					 &providers);
		sd_bus_error_free(&err);
		sd_bus_get_property_strv(bus, dest, dev->path, device_iface,
					 "reachableAddresses", &err, &addrs);
		sd_bus_error_free(&err);

		snprintf(link, sizeof(link), "%s %s",
			 (providers && providers[0]) ? providers[0] : "?",
			 (addrs && addrs[0]) ? addrs[0] : "");
		free_strv(providers);
		free_strv(addrs);

		snprintf(body, sizeof(body),
			 "Charge: %d%%\nStatus: %s\nLink: %s", dev->charge,
			 dev->charging ? "Charging" : "Discharging", link);
		notify("normal", name, body);
		return;
	}

	if (!daemon_running(bus)) {
		notify("normal", "KDE Connect", "Daemon is not running");
		return;
	}

	char **ids = device_ids(bus, 0);
	char paired[384] = "";
	size_t used = 0;

	for (char **id = ids; ids && *id; ++id) {
		char path[192];
		char name[128] = "";

		snprintf(path, sizeof(path), "%s/devices/%s", daemon_obj, *id);
		if (device_string(bus, path, device_iface, "name", name,
				  sizeof(name)) < 0 || name[0] == '\0')
			continue;

		used += (size_t)snprintf(paired + used, sizeof(paired) - used,
					 "%s%s", used ? ", " : "", name);
		if (used >= sizeof(paired))
			break;
	}
	free_strv(ids);

	snprintf(body, sizeof(body), "No device reachable\nPaired: %s",
		 paired[0] ? paired : "none");
	notify("normal", "KDE Connect", body);
}

static void ring(sd_bus *bus, const device *dev)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	char path[256];

	snprintf(path, sizeof(path), "%s/findmyphone", dev->path);
	sd_bus_call_method(bus, dest, path, fmp_iface, "ring", &err, NULL, "");
	sd_bus_error_free(&err);
}

static void handle_click(sd_bus *bus, char btn, const device *dev,
			 int have_device)
{
	switch (btn) {
	case '1':
		notify_details(bus, dev, have_device);
		break;
	case '2': {
		char *argv[] = {"kdeconnect-app", NULL};
		spawn("kdeconnect-app", argv);
		break;
	}
	case '3':
		if (have_device)
			ring(bus, dev);
		break;
	}
}

static int on_change(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
	(void)m;
	(void)userdata;
	(void)err;
	signal_bar();
	return 0;
}

/*
 * KDE Connect emits its own refreshed() on the battery interface; standard
 * PropertiesChanged is not guaranteed from the Qt adapter, so match both.
 */
static int watch(sd_bus *bus)
{
	static const char *const matches[] = {
		"type='signal',interface='org.kde.kdeconnect.device.battery',"
		"member='refreshed'",
		"type='signal',interface='org.freedesktop.DBus.Properties',"
		"member='PropertiesChanged',"
		"arg0='org.kde.kdeconnect.device.battery'",
		"type='signal',interface='org.kde.kdeconnect.daemon',"
		"member='deviceVisibilityChanged'",
		"type='signal',sender='org.freedesktop.DBus',"
		"interface='org.freedesktop.DBus',member='NameOwnerChanged',"
		"arg0='org.kde.kdeconnect'",
	};

	for (size_t i = 0; i < sizeof(matches) / sizeof(*matches); ++i) {
		if (sd_bus_add_match(bus, NULL, matches[i], on_change, NULL) <
		    0) {
			(void)fprintf(stderr,
				      "error: could not add match rule\n");
			return 1;
		}
	}

	for (;;) {
		int r = sd_bus_process(bus, NULL);
		if (r < 0)
			return 1;
		if (r > 0)
			continue;
		if (sd_bus_wait(bus, (uint64_t)-1) < 0)
			return 1;
	}
}

int main(int argc, char *argv[])
{
	sd_bus *bus;
	if (sd_bus_open_user(&bus) < 0)
		return 0;

	if (argc > 1 && strcmp(argv[1], "--watch") == 0) {
		int r = watch(bus);
		sd_bus_unref(bus);
		return r;
	}

	device dev = {.charge = -1};
	int have_device = daemon_running(bus) && find_device(bus, &dev);

	const char *btn = getenv("BLOCK_BUTTON");
	if (btn && btn[0] != '\0')
		handle_click(bus, btn[0], &dev, have_device);

	if (!have_device) {
		sd_bus_unref(bus);
		puts(ICON_OFF);
		return 0;
	}

	check_low(bus, &dev);
	sd_bus_unref(bus);

	if (dev.charge >= THRESHOLD_HIDE)
		return 0;

	if (dev.charging)
		printf(ICON " %d +\n", dev.charge);
	else
		printf(ICON " %d\n", dev.charge);

	return 0;
}
