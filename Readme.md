# Basic FFMpeg-driven video playback with SFML

On Linux it will use the system installation of FFMpeg. Windows builds will need to install it via
vcpkg or something else. Readily available binary distributions of FFMpeg for windows seem to lack
all the libs, just shipping the basic binaries.

I'm strictly an amateur and this example is not high quality or production grade. This is the result
of a few days hacking and needs plenty of love before ideally being released. I only make this available
to hopefully save others some time and give them a base to work from.

Because the audio and video decoding is happening on main thread, if the game loop is too slow the audio
buffer will not be filled quickly enough. At extremely low loop speed presumably the video buffer would
suffer the same problem. I'm guessing having a thread dedicated to decoding and filling the buffers this
wouldn't be an issue but I didn't get that far.

## How To

Build with CMake, copy the provided mp4 sample next to the binary and run.

## Features

* Handles any codec combination that FFMpeg is compiled for.
* Uses SFML for rendering and audio output
* Tries to keep a similar API as SFML
* PTS handling for audio/video sync
* Will run at its own speed regardless of game loop (as long as its fast enough)

## Missing

* Seeking
* Multiple audio streams
* Subtitles
* Full sprite support (some of the basics are forwarded to the sprite object but not all)