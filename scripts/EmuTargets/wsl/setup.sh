#!/bin/sh
# Builds one of CassoLink's emulator targets in this WSL instance, as root.
#
#   setup.sh kegs     <script folder> <IIgs ROM 03 file>
#   setup.sh gsplus   <script folder> <IIgs ROM 03 file>
#   setup.sh lancegs  <script folder> <MAME roms folder>
#   setup.sh uthernet <script folder>
#
# The script folder is scripts/EmuTargets/wsl as the instance sees it.
# KEGS and GSplus are built from source with two patches (kegs-patch.py), into
# /root/kegsrun and /root/gsplusrun. The LANceGS target is MAME 0.285 built
# from source with the Apple II drivers alone, in /opt/mame, with dnsmasq and
# wirepad.py in /root/netrun. The Uthernet target is AppleWin's Linux build in
# /opt/applewin.
set -e
export DEBIAN_FRONTEND=noninteractive
TARGET=$1
S=$2

case "$TARGET" in
kegs)
    apt-get install -y -q xvfb libx11-dev libxext-dev libpulse-dev unzip build-essential python3 curl > /root/kegs-deps.log 2>&1
    rm -rf /root/kegs-dl && mkdir -p /root/kegs-dl && cd /root/kegs-dl
    curl -sLO https://kegs.sourceforge.net/kegs.1.38.zip
    unzip -q kegs.1.38.zip
    cd kegs.1.38/src
    python3 "$S/kegs-patch.py" .
    rm -f vars && ln -s vars_x86linux vars
    make -j"$(nproc)" CC="gcc -std=gnu17" > /root/kegs-build.log 2>&1
    mkdir -p /root/kegsrun
    cp ../xkegs /root/kegsrun/xkegs
    cp "$3" /root/kegsrun/ROM.03
    ;;
gsplus)
    apt-get install -y -q git libsdl3-dev cmake ninja-build build-essential python3 > /root/gsplus-deps.log 2>&1
    rm -rf /root/gsplus
    git clone -q --depth 1 https://github.com/digarok/gsplus.git /root/gsplus
    cd /root/gsplus
    python3 "$S/kegs-patch.py" gsplus/src
    cmake -G Ninja -B build -S gsplus/src > /root/gsplus-cmake.log 2>&1
    cmake --build build --target gsplus-sdl > /root/gsplus-build.log 2>&1
    mkdir -p /root/gsplusrun
    cp build/gsplus /root/gsplusrun/gsplus
    cp "$3" /root/gsplusrun/ROM.03
    ;;
lancegs)
    apt-get install -y -q git build-essential python3 libsdl2-dev libsdl2-ttf-dev libfontconfig-dev \
        libpulse-dev libasound2-dev libxinerama-dev libxi-dev dnsmasq-base iproute2 iptables conntrack > /root/mame-deps.log 2>&1
    if [ ! -d /opt/mame ]; then
        git clone -q --depth 1 --branch mame0285 https://github.com/mamedev/mame.git /opt/mame
    fi
    cd /opt/mame
    make -j"$(nproc)" SOURCES=src/mame/apple/apple2e.cpp REGENIE=1 NO_USE_PORTAUDIO=1 NO_USE_PIPEWIRE=1 USE_QTDEBUG=0 > /root/mame-build.log 2>&1
    mkdir -p /root/mame/roms /root/.mame/cfg /root/netrun
    cp -r "$3/." /root/mame/roms/
    tr -d '\r' < "$S/wirepad.py" > /root/netrun/wirepad.py
    tr -d '\r' < "$S/apple2ee.cfg" > /root/.mame/cfg/apple2ee.cfg
    ;;
uthernet)
    apt-get install -y -q git cmake g++ xxd ninja-build pkg-config libncurses-dev libevdev-dev \
        libsdl2-image-dev libsdl2-dev libgles-dev libslirp-dev python3 > /root/applewin-deps.log 2>&1
    rm -rf /opt/applewin
    git clone -q --depth 1 --recurse-submodules --shallow-submodules https://github.com/audetto/AppleWin.git /opt/applewin
    python3 "$S/applewin-patch.py" /opt/applewin/source/frontends/sdl/sdlframe.cpp
    mkdir -p /opt/applewin/build && cd /opt/applewin/build
    cmake -G Ninja -DCMAKE_BUILD_TYPE=RELEASE -DBUILD_SA2=ON .. > /root/applewin-cmake.log 2>&1
    ninja > /root/applewin-build.log 2>&1
    ;;
*)
    echo "setup.sh: unknown target $TARGET" >&2
    exit 1
    ;;
esac

echo "setup.sh: $TARGET is built"
