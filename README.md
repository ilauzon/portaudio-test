# PortAudio Panning Test

This is a proof-of-concept for panning control in a stereo system with PortAudio.

## Setup
1. Install portaudio.
    ```sh
    make install-deps
    ```
    - This install the portaudio libraries required to control your audio devices. In addition, it installs binaries and the source code to a number of portaudio tests, accessible via `./lib/portaudio/bin` and `./lib/portaudio/tests` respectively.
2. Install [libsndfile](https://libsndfile.github.io/libsndfile/).
3. Install [WxWidgets](https://wxwidgets.org/).

## Build and Run
```sh
make && make run
```