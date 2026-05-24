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

* **Graphical Subtitles**

* Suport for more recent version of ffmpeg (master branch, April 2026).

* Support for more recent Raspberry OS versions: Buster, Bullseye and Trixie
  - Please see detailed instructions regarding the **kms driver** and disabling the GUI.

## LIMITATIONS

OMXPlayer does not support software video decoding. HEVC is not supported, and DVD video
will only play on systems with mpeg2 hardware decoding. This can be purchased on rpi3 but
isn't available on the rpi4.

DVD menus are not supported.

## COMPILING

To compile OMXPlayer natively on you Raspberry PI you will need around 230 MBs of RAM. You will
also need the following packages:

### Development packages

Bullseye no longer comes with the required firmware files but it should still be possible to
get the required files by compiling and installing RaspberryPi's
[userland](https://github.com/raspberrypi/userland) repo (requires cmake).

To compile OMXPlayer you will also need the following packages:

    git libasound2-dev libpcre2-dev libboost-dev libcairo2-dev libdvdread-dev \
    libdbus-1-dev libavutil-dev libswresample-dev libavcodec-dev libavformat-dev

Once you have these installed you should be able to compile OMXPlayer with a `make`

## RUNNING

To run OMXPlayer need to disable the kms driver. You can do this by replacing it with the fake
kms driver or by disabling it completely. You can do this by changing the `dtoverlay` setting in
your system's `config.txt` file, located in `/boot/firmware` (if present) or `/boot` (if not).

    # kms is enabled so omxplayer can't run
    dtoverlay=vc4-kms-v3d

    # the fake kms driver is enabled (note the f before kms)
    dtoverlay=vc4-fkms-v3d

    # the kms driver is completely disabled (recommended)
    #dtoverlay=vc4-kms-v3d

If you are on **Bullseye** or **Trixie** you will also need to disable the graphical user
interface and boot into command line mode. **Buster** is the last version of RasberryPi
OS whose GUI does not need kms to run.

You will also need the following libraries:

    libbrcmEGL.so libbrcmGLESv2.so libopenmaxil.so libbcm_host.so libvchiq_arm.so libvcos.so

You will also need the following packages (included in the full version of Raspberry PI OS):

    libavcodec58 libavformat58 libavutil56 libswresample3 libcairo2

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
