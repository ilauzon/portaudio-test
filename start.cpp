#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "six_channel.h"
#include "utils.h"
#include "portaudio_listener.h"

// Global audio data
paTestData gData;

// ============================
// ROOM + SPEAKER POSITIONS
// ============================


static void initRoomAndSpeakers(paTestData& data)
{
    // ... Keep room bounds and speaker position logic ...
    // ... Stop copying at "Open and read the audio file using libsndfile" ...

    data.subjectBounds[0] = { -3.0f, -3.0f };
    data.subjectBounds[1] = {  3.0f,  3.0f };
    
    // ... (Keep the speaker position setup code for CHANNEL_COUNT == 6) ...
    // COPY THE SPEAKER SETUP CODE FROM YOUR ORIGINAL FILE HERE

    data.currentListenerPosition = { 0.0, 0.0 };
    data.listenerYaw = 0.0;

    setMaxGain(&data);

    if (CHANNEL_COUNT == 2)
    {
        data.speakerPositions[0] = { 1, 0 };
        data.speakerPositions[1] = { -1, 0 };
    }
    else if (CHANNEL_COUNT == 6)
    {
        data.speakerPositions[Centre] = getCircularCoordinates(0 / 5.0 + 0.25, 2.0);
        data.speakerPositions[FrontRight] = getCircularCoordinates(-1 / 5.0 + 0.25, 2.0);
        data.speakerPositions[BackRight] = getCircularCoordinates(-2 / 5.0 + 0.25, 2.0);
        data.speakerPositions[BackLeft] = getCircularCoordinates(-3 / 5.0 + 0.25, 2.0);
        data.speakerPositions[FrontLeft] = getCircularCoordinates(-4 / 5.0 + 0.25, 2.0);
        data.speakerPositions[Subwoofer] = { 0, 0 };
    }
    else
    {
        std::exit(EXIT_FAILURE);
    }
}

// ============================
// CHANNEL PHASES & VOLUMES
// ============================
static void initChannels(paTestData& data)
{
    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        data.channelGains[i] = 1;
    }
}

// ============================
// PUBLIC INITIALIZER
// Called by GUI BEFORE drawing
// ============================
void initAudioData()
{
    initRoomAndSpeakers(gData);
    initChannels(gData);
}

// ============================
// START AUDIO PLAYBACK
// (Called by GUI thread)
// ============================
int start()
{
    // Ensure data is initialized
    initAudioData();

    PaStream* stream = startPlayback(&gData);
    if (!stream)
        return EXIT_FAILURE;

    endPlayback(stream);
    return EXIT_SUCCESS;
}