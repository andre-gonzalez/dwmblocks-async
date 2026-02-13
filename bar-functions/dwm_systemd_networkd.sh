#!/bin/sh

# Handle click actions
case "$BLOCK_BUTTON" in
	1) st -e tmux new-session -A -s iwctl iwctl & exit 0 ;;
	3)
		interface=$(ip addr | awk '/state UP/ {gsub(":","");print $2}' | grep -v 'virbr0' | head -n 1)
		ip_addr=$(ip -4 addr show "$interface" 2>/dev/null | awk '/inet / {print $2}' | cut -d/ -f1)
		pub_ip=$(curl -s ifconfig.me 2>/dev/null)
		gateway=$(ip route | awk '/default/ {print $3}' | head -n 1)
		notify-send "Network Details" "Interface: $interface\nLocal IP: $ip_addr\nPublic IP: $pub_ip\nGateway: $gateway"
		exit 0
		;;
esac

interface=$(ip addr | awk '/state UP/ {gsub(":","");print $2}' | grep -v 'virbr0' | head -n 1)
status=$(iwctl station $interface show | grep State | grep -o 'connected\|disconnected')
wired=$(ip -s link show | grep '^[0-9]: enp' | grep -c 'state UP')

if [ "$wired" = 1 ]; then
    echo ""
elif [ "$status" = "connected" ]; then
		network=$(iwctl station wlan0 show | grep -Po 'Connected network \K.*' | awk '{$1=$1;print}')
		if [ "$network" = "Davi" ]; then
			echo ""
			exit 0
		fi
		strength=$(iw dev "$interface" link | awk '/signal/ {gsub("-",""); print $2}')
		if [ "$strength" -ge 80 ]; then
			echo "󰤨"" $network"
		elif [ "$strength" -le 20 ]; then
			echo "󰤯"" $network"
		elif [ "$strength" -le 40 ]; then
			echo "󰤟"" $network"
		elif [ "$strength" -le 60 ]; then
			echo "󰤢"" $network"
		elif [ "$strength" -le 80 ]; then
			echo "󰤥"" $network"
		else
			echo "󰤠"" $network"
		fi
else
	echo "󰤭"
fi
