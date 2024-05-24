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
 Return         | `boolean` | Returns `true` if subtitle was selected, `false` otherwise


##### SelectAudio

Selects the audio stream at a given index.

   Params       |   Type    | Description
:-------------: | ----------| ------------------------------------
 1              | `int32`   | Index of audio stream to select
 Return         | `boolean` | Returns `true` if stream was selected, `false` otherwise

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

