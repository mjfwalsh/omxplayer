omxplayer(1) -- Raspberry Pi command line OMX player
====================================================

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

## DOWNLOADING

    git clone https://github.com/mjfwalsh/omxplayer.git

## HELP AND DOCS

omxplayer's built-in help and the man page are all generated from this
README.md file during make. You may need to change the Makefile
if you modify the structure of README.md!

## COMPILING

Run this script which run checks for system requirements, and required packages.

    ./prepare-native-raspbian.sh

There are two ways of building omxplayer:

1. Using a specially compiled version of ffmpeg which can be built by running

	make ffmpeg

2. Or by using the same libraries as provided by apt-get. The prepare-native-raspbian.sh
script lists these as optional packages. They are:

	libavutil-dev libswresample-dev libavcodec-dev libavformat-dev libswscale-dev

Once you have either compiled ffmpeg or installed the optional packages, compile
omxplayer with

    make

and install with

    sudo make install

## SYNOPSIS

Usage: omxplayer [OPTIONS] [FILE]

    -h  --help                  Print this help
    -v  --version               Print version info
    -k  --keys                  Print key bindings
    -n  --aidx  index           Audio stream index, index can be language code or index number
    -o  --adev  device          Audio out device      : e.g. hdmi/local/both/alsa[:device]
    -i  --info                  Dump stream format and exit
    -I  --with-info             dump stream format before playback
    -s  --stats                 Pts and buffer stats
    -p  --passthrough           Audio passthrough
    -d  --deinterlace           Force deinterlacing
        --nodeinterlace         Force no deinterlacing
        --nativedeinterlace     let display handle interlace
        --anaglyph type         convert 3d to anaglyph
        --advanced[=0]          Enable/disable advanced deinterlace for HD videos (default enabled)
    -w  --hw                    Hw audio decoding
    -3  --3d mode               Switch tv into 3d mode (e.g. SBS/TB)
    -M  --allow-mvc             Allow decoding of both views of MVC stereo stream
    -y  --hdmiclocksync         Display refresh rate to match video (default)
    -z  --nohdmiclocksync       Do not adjust display refresh rate to match video
    -t  --sid index             Show subtitle with index, index can be language code or index number
    -r  --refresh               Adjust framerate/resolution to video
    -g  --genlog=logfile        Generate log file
    -l  --pos n                 Start position (hh:mm:ss)
        --track n               Play a DVD track (natural number, default 1)
    -b  --blank[=0xAARRGGBB]    Set the video background color to black (or optional ARGB value)
        --loop                  Loop file. Ignored if file not seekable
    -a  --no-playlist           Disable playlist and remember position funcctionality
        --no-boost-on-downmix   Don't boost volume when downmixing
        --vol n                 set initial volume in millibels (default 0)
        --amp n                 set initial amplification in millibels (default 0)
        --no-osd                Do not display status information on screen
        --no-keys               Disable keyboard input (prevents hangs for certain TTYs)
        --subtitles path        External subtitles in UTF-8 srt format
        --font-size size        Font size in 1/1000 screen height (default: 55)
        --align left/center     Subtitle alignment (default: left)
        --no-ghost-box          No semitransparent boxes behind subtitles
        --lines n               Number of lines in the subtitle buffer (default: 3)
        --aspect-mode type      Letterbox, fill, stretch (default: letterbox)
        --audio_fifo  n         Size of audio output fifo in seconds
        --video_fifo  n         Size of video output fifo in MB
        --audio_queue n         Size of audio input queue in MB
        --video_queue n         Size of video input queue in MB
        --threshold   n         Amount of buffered data required to finish buffering [s]
        --timeout     n         Timeout for stalled file/network operations (default 10s)
        --orientation n         Set orientation of video (0, 90, 180 or 270)
        --fps n                 Set fps of video where timestamps are not present
        --live                  Set for live tv or vod type stream
        --layout                Set output speaker layout (e.g. 5.1)
        --dbus_name name        default: org.mpris.MediaPlayer2.omxplayer
        --key-config <file>     Uses key bindings in <file> instead of the default
        --alpha                 Set video transparency (0..255)
        --layer n               Set video render layer number (higher numbers are on top)
        --display n             Set display to output to
        --cookie 'cookie'       Send specified cookie as part of HTTP requests
        --user-agent 'ua'       Send specified User-Agent as part of HTTP requests
        --lavfdopts 'opts'      Options passed to libavformat, e.g. 'probesize:250000,...'
        --avdict 'opts'         Options passed to demuxer, e.g., 'rtsp_transport:tcp,...'

For example:

    ./omxplayer -p -o hdmi test.mkv

## KEY BINDINGS

Key bindings to control omxplayer while playing:

    1           decrease speed
    2           increase speed
    <           rewind
    >           fast forward
    z           show info
    j           previous audio stream
    k           next audio stream
    i           previous chapter
    o           next chapter
    9           previous file/track
    0           next file/track
    n           previous subtitle stream
    m           next subtitle stream
    s           toggle subtitles
    w           show subtitles
    x           hide subtitles
    d           decrease subtitle delay (- 250 ms)
    f           increase subtitle delay (+ 250 ms)
    q           exit omxplayer
    p / space   pause/resume
    -           decrease volume
    + / =       increase volume
    left arrow  seek -30 seconds
    right arrow seek +30 seconds
    down arrow  seek -10 minutes
    up arrow    seek +10 minutes

## KEY CONFIG SYNTAX

A key config file is a series of rules of the form [action]:[key]. Multiple keys can be bound
to the same action, and comments are supported by adding a # in front of the line.
The list of valid [action]s roughly corresponds to the list of default key bindings above and are:

    DECREASE_SPEED
    INCREASE_SPEED
    REWIND
    FAST_FORWARD
    SHOW_INFO
    PREVIOUS_AUDIO
    NEXT_AUDIO
    PREVIOUS_CHAPTER
    NEXT_CHAPTER
    PREVIOUS_FILE
    NEXT_FILE
    PREVIOUS_SUBTITLE
    NEXT_SUBTITLE
    TOGGLE_SUBTITLE
    DECREASE_SUBTITLE_DELAY
    INCREASE_SUBTITLE_DELAY
    EXIT
    PAUSE
    DECREASE_VOLUME
    INCREASE_VOLUME
    SEEK_BACK_SMALL
    SEEK_FORWARD_SMALL
    SEEK_BACK_LARGE
    SEEK_FORWARD_LARGE
    STEP

Valid [key]s include all alpha-numeric characters and most symbols, as well as:

    left
    right
    up
    down
    esc
    space
    num [decimal or hex keycode]

For example:

    EXIT:esc
    PAUSE:p
    PAUSE:space
    REWIND:left
    SEEK_FORWARD_SMALL:num 0x4f43
    NEXT_FILE:num 63
    EXIT:q

## DBUS CONTROL

`omxplayer` can be controlled via DBUS.  There are three interfaces, all of
which present a different set of commands.  For examples on working with DBUS
take a look at the supplied [dbuscontrol.sh](dbuscontrol.sh) file.

### Root Interface

The root interface is accessible under the name
`org.mpris.MediaPlayer2`.

#### Methods

Root interface methods can be accessed through `org.mpris.MediaPlayer2.MethodName`.

##### Quit

Stops the currently playing video.  This will cause the currently running
omxplayer process to terminate.

   Params       |   Type
:-------------: | -------
 Return         | `null`

##### Raise

No effect.

   Params       |   Type
:-------------: | -------
 Return         | `null`

#### Properties

Root interface properties can be accessed through `org.freedesktop.DBus.Properties.Get`
and `org.freedesktop.DBus.Properties.Set` methods with the string
`"org.mpris.MediaPlayer2"` as first argument and the string `"PropertyName"` as
second argument.

##### CanQuit (ro)

Whether or not the player can quit.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`

##### Fullscreen (ro)

Whether or not the player can is fullscreen.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`

##### CanSetFullscreen (ro)

Whether or not the player can set fullscreen.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`

##### CanRaise (ro)

Whether the display window can be brought to the top of all the window.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`

##### HasTrackList (ro)

Whether or not the player has a track list.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`

##### Identity (ro)

Name of the player.

   Params       |   Type
:-------------: | --------
 Return         | `string`

##### SupportedUriSchemes (ro)

Playable URI formats.

   Params       |   Type
:-------------: | ----------
 Return         | `string[]`

##### SupportedMimeTypes (ro)

Supported mime types.  **Note**: currently not implemented.

   Params       |   Type
:-------------: | ----------
 Return         | `string[]`


### Player Interface

The player interface is accessible under the name
`org.mpris.MediaPlayer2.Player`.

#### Methods

Player interface methods can be accessed through `org.mpris.MediaPlayer2.Player.MethodName`.

##### Next

Skip to the next chapter.

   Params       |   Type
:-------------: | -------
 Return         | `null`

##### Previous

Skip to the previous chapter.

   Params       |   Type
:-------------: | -------
 Return         | `null`

##### Play

Play the video. If the video is playing, it has no effect, if it is
paused it will play from current position.

   Params       |   Type
:-------------: | -------
 Return         | `null`

##### Pause

Pause the video. If the video is playing, it will be paused, if it is
paused it will stay in pause (no effect).

   Params       |   Type
:-------------: | -------
 Return         | `null`

##### PlayPause

Toggles the play state.  If the video is playing, it will be paused, if it is
paused it will start playing.

   Params       |   Type
:-------------: | -------
 Return         | `null`

##### Stop

Stops the video. This has the same effect as Quit (terminates the omxplayer instance).

   Params       |   Type
:-------------: | -------
 Return         | `null`

##### Seek

Perform a *relative* seek, i.e. seek plus or minus a certain number of
microseconds from the current position in the video.

   Params       |   Type            | Description
:-------------: | ----------------- | ---------------------------
 1              | `int64`           | Microseconds to seek
 Return         | `null` or `int64` | If the supplied offset is invalid, `null` is returned, otherwise the offset (in microseconds) is returned

##### SetPosition

Seeks to a specific location in the file.  This is an *absolute* seek.

   Params       |   Type            | Description
:-------------: | ----------------- | ------------------------------------
 1              | `string`          | Path (not currently used)
 2              | `int64`           | Position to seek to, in microseconds
 Return         | `null` or `int64` | If the supplied position is invalid, `null` is returned, otherwise the position (in microseconds) is returned

##### SetAlpha

Set the alpha transparency of the player [0-255].

   Params       |   Type            | Description
:-------------: | ----------------- | ------------------------------------
 1              | `string`          | Path (not currently used)
 2              | `int64`           | Alpha value, 0-255

##### SetLayer

Seeks the video playback layer.

   Params       |   Type            | Description
:-------------: | ----------------- | ------------------------------------
 1              | `int64`           | Layer to switch to

##### Mute

Mute the audio stream.  If the volume is already muted, this does nothing.

   Params       |   Type
:-------------: | -------
 Return         | `null`

##### Unmute

Unmute the audio stream.  If the stream is already unmuted, this does nothing.

   Params       |   Type
:-------------: | -------
 Return         | `null`

##### ListSubtitles

Returns a array of all known subtitles.  The length of the array is the number
of subtitles.  Each item in the araay is a string in the following format:

    <index>:<language>:<name>:<codec>:<active>

Any of the fields may be blank, except for `index`.  `language` is the language
code, such as `eng`, `chi`, `swe`, etc.  `name` is a description of the
subtitle, such as `foreign parts` or `SDH`.  `codec` is the name of the codec
used by the subtitle, sudh as `subrip`.  `active` is either the string `active`
or an empty string.

   Params       |   Type
:-------------: | ----------
 Return         | `string[]` 

##### ListAudio

Returns and array of all known audio streams.  The length of the array is the
number of streams.  Each item in the array is a string in the following format:

    <index>:<language>:<name>:<codec>:<active>

See `ListSubtitles` for a description of what each of these fields means.  An
example of a possible string is:

    0:eng:DD 5.1:ac3:active

   Params       |   Type
:-------------: | ----------
 Return         | `string[]` 

##### ListVideo

Returns and array of all known video streams.  The length of the array is the
number of streams.  Each item in the array is a string in the following format:

    <index>:<language>:<name>:<codec>:<active>

See `ListSubtitles` for a description of what each of these fields means.  An
example of a possible string is:

    0:eng:x264:h264:active

   Params       |   Type
:-------------: | ----------
 Return         | `string[]` 

##### SelectSubtitle

Selects the subtitle at a given index.

   Params       |   Type    | Description
:-------------: | ----------| ------------------------------------
 1              | `int32`   | Index of subtitle to select
 Return         | `boolean` | Returns `true` if subtitle was selected, `false otherwise


##### SelectAudio

Selects the audio stream at a given index.

   Params       |   Type    | Description
:-------------: | ----------| ------------------------------------
 1              | `int32`   | Index of audio stream to select
 Return         | `boolean` | Returns `true` if stream was selected, `false otherwise

##### ShowSubtitles

Turns on subtitles.

   Params       |   Type 
:-------------: | -------
 Return         | `null`

##### HideSubtitles

Turns off subtitles.

   Params       |   Type 
:-------------: | -------
 Return         | `null`

##### GetSource

The current file or stream that is being played.

   Params       |   Type
:-------------: | ---------
 Return         | `string`


##### Action

Execute a "keyboard" command.  For available codes, see
[KeyConfig.h](KeyConfig.h).


   Params       |   Type    | Description
:-------------: | ----------| ------------------
 1              | `int32`   | Command to execute
 Return         | `null`    | 


#### Properties

Player interface properties can be accessed through `org.freedesktop.DBus.Properties.Get`
and `org.freedesktop.DBus.Properties.Set` methods with the string
`"org.mpris.MediaPlayer2"` as first argument and the string `"PropertyName"` as
second argument.

##### CanGoNext (ro)

Whether or not the play can skip to the next track.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`

##### CanGoPrevious (ro)

Whether or not the player can skip to the previous track.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`

##### CanSeek (ro)

Whether or not the player can seek.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`


##### CanControl (ro)

Whether or not the player can be controlled.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`

##### CanPlay (ro)

Whether or not the player can play.

Return type: `boolean`.

##### CanPause (ro)

Whether or not the player can pause.

   Params       |   Type
:-------------: | ---------
 Return         | `boolean`

##### PlaybackStatus (ro)

The current state of the player, either "Paused" or "Playing".

   Params       |   Type
:-------------: | ---------
 Return         | `string`

##### Volume (rw)

When called with an argument it will set the volume and return the current
volume.  When called without an argument it will simply return the current
volume.  As defined by the [MPRIS][MPRIS_volume] specifications, this value
should be greater than or equal to 0. 1 is the normal volume.
Everything below is quieter than normal, everything above is louder.

Millibels can be converted to/from acceptable values using the following:

    volume = pow(10, mB / 2000.0);
    mB     = 2000.0 * log10(volume)

   Params       |   Type    | Description
:-------------:	| --------- | ---------------------------
 1 (optional)   | `double`  | Volume to set
 Return         | `double`  | Current volume

[MPRIS_volume]: http://specifications.freedesktop.org/mpris-spec/latest/Player_Interface.html#Simple-Type:Volume

##### OpenUri (w)

Restart and open another URI for playing.

   Params       |   Type    | Description
:-------------: | --------- | --------------------------------
1               | `string`  | URI to play

##### Position (ro)

Returns the current position of the playing media.

   Params       |   Type    | Description
:-------------: | --------- | --------------------------------
 Return         | `int64`   | Current position in microseconds

##### MinimumRate (ro)

Returns the minimum playback rate of the video.

   Params       |   Type
:-------------: | -------
 Return         | `double`

##### MaximumRate (ro)

Returns the maximum playback rate of the video.

   Params       |   Type
:-------------: | -------
 Return         | `double`

##### Rate (rw)

When called with an argument it will set the playing rate and return the
current rate. When called without an argument it will simply return the
current rate. Rate of 1.0 is the normal playing rate. A value of 2.0
corresponds to two times faster than normal rate, a value of 0.5 corresponds
to two times slower than the normal rate.

   Params       |   Type    | Description
:-------------:	| --------- | ---------------------------
 1 (optional)   | `double`  | Rate to set
 Return         | `double`  | Current rate

##### Metadata (ro)

Returns track information: URI and length.

   Params       |   Type    | Description
:-------------: | --------- | ----------------------------
 Return         | `dict`    | Dictionnary entries with key:value pairs

##### Aspect (ro)

Returns the aspect ratio.

   Params       |   Type    | Description
:-------------: | --------- | ----------------------------
 Return         | `double`  | Aspect ratio

##### VideoStreamCount (ro)

Returns the number of video streams.

   Params       |   Type    | Description
:-------------: | --------- | ----------------------------
 Return         | `int64`   | Number of video streams

##### ResWidth (ro)

Returns video width

   Params       |   Type    | Description
:-------------: | --------- | ----------------------------
 Return         | `int64`   | Video width in px

##### ResHeight (ro)

Returns video width

   Params       |   Type    | Description
:-------------: | --------- | ----------------------------
 Return         | `int64`   | Video height in px

##### Duration (ro)

Returns the total length of the playing media.

   Params       |   Type    | Description
:-------------: | --------- | ----------------------------
 Return         | `int64`   | Total length in microseconds

