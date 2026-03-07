/*
 * dwm_memory - C replacement for dwm_memory shell script
 * Displays memory usage icon when above threshold.
 * Zero forks. Reads /proc/meminfo and battery sysfs directly.
 *
 * GNU GPLv3
 *
 * Compile: gcc -Ofast -std=c99 -Wall -Wextra -Wpedantic -o dwm_memory_c dwm_memory.c
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_battery_status(char *buf, size_t len)
{
	const char *paths[] = {
		"/sys/class/power_supply/BAT1/status",
		"/sys/class/power_supply/BAT0/status",
	};
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
		FILE *fp = fopen(paths[i], "r");
		if (fp) {
			if (fgets(buf, (int)len, fp)) {
				buf[strcspn(buf, "\n")] = '\0';
				fclose(fp);
				return 0;
			}
			fclose(fp);
		}
	}
	return -1;
}

int main(void)
{
	const char *btn = getenv("BLOCK_BUTTON");
	if (btn) {
		switch (btn[0]) {
		case '1': system("st -e tmux new-session -A -s btop_memory btop -p 4"); break;
		case '3': system("notify-send 'Top Memory Processes' \"$(ps axo pid,comm,%mem --sort=-%mem | head -n 6)\""); break;
		}
	}

	FILE *fp = fopen("/proc/meminfo", "r");
	if (!fp)
		return 0;

	long total = 0, avail = 0;
	char line[128];
	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, "MemTotal:", 9) == 0)
			sscanf(line + 9, " %ld", &total);
		else if (strncmp(line, "MemAvailable:", 13) == 0) {
			sscanf(line + 13, " %ld", &avail);
			break;
		}
	}
	fclose(fp);

	if (total == 0)
		return 0;

	long used = total - avail;
	int pct = (int)(used * 100 / total);

	int threshold = 75;
	char bat_status[32];
	if (read_battery_status(bat_status, sizeof(bat_status)) == 0 &&
	    strcmp(bat_status, "Discharging") == 0)
		threshold = 15;

	if (pct <= threshold)
		return 0;

	long gb = used / 1048576;
	if (gb >= 1)
		printf(" %ldG\n", gb);
	else
		printf(" %ldM\n", used / 1024);

	return 0;
}
