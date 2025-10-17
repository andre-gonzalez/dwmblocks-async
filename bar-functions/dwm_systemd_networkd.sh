#!/bin/sh

interface=$(ip addr | awk '/state UP/ {gsub(":","");print $2}' | grep -v 'virbr0' | head -n 1)
status=$(iwctl station $interface show | grep State | grep -o 'connected\|disconnected')
wired=$(ip -s link show enp2s0 | grep -c 'state UP')
wired_enp0s20f0u1c2=$(ip -s link show enp0s20f0u1c2 | grep -c 'state UP')

if [ "$wired_enp2s0" = 1 ] || [ "$wired_enp0s20f0u1c2" = 1 ]; then
    # echo "󰈁"
    echo ""
elif [ "$status" = "connected" ]; then
		network=$(iwctl station wlan0 show | grep -Po 'Connected network \K.*' | awk '{$1=$1;print}')
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
