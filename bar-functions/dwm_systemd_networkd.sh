#!/bin/sh

# dwm_systemd_networkd - network status for dwmblocks
# GNU GPLv3

# Dependencies: iw, iwd (for iwctl click handler), ping

WLAN="wlan0"

case "$BLOCK_BUTTON" in
	1) st -e tmux new-session -A -s iwctl iwctl ;;
	3)
		IW_OUT=$(iw dev "$WLAN" link 2>/dev/null)
		network=""
		while IFS= read -r line; do
			case "$line" in *SSID:*) network="${line#*SSID: }"; break ;; esac
		done <<EOF
$IW_OUT
EOF
		ip_addr=$(ip -4 addr show "$WLAN" 2>/dev/null | awk '/inet / {split($2,a,"/"); print a[1]}')
		pub_ip=$(curl -s ifconfig.me 2>/dev/null)
		gateway=$(ip route show default 2>/dev/null | awk '{print $3; exit}')
		notify-send "Network Details" "Network: $network\nInterface: $WLAN\nLocal IP: $ip_addr\nPublic IP: $pub_ip\nGateway: $gateway"
		;;
esac

# Wired check via sysfs (zero forks)
for iface in /sys/class/net/enp*; do
	[ -e "$iface" ] || continue
	read -r state < "$iface/operstate"
	if [ "$state" = "up" ]; then
		if ! ping -c 1 -W 2 1.1.1.1 >/dev/null 2>&1; then
			echo " 󰈂"
		else
			echo ""
		fi
		exit 0
	fi
done

# Wireless state via sysfs (zero forks)
read -r wlan_state < "/sys/class/net/$WLAN/operstate"
if [ "$wlan_state" != "up" ]; then
	echo "󰤭"
	exit 0
fi

# Single iw call for SSID + connection check (1 fork)
IW_OUT=$(iw dev "$WLAN" link 2>/dev/null)
case "$IW_OUT" in
	"Not connected."*)
		echo "󰤭"
		exit 0
		;;
esac

# Parse SSID from iw output with shell builtins (zero forks)
network=""
while IFS= read -r line; do
	case "$line" in *SSID:*) network="${line#*SSID: }"; break ;; esac
done <<EOF
$IW_OUT
EOF

# Signal: parse dBm from already-fetched iw output (works on nl80211-only drivers)
strength=""
while IFS= read -r line; do
	case "$line" in
		*"signal: "*)
			sig="${line#*signal: }"
			sig="${sig% dBm}"
			sig="${sig%.*}"
			strength=$(( (sig + 100) * 2 ))
			[ "$strength" -gt 100 ] && strength=100
			[ "$strength" -lt 0 ] && strength=0
			break
			;;
	esac
done <<EOF
$IW_OUT
EOF

# Fallback: /proc/net/wireless (legacy WE drivers)
if [ -z "$strength" ]; then
	while IFS=' ' read -r iface _ _ level _; do
		case "$iface" in
			"$WLAN":*) strength="${level%%.*}"; strength="${strength#-}"; break ;;
		esac
	done < /proc/net/wireless
fi

[ -n "$strength" ] || strength=50

# Internet connectivity check (1 fork)
if ! ping -c 1 -W 2 1.1.1.1 >/dev/null 2>&1; then
	if [ "$strength" -ge 80 ]; then
		echo "󰤩"
	elif [ "$strength" -le 20 ]; then
		echo "󰤫"
	elif [ "$strength" -le 40 ]; then
		echo "󰤠"
	elif [ "$strength" -le 60 ]; then
		echo "󰤣"
	else
		echo "󰤦"
	fi
else
	# Home networks — hide icon
	case " Davi CasaRio_5G Que Wifi? " in
		*" $network "*)
			echo ""
			exit 0
			;;
	esac
	if [ "$strength" -ge 80 ]; then
		echo "󰤨"
	elif [ "$strength" -le 20 ]; then
		echo "󰤯"
	elif [ "$strength" -le 40 ]; then
		echo "󰤟"
	elif [ "$strength" -le 60 ]; then
		echo "󰤢"
	else
		echo "󰤥"
	fi
fi
