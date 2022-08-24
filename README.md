# OMXPlayer

OMXPlayer is a command-line video player for the Raspberry Pi. It plays
video directly from the command line and plays outside your
[desktop environment](https://en.wikipedia.org/wiki/Desktop_environment). OMXPlayer uses the
[OpenMAX](https://en.wikipedia.org/wiki/OpenMAX) API to access the hardware video decoder in the
[GPU](https://en.wikipedia.org/wiki/Graphics_processing_unit). Hardware
acceleration along with command-line use allows ultra low overhead, low power video playback. It
was originally developed as a testbed for [Kodi](https://en.wikipedia.org/wiki/Kodi_(software))
on the Raspberry Pi.

This fork adds the following features:

* **Position remembering**: If you stop playing a file, OMXPlayer will remember where you
left off and begin playing from that position next time you play the file.

* **Auto-playlists**: OMXPlayer will automatically play the next file in the folder when the
previous file finished.

* **Recently played folder**: OMXPlayer creates a folder called OMXPlayerRecent off your home
directory with links to 20 most recently played files.

* **Experimental DVD support**: OMXPlayer can play iso/dmg DVD files as well DVD block devices.

* **CEC Input**: OMXPlayer should respond to commands from your TV's remote control.

## LIMITATIONS

OMXPlayer does not support software video decoding. HEVC is not supported, and DVD video
will only play on systems with mpeg2 hardware decoding. This can be purchased on rpi3 but
isn't available on the rpi4.

DVD menus are not supported.

OMXPlayer will not work on 64bit systems.

## DOWNLOAD

    git clone https://github.com/mjfwalsh/omxplayer.git

## COMPILING

To compile OMXPlayer natively on you Raspberry PI you will need around 230 MBs of RAM. You will
also need the following packages:

### Firmware packages

    libraspberrypi-dev libraspberrypi0 libraspberrypi-bin

(Bullseye no longer comes with the required firmware packages but it should still be possible to
get the required files by updating your firmware using `sudo rpi-update`.)

### Development packages

    git perl gcc g++ pkg-config binutils libc6-dev libstdc++-8-dev libasound2-dev
    libpcre2-dev libboost-dev libfreetype6-dev libcairo2-dev libdvdread-dev
    libdbus-1-dev libavutil-dev libswresample-dev libavcodec-dev libavformat-dev

Once you have these installed you should be able to compile OMXPlayer with a `make`

## RUNNING

To run OMXPlayer need to enable the fake kms driver by replacing 'dtoverlay=vc4-kms-v3d'
with 'dtoverlay=vc4-fkms-v3d' (note the f before kms) in your system's /boot/config.txt
file.

You will also need the following packages:

### General

    libc6 libdbus-1-3 libasound2 libfreetype6 libgcc1 libpcre3
    libstdc++6 zlib1g ca-certificates dbus libcairo2

(As far as I know all of these come pre-installed with Raspberry Pi OS)

### FFmpeg

    libavcodec58 libavformat58 libavutil56 libswresample3

(These come pre-installed on the full version of Raspberry Pi OS)

### DVDs

    libdvdread4 libdvdcss2

(These are optional and only loaded when trying to play a DVD. See
https://www.videolan.org/developers/libdvdcss.html on how to obtain libdvdcss2).

## COMMAND LINE OPTIONS AND DBUS

Please see the [manpage](omxplayer.pod) for command line options.

Please see [dbus.md](dbus.md) for details on OMXPlayer's dbus interface.
