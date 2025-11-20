#include "portaudio_listener.h"
#include "six_channel.h"
#include <array>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// PortAudio is already included by portaudio_listener.h via "portaudio.h"

// ------------ Global selected output device ------------
// Set from the GUI via SetOutputDeviceIndex(...)
static int gOutputDeviceIndex = paNoDevice;

void SetOutputDeviceIndex(int index)
{
    gOutputDeviceIndex = index;
}

// ------------ Constants ------------
#define FRAMES_PER_BUFFER   (256)
#include <math.h>
#include <ostream>
#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>

#define FRAMES_PER_BUFFER (256)
#ifndef M_PI
#define M_PI (3.14159265)
#endif
#define REFERENCE_DISTANCE  (1.0f)

// ------------ Helpers ------------

static void checkErr(PaError err)
{
    if (err != paNoError)
    {
        std::printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        std::fflush(stdout);
        std::exit(EXIT_FAILURE);
    }
}

// distance from listener to each speaker
static std::array<float, CHANNEL_COUNT>
calculateSpeakerDistances(Point subjectPosition,
                          const Point speakerPositions[CHANNEL_COUNT])
{
    std::array<float, CHANNEL_COUNT> distances{};

    for (int i = 0; i < CHANNEL_COUNT; ++i)
    {
        Point currentSpeaker = speakerPositions[i];
        float xDiff = subjectPosition.x - currentSpeaker.x;
        float yDiff = subjectPosition.y - currentSpeaker.y;
        distances[i] = std::sqrt(xDiff * xDiff + yDiff * yDiff);
    }

    return distances;
}

// Get the point in 2D space that corresponds to a single-value position
// around the circle's circumference.
static Point getCircularCoordinates(float circularPosition, float radius)
{
    float angle = circularPosition * 2.0f * M_PI;
    Point p;
    p.x = radius * std::cos(angle);
    p.y = radius * std::sin(angle);
    return p;
}

std::string getSixChannelName(int channel) {
    switch(channel) {
        case SixChannelSetup::BackLeft:     return "BackLeft";
        case SixChannelSetup::BackRight:    return "BackRight";
        case SixChannelSetup::FrontLeft:    return "FrontLeft";
        case SixChannelSetup::FrontRight:   return "FrontRight";
        case SixChannelSetup::Centre:       return "Centre";
        case SixChannelSetup::Subwoofer:    return "Subwoofer";
        default: exit(EXIT_FAILURE);
    }
}

static int paTestCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    (void)inputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    paTestData *data = (paTestData *)userData;
    float *out = (float *)outputBuffer;

    // Compute distance to each speaker for this buffer
    std::array<float, CHANNEL_COUNT> distances =
        calculateSpeakerDistances(data->currentListenerPosition,
                                  data->speakerPositions);

    for (unsigned long i = 0; i < framesPerBuffer; ++i)
    {
        for (int c = 0; c < CHANNEL_COUNT; ++c)
        {
                float speakerDistance = distances[c];
    
            // Simple distance → gain mapping:
            // closer => louder, farther => quieter
            // gain in (0, 1], safe for visualization and audio
            float gain = REFERENCE_DISTANCE / (REFERENCE_DISTANCE + speakerDistance);

            // Store for GUI visualization
                data->channelVolumes[c] = gain;

            // Phase update
            int phaseOffset = data->channelPhases[c];
            data->channelPhases[c] += 1;
            if (data->channelPhases[c] >= TABLE_SIZE) {
                data->channelPhases[c] -= TABLE_SIZE;
            }

            float sample = data->sine[phaseOffset] * gain;

            *out++ = sample;
        }
    }
    return paContinue;
}

// ------------ Start / end playback ------------

PaStream* startPlayback(paTestData *data)
{
    PaError err = Pa_Initialize();
    checkErr(err);

    // Decide which output device to use:
    // - If GUI set gOutputDeviceIndex, use that.
    // - Otherwise, fall back to PortAudio default output device.
    int outputDevice = gOutputDeviceIndex;
    if (outputDevice == paNoDevice)
        outputDevice = Pa_GetDefaultOutputDevice();

    if (outputDevice == paNoDevice)
    {
        std::printf("No valid output device selected or available.\n");
        std::fflush(stdout);
        Pa_Terminate();
        return nullptr;
    }

    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(outputDevice);
    if (!deviceInfo)
    {
        std::printf("Failed to get device info for index %d.\n", outputDevice);
        std::fflush(stdout);
        Pa_Terminate();
        return nullptr;
    }

    if (deviceInfo->maxOutputChannels < CHANNEL_COUNT)
    {
        std::printf("Selected device '%s' does not support %d output channels "
                    "(maxOutputChannels = %d).\n",
                    deviceInfo->name,
                    CHANNEL_COUNT,
                    deviceInfo->maxOutputChannels);
        std::fflush(stdout);
        Pa_Terminate();
        return nullptr;
    }

    std::printf("Using output device %d: %s (maxOutputChannels=%d)\n",
                outputDevice,
                deviceInfo->name,
                deviceInfo->maxOutputChannels);
    std::fflush(stdout);

    PaStreamParameters outputParameters;
    std::memset(&outputParameters, 0, sizeof(outputParameters));

    outputParameters.device = outputDevice;
    outputParameters.channelCount = CHANNEL_COUNT;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency =
        deviceInfo->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    PaStream* stream = nullptr;
    err = Pa_OpenStream(&stream,
                        nullptr,                // no input
                        &outputParameters,      // output only
                        SAMPLE_RATE,
                        FRAMES_PER_BUFFER,
                        paNoFlag,
                        paTestCallback,
                        data);
    checkErr(err);

    err = Pa_StartStream(stream);
    checkErr(err);

    // Simple test movement of the listener over time
    const int stepsPerSec     = 100;
    const int testPeriodInSec = 10;
    const int totalSteps      = stepsPerSec * testPeriodInSec;
    const int sleepMs         = (testPeriodInSec * 1000) / totalSteps;

    if (CHANNEL_COUNT == 2)
    {
        // Sweep listener left ↔ right
        for (int i = 0; i < totalSteps; ++i)
        {
            float t = i / (float)totalSteps;   // 0..1
            float targetX = 1.0f - t * 2.0f;   // 1 → -1
            data->currentListenerPosition.x = targetX;
            data->currentListenerPosition.y = 0.0f;
            Pa_Sleep(sleepMs);
        }
    }
    else if (CHANNEL_COUNT == 6)
    {
        // Move listener around a circle
        for (int i = 0; i < totalSteps; ++i)
        {
            float t = i / (float)totalSteps;   // 0..1
            Point targetPosition = getCircularCoordinates(t, 1.0f);
            data->currentListenerPosition.x = targetPosition.x;
            data->currentListenerPosition.y = targetPosition.y;
            Pa_Sleep(sleepMs);
        }
    }

    return stream;
}

void endPlayback(PaStream* stream)
{
    if (!stream)
        return;

    PaError err = Pa_StopStream(stream);
    checkErr(err);

    err = Pa_CloseStream(stream);
    checkErr(err);

    err = Pa_Terminate();
    checkErr(err);
}