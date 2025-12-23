#!/bin/sh

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
