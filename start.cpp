#include <cstdio>
#include <cstdlib>
#include "portaudio_listener.h"
#include "six_channel.h"

// Global audio data
paTestData gData;

// ============================
// WAVETABLE INITIALIZATION
// ============================
static void initWavetable(paTestData& data)
{
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        float phase = i / (float)TABLE_SIZE;
        float amplitude;

        // Triangle wave
        if (phase > 0.5f)
            amplitude = (phase * -4.f) + 3.f;
        else
            amplitude = phase * 4.f - 1.f;

        data.sine[i] = amplitude;
    }
}

// ============================
// ROOM + SPEAKER POSITIONS
// ============================
static void initRoomAndSpeakers(paTestData& data)
{
    // Define room bounds
    data.subjectBounds[0] = { -1.0f, -1.0f }; // bottom-left
    data.subjectBounds[1] = {  1.0f,  1.0f }; // top-right

    if (CHANNEL_COUNT == 2)
    {
        data.speakerPositions[0] = { 1, 0 };
        data.speakerPositions[1] = { -1, 0 };
    }
    else if (CHANNEL_COUNT == 6)
    {
        data.speakerPositions[0] = { 1.000,  0.000 };
        data.speakerPositions[1] = { 0.309,  0.951 };
        data.speakerPositions[2] = { -0.809,  0.588 };
        data.speakerPositions[3] = { -0.809, -0.588 };
        data.speakerPositions[4] = { 0.309, -0.951 };
        data.speakerPositions[5] = { 0, 0 };
    }
    else
    {
        std::exit(EXIT_FAILURE);
    }

    // Listener begins at origin
    data.currentListenerPosition = { 0, 0 };
}

// ============================
// CHANNEL PHASES & VOLUMES
// ============================
static void initChannels(paTestData& data)
{
    for (int i = 0; i < CHANNEL_COUNT; i++)
    {
        data.channelPhases[i] = 0;
        data.channelVolumes[i] = 0;
    }
}

// ============================
// PUBLIC INITIALIZER
// Called by GUI BEFORE drawing
// ============================
void initAudioData()
{
    initWavetable(gData);
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