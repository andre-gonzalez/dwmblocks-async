/*
 * dwm_systemd_networkd - C replacement for dwm_systemd_networkd.sh
 * Displays network status icon in dwmblocks.
 * Zero external forks in the display path.
 *
 * GNU GPLv3
 *
 * Compile: gcc -Ofast -std=c99 -Wall -Wextra -Wpedantic \
 *          -o dwm_systemd_networkd_c dwm_systemd_networkd.c
 */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/wireless.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define WLAN_IF "wlan0"

static const char *home_networks[] = { "Davi", "CasaRio_5G", "Que Wifi?" };
#define N_HOME (sizeof(home_networks) / sizeof(home_networks[0]))

static int read_sysfs(const char *path, char *buf, size_t len)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;
	ssize_t n = read(fd, buf, len - 1);
	close(fd);
	if (n <= 0)
		return -1;
	buf[n] = '\0';
	/* strip trailing newline */
	if (n > 0 && buf[n - 1] == '\n')
		buf[n - 1] = '\0';
	return 0;
}

static int check_wired(void)
{
	DIR *dir = opendir("/sys/class/net");
	if (!dir)
		return 0;
	struct dirent *ent;
	char path[512], state[32];
	while ((ent = readdir(dir))) {
		if (strncmp(ent->d_name, "enp", 3) != 0)
			continue;
		snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", ent->d_name);
		if (read_sysfs(path, state, sizeof(state)) == 0 && strcmp(state, "up") == 0) {
			closedir(dir);
			return 1;
		}
	}
	closedir(dir);
	return 0;
}

static int get_wifi_ssid(const char *ifname, char *ssid, size_t len)
{
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return -1;
	struct iwreq wrq;
	memset(&wrq, 0, sizeof(wrq));
	snprintf(wrq.ifr_name, IFNAMSIZ, "%s", ifname);
	wrq.u.essid.pointer = ssid;
	wrq.u.essid.length = len;
	int r = ioctl(sock, SIOCGIWESSID, &wrq);
	close(sock);
	if (r < 0)
		return -1;
	ssid[wrq.u.essid.length] = '\0';
	return (wrq.u.essid.length > 0) ? 0 : -1;
}

static int get_wifi_signal(const char *ifname)
{
	FILE *fp = fopen("/proc/net/wireless", "r");
	if (!fp)
		return -1;
	char line[256];
	size_t iflen = strlen(ifname);
	int signal = -1;
	while (fgets(line, sizeof(line), fp)) {
		if (strncmp(line, ifname, iflen) == 0 && line[iflen] == ':') {
			/* Format: iface: status link level noise ... */
			int level;
			if (sscanf(line + iflen + 1, " %*d %*d. %d.", &level) == 1)
				signal = (level < 0) ? -level : level;
			break;
		}
	}
	fclose(fp);
	return signal;
}

/* Non-blocking TCP connect to 1.1.1.1:53 with ~200ms timeout */
static int check_internet(void)
{
	int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (sock < 0)
		return 0;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(53);
	inet_pton(AF_INET, "1.1.1.1", &addr.sin_addr);

	int r = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (r == 0) {
		close(sock);
		return 1;
	}
	if (errno != EINPROGRESS) {
		close(sock);
		return 0;
	}

	struct pollfd pfd = { .fd = sock, .events = POLLOUT };
	r = poll(&pfd, 1, 200);
	if (r > 0 && (pfd.revents & POLLOUT)) {
		int err = 0;
		socklen_t elen = sizeof(err);
		getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &elen);
		close(sock);
		return (err == 0) ? 1 : 0;
	}
	close(sock);
	return 0;
}

static int is_home_network(const char *ssid)
{
	for (size_t i = 0; i < N_HOME; i++)
		if (strcmp(ssid, home_networks[i]) == 0)
			return 1;
	return 0;
}

/* WiFi icons: no-internet variants */
static const char *wifi_noinet(int strength)
{
	if (strength >= 80) return "󰤩";
	if (strength <= 20) return "󰤫";
	if (strength <= 40) return "󰤠";
	if (strength <= 60) return "󰤣";
	return "󰤦";
}

/* WiFi icons: internet-ok variants */
static const char *wifi_ok(int strength)
{
	if (strength >= 80) return "󰤨";
	if (strength <= 20) return "󰤯";
	if (strength <= 40) return "󰤟";
	if (strength <= 60) return "󰤢";
	return "󰤥";
}

static void handle_click(const char *btn)
{
	switch (btn[0]) {
	case '1':
		system("st -e tmux new-session -A -s iwctl iwctl");
		break;
	case '3': {
		char ssid[IW_ESSID_MAX_SIZE + 1] = "";
		get_wifi_ssid(WLAN_IF, ssid, IW_ESSID_MAX_SIZE);

		char cmd[1024];
		FILE *fp;
		char ip_addr[64] = "N/A", pub_ip[64] = "N/A", gateway[64] = "N/A";

		fp = popen("ip -4 addr show " WLAN_IF " 2>/dev/null | awk '/inet / {split($2,a,\"/\"); print a[1]}'", "r");
		if (fp) { if (fgets(ip_addr, sizeof(ip_addr), fp)) ip_addr[strcspn(ip_addr, "\n")] = '\0'; pclose(fp); }
		fp = popen("curl -s ifconfig.me 2>/dev/null", "r");
		if (fp) { if (fgets(pub_ip, sizeof(pub_ip), fp)) pub_ip[strcspn(pub_ip, "\n")] = '\0'; pclose(fp); }
		fp = popen("ip route show default 2>/dev/null | awk '{print $3; exit}'", "r");
		if (fp) { if (fgets(gateway, sizeof(gateway), fp)) gateway[strcspn(gateway, "\n")] = '\0'; pclose(fp); }

		snprintf(cmd, sizeof(cmd),
			 "notify-send 'Network Details' 'Network: %s\\nInterface: %s\\nLocal IP: %s\\nPublic IP: %s\\nGateway: %s'",
			 ssid, WLAN_IF, ip_addr, pub_ip, gateway);
		system(cmd);
		break;
	}
	}
}

int main(void)
{
	const char *btn = getenv("BLOCK_BUTTON");
	if (btn && btn[0] != '\0') {
		handle_click(btn);
		if (btn[0] == '3')
			return 0;
	}

	/* Wired check */
	if (check_wired()) {
		if (!check_internet())
			puts(" 󰈂");
		else
			puts("");
		return 0;
	}

	/* Wireless state from sysfs */
	char wlan_state[32];
	if (read_sysfs("/sys/class/net/" WLAN_IF "/operstate", wlan_state, sizeof(wlan_state)) < 0
	    || strcmp(wlan_state, "up") != 0) {
		puts("󰤭");
		return 0;
	}

	/* SSID via ioctl (zero forks) */
	char ssid[IW_ESSID_MAX_SIZE + 1] = "";
	if (get_wifi_ssid(WLAN_IF, ssid, IW_ESSID_MAX_SIZE) < 0) {
		puts("󰤭");
		return 0;
	}

	int strength = get_wifi_signal(WLAN_IF);
	if (strength < 0)
		strength = 0;

	int inet = check_internet();

	if (!inet) {
		puts(wifi_noinet(strength));
	} else {
		if (is_home_network(ssid)) {
			puts("");
			return 0;
		}
		puts(wifi_ok(strength));
	}

	return 0;
}
