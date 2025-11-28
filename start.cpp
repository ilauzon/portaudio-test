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
    // Define room bounds
    data.subjectBounds[0] = { -3.0f, -3.0f }; // bottom-left
    data.subjectBounds[1] = {  3.0f,  3.0f }; // top-right

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

    // Listener begins at origin
    data.currentListenerPosition = { 0.0, 0.0 };
    data.listenerYaw = 0.0;

    // set max gain
    setMaxGain(&data);

    // Open and read the audio file using libsndfile
    auto audioFilePath = "assets/audio/flac_2_0.flac";
    SF_INFO sfinfo;
    SNDFILE* file = sf_open(audioFilePath, SFM_READ, &sfinfo);

    // Allocate output buffer for 5.1
    data.audio.resize(sfinfo.frames * 6);

    std::vector<float> tempBuffer(sfinfo.frames * 6, 0.0f);

    sf_count_t framesRead = sf_readf_float(file, tempBuffer.data(), sfinfo.frames);
    if (framesRead != sfinfo.frames) {
        std::cerr << "Warning: read fewer frames than expected\n";
    }

    // Mix stereo to 5.1
    if (sfinfo.channels == 2) {
        for (sf_count_t i = 0; i < framesRead; ++i) {
            float left  = tempBuffer[i * 2 + 0];
            float right = tempBuffer[i * 2 + 1];

            data.audio[i * 6 + 0] = left;              // Front Left
            data.audio[i * 6 + 1] = right;              // Front Right
            data.audio[i * 6 + 2] = (left * right) * 0.5f;             // Centre
            data.audio[i * 6 + 3] = (left + right) * 0.25f;       // Subwoofer
            data.audio[i * 6 + 4] = left * 0.5f;      // Rear Left
            data.audio[i * 6 + 5] = right * 0.5f; // Rear Right
        }
    } else if (sfinfo.channels == 6) {
        // Copy directly
        for (sf_count_t i = 0; i < framesRead; ++i) {
            for (int ch = 0; ch < 6; ++ch) {
                data.audio[i * 6 + ch] = tempBuffer[i * 6 + ch];
            }
        }
    } else {
        std::cerr << "Invalid number of channels: " << sfinfo.channels << "\n";
        exit(EXIT_FAILURE);
    }

    data.readIndex = 0;

    sf_close(file);
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