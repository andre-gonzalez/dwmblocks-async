/*
 * dwm_spotify - C replacement for dwm_spotify shell script
 * Shows the current artist, track, and status from Spotify via sd-bus (MPRIS2)
 * Zero external forks in the display path. Single D-Bus round-trip.
 *
 * Joe Standring <git@joestandring.com>
 * GNU GPLv3
 *
 * Dependencies: spotify/spotifyd/spotify_player, libsystemd (sd-bus)
 * Compile: gcc -Ofast -std=c99 -Wall -Wextra -Wpedantic \
 *          -o dwm_spotify_c dwm_spotify.c $(pkg-config --cflags --libs libsystemd)
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>

static const char *players[] = {
	"org.mpris.MediaPlayer2.spotify_player",
	"org.mpris.MediaPlayer2.spotify",
	"org.mpris.MediaPlayer2.spotifyd",
};
#define N_PLAYERS (sizeof(players) / sizeof(players[0]))

static const char *obj    = "/org/mpris/MediaPlayer2";
static const char *piface = "org.mpris.MediaPlayer2.Player";
static const char *prop_iface = "org.freedesktop.DBus.Properties";

/*
 * Single GetAll call: returns all Player properties including
 * PlaybackStatus and Metadata in one D-Bus round-trip.
 */
static int get_all(sd_bus *bus, const char *dest,
		   char *status, size_t slen,
		   char *artist, size_t alen,
		   char *title, size_t tlen)
{
	sd_bus_error err = SD_BUS_ERROR_NULL;
	sd_bus_message *reply = NULL;
	int r;

	status[0] = '\0';
	artist[0] = '\0';
	title[0] = '\0';

	r = sd_bus_call_method(bus, dest, obj, prop_iface, "GetAll",
			       &err, &reply, "s", piface);
	sd_bus_error_free(&err);
	if (r < 0)
		return r;

	/* Reply signature: a{sv} */
	r = sd_bus_message_enter_container(reply, 'a', "{sv}");
	if (r < 0)
		goto done;

	while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
		const char *key;
		if (sd_bus_message_read(reply, "s", &key) < 0)
			break;

		if (strcmp(key, "PlaybackStatus") == 0) {
			if (sd_bus_message_enter_container(reply, 'v', "s") >= 0) {
				const char *val;
				if (sd_bus_message_read(reply, "s", &val) > 0)
					snprintf(status, slen, "%s", val);
				sd_bus_message_exit_container(reply);
			}
		} else if (strcmp(key, "Metadata") == 0) {
			if (sd_bus_message_enter_container(reply, 'v', "a{sv}") >= 0) {
				if (sd_bus_message_enter_container(reply, 'a', "{sv}") >= 0) {
					while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
						const char *mkey;
						if (sd_bus_message_read(reply, "s", &mkey) < 0)
							break;

						if (strcmp(mkey, "xesam:title") == 0) {
							if (sd_bus_message_enter_container(reply, 'v', "s") >= 0) {
								const char *val;
								if (sd_bus_message_read(reply, "s", &val) > 0)
									snprintf(title, tlen, "%s", val);
								sd_bus_message_exit_container(reply);
							}
						} else if (strcmp(mkey, "xesam:artist") == 0) {
							if (sd_bus_message_enter_container(reply, 'v', "as") >= 0) {
								if (sd_bus_message_enter_container(reply, 'a', "s") >= 0) {
									const char *val;
									if (sd_bus_message_read(reply, "s", &val) > 0)
										snprintf(artist, alen, "%s", val);
									sd_bus_message_exit_container(reply);
								}
								sd_bus_message_exit_container(reply);
							}
						} else {
							sd_bus_message_skip(reply, "v");
						}
						sd_bus_message_exit_container(reply);
					}
					sd_bus_message_exit_container(reply);
				}
				sd_bus_message_exit_container(reply);
			}
		} else {
			sd_bus_message_skip(reply, "v");
		}

		sd_bus_message_exit_container(reply);
	}

done:
	sd_bus_message_unref(reply);
	return (status[0] != '\0') ? 0 : -1;
}

static void handle_click(sd_bus *bus, const char *dest, const char *btn)
{
	const char *method = NULL;
	switch (btn[0]) {
	case '1': method = "PlayPause"; break;
	case '2':
		system("st -e tmux new-session -A -s spotify spotify_player");
		return;
	case '3': /* fall through */
	case '4': method = "Next"; break;
	case '5': method = "Previous"; break;
	default:  return;
	}
	if (method)
		sd_bus_call_method(bus, dest, obj, piface, method, NULL, NULL, "");
}

int main(void)
{
	sd_bus *bus;
	if (sd_bus_open_user(&bus) < 0)
		return 0;

	const char *dest = NULL;
	char status[32], artist[256], title[256];

	for (size_t i = 0; i < N_PLAYERS; i++) {
		if (get_all(bus, players[i], status, sizeof(status),
			    artist, sizeof(artist), title, sizeof(title)) == 0) {
			dest = players[i];
			break;
		}
	}

	if (!dest) {
		sd_bus_unref(bus);
		return 0;
	}

	const char *btn = getenv("BLOCK_BUTTON");
	if (btn && btn[0] != '\0')
		handle_click(bus, dest, btn);

	sd_bus_unref(bus);

	const char *icon = strcmp(status, "Playing") == 0 ? "\xe2\x96\xb6" : "\xef\x81\x8c";

	if (artist[0] == '\0')
		printf("%s %s", icon, title);
	else
		printf("%s %s - %s", icon, artist, title);

	return 0;
}
