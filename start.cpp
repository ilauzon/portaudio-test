#include <cstdio>
#include <cstdlib>
#include "portaudio_listener.h"


int start() {
    paTestData data;

    /* initialise wavetable */
    for(int i = 0; i<TABLE_SIZE; i++)
    {
        // data.sine[i] = (float) sin( ((double)i/(double)TABLE_SIZE) * M_PI * 2. );
        // data.sine[i] = ((double)i/(double)TABLE_SIZE) * 2 - 1;

        float amplitude = 0;
        float phase = i / (float)TABLE_SIZE;

        // triangle wave
        if (phase > 0.5) {
            amplitude = (phase * -4) + 3;
        } else {
            amplitude = phase * 4 - 1;
        }

        data.sine[i] = amplitude; 
    }

    if (CHANNEL_COUNT != 2) exit(EXIT_FAILURE);
    data.speakerPositions[0] = {2, 0};
    data.speakerPositions[1] = {0, 0.25};

    data.currentListenerPosition = {0, 0};

    for (int i = 0; i < CHANNEL_COUNT; i++) {
        data.channelPhases[i] = 0;
    }

    startPlayback(&data);

    return EXIT_SUCCESS;
}