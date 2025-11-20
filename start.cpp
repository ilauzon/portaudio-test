#include <cstdio>
#include <cstdlib>
#include "portaudio_listener.h"
#include "six_channel.h"


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

    if (CHANNEL_COUNT == 2) {
        data.speakerPositions[0] = {1, 0};
        data.speakerPositions[1] = {-1, 0};
    } else if (CHANNEL_COUNT == 6) {
        // 0 deg
        float step = 1 / 5.0f;
        data.speakerPositions[SixChannelSetup::Centre] = getCircularCoordinates(step * 0, 1);
        data.speakerPositions[SixChannelSetup::FrontRight] = getCircularCoordinates(step * 1, 1);
        data.speakerPositions[SixChannelSetup::BackRight] = getCircularCoordinates(step * 2, 1);
        data.speakerPositions[SixChannelSetup::BackLeft] = getCircularCoordinates(step * 3, 1);
        data.speakerPositions[SixChannelSetup::FrontLeft] = getCircularCoordinates(step * 4, 1);

        // ignored
        data.speakerPositions[SixChannelSetup::Subwoofer] = {0, 0};
    } else {
        exit(EXIT_FAILURE);
    }

    data.currentListenerPosition = {0, 0};

    for (int i = 0; i < CHANNEL_COUNT; i++) {
        data.channelPhases[i] = 0;
    }

    PaStream* stream = startPlayback(&data);
    endPlayback(stream);

    return EXIT_SUCCESS;
}