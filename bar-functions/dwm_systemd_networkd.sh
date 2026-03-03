#!/bin/sh

case "$BLOCK_BUTTON" in
	1) st -e tmux new-session -A -s iwctl iwctl ;;
	3)
		interface=$(ip addr | awk '/state UP/ {gsub(":","");print $2}' | grep -v 'virbr0' | head -n 1)
		ip_addr=$(ip -4 addr show "$interface" 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1)
		pub_ip=$(curl -s ifconfig.me 2>/dev/null)
		gateway=$(ip route | awk '/default/ {print $3}' | head -n 1)
		network=$(iwctl station "$interface" show | grep -Po 'Connected network \K.*' | awk '{$1=$1;print}')
		notify-send "Network Details" "Network: $network\nInterface: $interface\nLocal IP: $ip_addr\nPublic IP: $pub_ip\nGateway: $gateway"
		;;
esac

interface=$(ip addr | awk '/state UP/ {gsub(":","");print $2}' | grep -v 'virbr0' | head -n 1)
status=$(iwctl station $interface show | grep State | grep -o 'connected\|disconnected')
wired=$(ip -s link show | grep '^[0-9]: enp' | grep -c 'state UP')

no_con=""
if ! ping -c 1 -W 2 1.1.1.1 >/dev/null 2>&1; then
	no_con=" 󰈂"
fi

if [ "$wired" = 1 ]; then
    echo "$no_con"
elif [ "$status" = "connected" ]; then
		network=$(iwctl station wlan0 show | grep -Po 'Connected network \K.*' | awk '{$1=$1;print}')
		strength=$(iw dev "$interface" link | awk '/signal/ {gsub("-",""); print $2}')
		if [ -n "$no_con" ]; then
			if [ "$strength" -ge 80 ]; then
				echo "󰤩"
			elif [ "$strength" -le 20 ]; then
				echo "󰤫"
			elif [ "$strength" -le 40 ]; then
				echo "󰤠"
			elif [ "$strength" -le 60 ]; then
				echo "󰤣"
			elif [ "$strength" -le 80 ]; then
				echo "󰤦"
			else
				echo "󰤠"
			fi
		else
			if [ "$network" = "Davi" ] || [ "$network" = "CasaRio_5G" ] || [ "$network" = "Que Wifi?" ]; then
				echo "$no_con"
				exit 0
			fi
			if [ "$strength" -ge 80 ]; then
				echo "󰤨"
			elif [ "$strength" -le 20 ]; then
				echo "󰤯"
			elif [ "$strength" -le 40 ]; then
				echo "󰤟"
			elif [ "$strength" -le 60 ]; then
				echo "󰤢"
			elif [ "$strength" -le 80 ]; then
				echo "󰤥"
			else
				echo "󰤠"
			fi
		fi
else
	echo "󰤭"
fi
