/*
 * dwm_cpu - C replacement for dwm_cpu shell script
 * Displays CPU usage icon when above threshold.
 * Uses /proc/stat delta with cached previous snapshot. Zero forks.
 *
 * GNU GPLv3
 *
 * Compile: gcc -Ofast -std=c99 -Wall -Wextra -Wpedantic -o dwm_cpu_c dwm_cpu.c
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAT_FILE "/tmp/dwm_cpu_stat"

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
		case '1': system("st -e tmux new-session -A -s btop_cpu btop -p 1 &"); return 0;
		case '3': system("notify-send 'Top CPU Processes' \"$(ps axo pid,comm,%cpu --sort=-%cpu | head -n 6)\""); return 0;
		}
	}

	/* Read /proc/stat first line: cpu user nice system idle iowait irq softirq steal */
	FILE *fp = fopen("/proc/stat", "r");
	if (!fp)
		return 0;

	long user, nice, system_, idle, iowait, irq, softirq, steal;
	if (fscanf(fp, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
		   &user, &nice, &system_, &idle, &iowait, &irq, &softirq, &steal) != 8) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	long total = user + nice + system_ + idle + iowait + irq + softirq + steal;

	/* Read previous snapshot */
	long prev_total = 0, prev_idle = 0;
	fp = fopen(STAT_FILE, "r");
	if (fp) {
		fscanf(fp, "%ld %ld", &prev_total, &prev_idle);
		fclose(fp);
	}

	/* Save current snapshot */
	fp = fopen(STAT_FILE, "w");
	if (fp) {
		fprintf(fp, "%ld %ld\n", total, idle);
		fclose(fp);
	}

	long d_total = total - prev_total;
	long d_idle = idle - prev_idle;

	if (d_total <= 0)
		return 0;

	int cpu_pct = (int)((d_total - d_idle) * 100 / d_total);

	int threshold = 70;
	char bat_status[32];
	if (read_battery_status(bat_status, sizeof(bat_status)) == 0 &&
	    strcmp(bat_status, "Discharging") == 0)
		threshold = 20;

	if (cpu_pct >= threshold)
		printf(" \xef\x92\xbc %d%%\n", cpu_pct);

	return 0;
}
