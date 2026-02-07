#ifndef CONFIG_H
#define CONFIG_H

// String used to delimit block outputs in the status.
#define DELIMITER " · "

// Maximum number of Unicode characters that a block can output.
#define MAX_BLOCK_OUTPUT_LENGTH 45

// Control whether blocks are clickable.
#define CLICKABLE_BLOCKS 1

// Control whether a leading delimiter should be prepended to the status.
#define LEADING_DELIMITER 0

// Control whether a trailing delimiter should be appended to the status.
#define TRAILING_DELIMITER 0

// Define blocks for the status feed as X(icon, cmd, interval, signal).
// To force update on one add 34 to the number
#define BLOCKS(X)             \
	X("",	"dwm_spotify", 3,  0) \
	X("",	"dwm_currency", 1800,  1) \
	X("",	"dwm_countdown", 1,  0) \
	X("",	"dwm_vpn", 10,  0) \
	X("",	"dwm_systemd_networkd", 5,  0) \
	X("",	"dwm_ufw",	60, 0) \
	X("",	"dwm_storage", 14400,  0) \
	X("",	"dwm_memory", 5,  0) \
	X("",	"dwm_cpu", 5,  0) \
	X("",	"dwm_packages",	14400, 13) \
	X("",	"dwm_battery", 300,  0) \
	X("",	"dwm_bluetooth", 60, 12) \
	X("",	"dwm_pulse", 0, 10) \
	X("",	"dwm_backlight", 0, 11) \
	X("",	"dwm_do_not_disturb", 0,  14) \
	X("",	"dwm_date", 30,  0) \

#endif  // CONFIG_H
