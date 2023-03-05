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

## COMPILING

To compile OMXPlayer natively on you Raspberry PI you will need around 230 MBs of RAM. You will
also need the following packages:

### Development packages

Bullseye no longer comes with the required firmware files but it should still be possible to
get the required files by updating your firmware using `sudo rpi-update`.

To compile omxplayer you will also need the following packages<sup>[*](#required-packages)</sup>:

    git libasound2-dev libpcre2-dev libboost-dev libcairo2-dev libdvdread-dev
    libdbus-1-dev libavutil-dev libswresample-dev libavcodec-dev libavformat-dev

Once you have these installed you should be able to compile OMXPlayer with a `make`

## RUNNING

To run OMXPlayer need to disable the kms driver. You can do this by replacing it with the fake
kms driver or by disabling it completely. You can do this by changing the `dtoverlay` setting in
your system's `/boot/config.txt` file.

    # kms is enabled so omxplayer can't run
    dtoverlay=vc4-kms-v3d

    # the fake kms driver is enabled (note the f before kms)
    dtoverlay=vc4-fkms-v3d

    # the kms driver is completely disabled
    #dtoverlay=vc4-kms-v3d

You will also need the following static libraries:

    libbrcmEGL.so libbrcmGLESv2.so libopenmaxil.so

which should be in the `/opt/vc/lib` directory.

You will also need the following packages<sup>[*](#required-packages)</sup>:

    libavcodec58 libavformat58 libavutil56 libswresample3 libcairo2

(These packages are included in the full version of Raspberry PI OS.)

### DVDs

Playing DVDs is experimental and not guaranteed to work. DVD menus are not supported.

To play DVDs you will need to purchase a [MPEG-2 licence](https://codecs.raspberrypi.com/mpeg-2-license-key/)
(only available for the Raspberry PI 3).

While `libdvdread8` comes installed with most version of Raspberry PI OS, to play most DVDs you
will need `libdvdcss2`. See [Videolan's install instructions](https://www.videolan.org/developers/libdvdcss.html)
on how to get it.

## COMMAND LINE OPTIONS AND DBUS

Please see the [manpage](omxplayer.pod) for command line options.

Please see [dbus.md](dbus.md) for details on OMXPlayer's dbus interface.

## Required packages

The above package lists assume you have all the packages that come preinstalled with
Raspberry PI OS (lite) still installed.

If you don't you will also need:

### To compile

    libraspberrypi-dev libraspberrypi0 libraspberrypi-bin gcc g++ libstdc++-10-dev pkg-config
    binutils libc6-dev libfreetype6-dev  perl

### To run

    libc6 libdbus-1-3 libasound2 libfreetype6 libgcc1 libpcre3
    libstdc++6 zlib1g ca-certificates dbus libdvdread8
