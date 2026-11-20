#!/bin/sh
# Starts one of CassoLink's emulator targets in this WSL instance, headless,
# as root, after setup.sh has built it. Disks are paths the instance sees.
#
#   run.sh kegs     <drive 1> <drive 2> <Windows host address> <TCP port>
#   run.sh gsplus   <drive 1> <drive 2> <Windows host address> <TCP port>
#   run.sh lancegs  <drive 1> <drive 2>
#   run.sh uthernet <drive 1> <drive 2>
#   run.sh harddisk <drive 1> <hard disk 1> <hard disk 2>
#   run.sh adtpro   <drive 1> <hard disk 1> <hard disk 2>
#   run.sh stop
#
# KEGS and GSplus run a IIgs at 2.8 MHz with the modem port connected to the
# host's port; unthrottled, the shim's waits, which are counted in
# instructions, run out. KEGS draws to Xvfb and GSplus to SDL's offscreen
# driver.
#
# The LANceGS target is MAME's enhanced //e with the card in slot 3 on a TAP
# device. wirepad.py relays its frames to tap-wire, which has the address
# 10.0.3.1, padding frames to the Apple as a real wire does. dnsmasq gives the
# Apple 10.0.3.2, and UDP 6502 on eth0 is forwarded to it.
#
# The Uthernet target is AppleWin's enhanced //e with the card in slot 3 over
# libslirp, with UDP 6502 on the instance forwarded to the Apple.
#
# The hard disk target is the Uthernet target with the hard disk card in slot
# 7 and both its drives. The Apple boots slot 7 first, so the first hard disk
# has to boot the Disk II in slot 6.
#
# The ADTPro target is the hard disk target booting ADTPro's Ethernet client,
# with ADTPro's server running here, for a comparison with CassoLink. The
# Apple reaches the server at 10.0.0.1, libslirp's address for this instance.
# AppleWin's window is an X11 window, so xdotool can type into it.
#
# Each runs for an hour at most.
#
# With CASSO_VISIBLE set, AppleWin's window is shown through WSLg as an X11
# window, which xdotool can type into; otherwise it runs offscreen.
TARGET=$1
SA2_VIDEO=offscreen
[ -n "$CASSO_VISIBLE" ] && SA2_VIDEO=x11

# GSplus doesn't end on SIGTERM while it runs offscreen, so whatever is left
# after a second is killed outright. The LANceGS's forward of UDP 6502 is
# removed with its connection tracking: a connection that prompts each second
# keeps a tracked flow alive, and would go on to 10.0.3.2 under the next
# target.
stop_all() {
    for signal in TERM KILL; do
        pkill -"$signal" -x xkegs
        pkill -"$signal" -x gsplus
        pkill -"$signal" -x Xvfb
        pkill -"$signal" -x mame
        pkill -"$signal" -x sa2
        pkill -"$signal" -f wirepad.py
        pkill -"$signal" -x dnsmasq
        pkill -"$signal" -f org.adtpro.ADTPro
        sleep 1
    done

    iptables -t nat -F PREROUTING 2> /dev/null
    iptables -t nat -F POSTROUTING 2> /dev/null
    conntrack -D -p udp --dport 6502 > /dev/null 2>&1
}

write_kegs_config() {
    cat > "$1/config.kegs" <<EOF
# KEGS configuration file version 1.07

s6d1 = $2
s6d2 = $3

g_cfg_rom_path = $1/ROM.03
g_limit_speed = 2
g_serial_cfg[1] = 2
g_serial_remote_ip[1] = $4
g_serial_remote_port[1] = $5
g_config_kegs_auto_update = 0
EOF
}

case "$TARGET" in
stop)
    stop_all
    ;;
kegs)
    stop_all
    write_kegs_config /root/kegsrun "$2" "$3" "$4" "$5"
    Xvfb :99 -screen 0 1600x1200x24 > /dev/null 2>&1 &
    sleep 1
    cd /root/kegsrun
    DISPLAY=:99 exec timeout 3600 ./xkegs -cfg /root/kegsrun/config.kegs > /root/kegsrun/kegs.log 2>&1
    ;;
gsplus)
    stop_all
    write_kegs_config /root/gsplusrun "$2" "$3" "$4" "$5"
    cd /root/gsplusrun
    SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy exec timeout 3600 ./gsplus -audio 0 -cfg /root/gsplusrun/config.kegs > /root/gsplusrun/gsplus.log 2>&1
    ;;
lancegs)
    stop_all
    ip link show tap-mess-0-0 > /dev/null 2>&1 || ip tuntap add dev tap-mess-0-0 mode tap
    ip addr flush dev tap-mess-0-0
    ip link set tap-mess-0-0 up

    : > /root/netrun/wirepad.log
    python3 /root/netrun/wirepad.py &
    sleep 1
    ip addr flush dev tap-wire
    ip addr add 10.0.3.1/24 dev tap-wire
    ip link set tap-wire up

    dnsmasq --interface=tap-wire --bind-interfaces --except-interface=lo \
        --dhcp-range=10.0.3.2,10.0.3.2,12h --dhcp-option=3,10.0.3.1 \
        --log-dhcp --log-facility=/root/netrun/dnsmasq.log --port=0

    sysctl -q -w net.ipv4.ip_forward=1
    iptables -t nat -F PREROUTING
    iptables -t nat -A PREROUTING -i eth0 -p udp --dport 6502 -j DNAT --to-destination 10.0.3.2:6502
    iptables -t nat -F POSTROUTING
    iptables -t nat -A POSTROUTING -o eth0 -s 10.0.3.0/24 -j MASQUERADE

    unset DISPLAY WAYLAND_DISPLAY
    SDL_VIDEODRIVER=dummy exec timeout 3600 /opt/mame/mame apple2ee -cfg_directory /root/.mame/cfg -rompath /root/mame/roms \
        -sl3 lancegs -flop1 "$2" -flop2 "$3" -networkprovider taptun \
        -video none -sound none -skip_gameinfo -seconds_to_run 3600 \
        -keyboardprovider none -mouseprovider none -joystickprovider none > /root/netrun/mame.log 2>&1
    ;;
uthernet)
    stop_all
    exec env ${SA2_VIDEO:+SDL_VIDEODRIVER=$SA2_VIDEO} timeout 3600 /opt/applewin/build/sa2 --no-audio --no-imgui \
        -r 'Configuration\Slot 3.Card type=15' \
        --nat '3,udp,,6502,,6502' \
        --d1 "$2" --d2 "$3" > /root/applewin.log 2>&1
    ;;
harddisk)
    stop_all
    exec env ${SA2_VIDEO:+SDL_VIDEODRIVER=$SA2_VIDEO} timeout 3600 /opt/applewin/build/sa2 --no-audio --no-imgui \
        -r 'Configuration\Slot 3.Card type=15' \
        -r 'Configuration\Slot 7.Card type=5' \
        --nat '3,udp,,6502,,6502' \
        --d1 "$2" --h1 "$3" --h2 "$4" > /root/applewin.log 2>&1
    ;;
adtpro)
    stop_all
    (cd /root/adtpro && ./adtpro.sh headless ethernet > /root/adtpro/server.log 2>&1 &)
    SDL_VIDEODRIVER=x11 exec timeout 3600 /opt/applewin/build/sa2 --no-audio --no-imgui \
        -r 'Configuration\Slot 3.Card type=15' \
        -r 'Configuration\Slot 7.Card type=5' \
        --d1 "$2" --h1 "$3" --h2 "$4" > /root/applewin.log 2>&1
    ;;
*)
    echo "run.sh: unknown target $TARGET" >&2
    exit 1
    ;;
esac
