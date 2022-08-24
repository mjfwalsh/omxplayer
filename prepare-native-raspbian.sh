#!/bin/sh

check_dpkg_installed() {
	MISSING_PKGS=""
	for pkg in $@
	do
		echo -n "."
		if [ $(dpkg-query -W -f='${Status}' $pkg 2>/dev/null | grep -c "ok installed") -eq 0 ];
		then
			MISSING_PKGS="$MISSING_PKGS $pkg"
		fi
	done
}

#missing packages
MISSING_OMX_PKGS=""
MISSING_DEV_HEADERS=""
MISSING_COMPILE_FFMPEG_PKGS=""
MISSING_FFMPEG_PKGS=""

# packages
REQUIRED_OMX_PKGS="git perl gcc g++ pkg-config binutils libasound2-dev libpcre2-dev libboost-dev libfreetype6-dev libcairo2-dev libdvdread-dev libdbus-1-dev"
REQUIRED_DEV_HEADERS="libraspberrypi-dev libraspberrypi0 libraspberrypi-bin"
FFMPEG_PKGS="libavutil-dev libswresample-dev libavcodec-dev libavformat-dev"

echo -n "Running checks for native build on Raspberry PI OS"

echo =======================================================================================

echo "Checking amount of RAM in system\n"
#We require ~230MB of total RAM
TOTAL_RAM=`grep MemTotal /proc/meminfo | awk '{print $2}'`
TOTAL_SWAP=`grep SwapTotal /proc/meminfo | awk '{print $2}'`
if [ "$TOTAL_RAM" -lt 230000 ]; then
	echo "Your system has $TOTAL_RAM kB RAM available, which is too low. Checking swap space."
	if [ "$TOTAL_SWAP" -lt 230000 ]; then
		echo "Your system has $TOTAL_SWAP kB swap available, which is too low."
		echo "In order to compile ffmpeg you need to set memory_split to 16 for 256MB RAM PIs (0 does not work) or to at most 256 for 512MB RAM PIs, respectively."
		echo "Otherwise there is not enough RAM to compile ffmpeg. Please do the apropriate in the raspi-config and select finish to reboot your RPi."
		echo "Warning: to run compiled omxplayer please start raspi-config again and set memory_split to at least 128."
	else
		echo "You have enough swap space to compile, but speed will be lower and SD card wear will be increased."
		echo "In order to compile ffmpeg you need to set memory_split to 16 for 256MB RAM PIs (0 does not work) or to at most 256 for 512MB RAM PIs, respectively."
		echo "Otherwise there is not enough RAM to compile ffmpeg. Please do the apropriate in the raspi-config and select finish to reboot your RPi."
		echo "Warning: to run compiled omxplayer please start raspi-config again and set memory_split to at least 128."
	fi
else
	echo "Looks good"
fi
echo ""

echo =======================================================================================

# These can either be supplied by dpkg or via rpi-update.
# First, check dpkg to avoid messing with dpkg-managed files!
echo -n "Checking for OMX development headers"
check_dpkg_installed $REQUIRED_DEV_HEADERS
MAJOR_VERSION=`lsb_release -sc`
MISSING_DEV_HEADERS=$MISSING_PKGS
echo "done\n\n"

if [ "$MAJOR_VERSION" = "bullseye" ]; then
	echo "Bullseye no longer comes with the required firmware files to compile omxplayer."
	echo "However it is still possible to compile and run omxplayer if you get the required"
	echo "firmware files and make the appropriate changes to the display driver.\n"

	echo "You can install the required firmware files by using the following command:\n"
	echo "sudo rpi-update\n"

	echo "You will also need to enable the fake kms driver by replacing 'dtoverlay=vc4-kms-v3d'"
	echo "with 'dtoverlay=vc4-fkms-v3d' (not the f before kms) in your system's /boot/config.txt"
	echo "file."
elif [ ! -z "$MISSING_DEV_HEADERS" ]; then
	echo "You are missing the following OMX development headers:\n"

	echo $MISSING_DEV_HEADERS

	echo "\nYou can install these using apt-get"
else
	echo "Looks good"
fi
echo ""

echo =======================================================================================

echo -n "Checking for packages required to compile omxplayer"
check_dpkg_installed $REQUIRED_OMX_PKGS
MISSING_OMX_PKGS=$MISSING_PKGS
echo "done\n"

if [ ! -z "$MISSING_OMX_PKGS" ]; then
	echo "You are missing the following required packages:\n"
	echo $MISSING_OMX_PKGS
else
	echo "Looks good"
fi
echo ""

echo =======================================================================================

echo -n "Checking for ffmpeg development libraries"
check_dpkg_installed $FFMPEG_PKGS
MISSING_FFMPEG_PKGS=$MISSING_PKGS
echo "done\n"

if [ ! -z "$MISSING_FFMPEG_PKGS" ]; then
	echo "You are missing the following required packages:\n"
	echo $MISSING_FFMPEG_PKGS
else
	echo "Looks good. You may compile omxplayer without compiling ffmpeg."
fi
echo ""

echo =======================================================================================

if [ -z "$MISSING_OMX_PKGS" ] && [ -z "$MISSING_DEV_HEADERS" ] && [ -z "$MISSING_FFMPEG_PKGS" ]; then
	echo "Your kit looks good.\n"
	echo "You make proceed to compile and install omxplayer by running:\n"
	echo "make && sudo make install\n"
	exit 0
fi

echo "You need to install the required dependancies before proceeding.\n"
echo "Remember to run 'sudo apt-get update' first.\n"
