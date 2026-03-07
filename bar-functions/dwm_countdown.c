/*
 * dwm_countdown - C replacement for dwm_countdown shell script
 * Displays the status of countdown.sh in dwmblocks
 *
 * Joe Standring <git@joestandring.com>
 * GNU GPLv3
 *
 * Dependencies: https://github.com/joestandring/countdown
 */

#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	glob_t g;
	if (glob("/tmp/countdown.*", 0, NULL, &g) != 0 || g.gl_pathc == 0) {
		globfree(&g);
		return 0;
	}

	const char *path = g.gl_pathv[0];

	const char *btn = getenv("BLOCK_BUTTON");
	if (btn && btn[0] == '1' && btn[1] == '\0') {
		system("killall countdown");
		unlink(path);
		system("notify-send \"Countdown\" \"Cancelled\"");
		globfree(&g);
		return 0;
	}

	FILE *fp = fopen(path, "r");
	globfree(&g);
	if (!fp)
		return 0;

	char last[256];
	if (!fgets(last, sizeof(last), fp)) {
		fclose(fp);
		return 0;
	}
	fclose(fp);
	last[strcspn(last, "\n")] = '\0';

	if (last[0] == '\0')
		return 0;

	const char *sep1 = getenv("SEP1");
	const char *sep2 = getenv("SEP2");
	const char *ident = getenv("IDENTIFIER");

	if (!sep1) sep1 = "";
	if (!sep2) sep2 = "";

	if (ident && strcmp(ident, "unicode") == 0)
		printf("%s⏳ %s%s\n", sep1, last, sep2);
	else
		printf("%sCDN %s%s\n", sep1, last, sep2);

	return 0;
}
