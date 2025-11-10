# PortAudio Panning Test

This is a proof-of-concept for panning control in a stereo system with PortAudio.

## Setup
```sh
make install-deps
```

This install the portaudio libraries required to control your audio devices. In addition, it installs binaries and the source code to a number of portaudio tests, accessible via `./lib/portaudio/bin` and `./lib/portaudio/tests` respectively.

## Build and Run
```sh
make && 
OUTPUT_DEVICE=0 make run
```

This attempts to play a triangle wave that pans from left to right in your first available audio device.

If this does not work, you need to change the audio device it is trying to play from. The program starts with printing out a list of your available audio devices, change `OUTPUT_DEVICE` to the device number of the device you want audio to play from.
