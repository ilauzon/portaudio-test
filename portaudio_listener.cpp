#include "portaudio_listener.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <math.h>
#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>

#define FRAMES_PER_BUFFER (256)
#ifndef M_PI
#define M_PI (3.14159265)
#endif
#define OUTPUT_DEVICE (6)
#define REFERENCE_DISTANCE (1)

static void checkErr(PaError err) {
    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }
}

static inline float max(float a, float b) { return a > b ? a : b; }

std::array<float, CHANNEL_COUNT>
calculateSpeakerDistances(Point subjectPosition,
                          Point speakerPositions[CHANNEL_COUNT]) {
    std::array<float, CHANNEL_COUNT> distances{};

    for (int i = 0; i < CHANNEL_COUNT; i++) {
        Point currentSpeaker = speakerPositions[i];
        float xDiff = subjectPosition.x - currentSpeaker.x;
        float yDiff = subjectPosition.y - currentSpeaker.y;
        distances[i] = std::sqrt(xDiff * xDiff + yDiff * yDiff);
    }

    return distances;
}

// Get the point in 2D space that corresponds to a single-value position around the circle's circumference.
Point getCircularCoordinates(float circularPosition, float radius) {
    float angle = circularPosition * 2.0f * M_PI;
    Point p;
    p.x = radius * cosf(angle);
    p.y = radius * sinf(angle);
    return p;
}

static int paTestCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags, void *userData) {
    paTestData *data = (paTestData *)userData;
    float *out = (float *)outputBuffer;
    (void)inputBuffer;

    std::array<float, CHANNEL_COUNT> distances = calculateSpeakerDistances(
        data->currentListenerPosition, data->speakerPositions);

    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        for (int c = 0; c < CHANNEL_COUNT; c++) {
        float speakerDistance = distances[c];
        float gainCompensation = speakerDistance / REFERENCE_DISTANCE;
        data->channelVolumes[c] = gainCompensation;

        // Change phase.
        int phaseOffset = data->channelPhases[c];
        data->channelPhases[c] += 1;
        if (data->channelPhases[c] >= TABLE_SIZE)
            data->channelPhases[c] -= TABLE_SIZE;

        // Change volume.
        float amplitude = data->sine[phaseOffset] * gainCompensation;
        *out++ = amplitude;
        }
    }

    return paContinue;
}

PaStream* startPlayback(paTestData *data) {
    PaError err;
    err = Pa_Initialize();
    checkErr(err);

    int numDevices = Pa_GetDeviceCount();
    printf("Number of devices: %d\n", numDevices);

    if (numDevices < 0) {
        printf("Error getting device count: PaError %d\n", numDevices);
        exit(EXIT_FAILURE);
    } else if (numDevices == 0) {
        printf("No available audio devices found.\n");
        exit(EXIT_SUCCESS);
    }

    const PaDeviceInfo *deviceInfo;
    for (int i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        printf("Device %d:\n", i);
        printf("\tname: %s\n", deviceInfo->name);
        printf("\tmaxInputChannels: %d\n", deviceInfo->maxInputChannels);
        printf("\tmaxOutputChannels: %d\n", deviceInfo->maxOutputChannels);
        printf("\tsdefaultSampleRate: %f\n", deviceInfo->defaultSampleRate);
    }

    // prompt user for output device
    int outputDevice;
    std::cout << "Enter your output device: ";
    std::cin >> outputDevice; // Program waits here for integer input

    PaStreamParameters outputParameters;

    memset(&outputParameters, 0, sizeof(outputParameters));
    outputParameters.channelCount = CHANNEL_COUNT;
    outputParameters.device = outputDevice;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency =
        Pa_GetDeviceInfo(outputDevice)->defaultLowOutputLatency;

    PaStream* stream;
    err = Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE,
                        FRAMES_PER_BUFFER, paNoFlag, paTestCallback, data);
    checkErr(err);

    err = Pa_StartStream(stream);
    checkErr(err);

    int stepsPerSec = 100;
    int testPeriodInSec = 10;
    int totalSteps = stepsPerSec * testPeriodInSec;

    if (CHANNEL_COUNT == 2) {
        for (int i = 0; i < totalSteps; i++) {
            float targetBalance = i / (float)totalSteps;
            float targetX = 1 - targetBalance * 2;
            printf("%f\n", targetX);
            data->currentListenerPosition.x = targetX;
            Pa_Sleep((testPeriodInSec * 1000) / totalSteps);
        }
    } else if (CHANNEL_COUNT == 6) {
        for (int i = 0; i < totalSteps; i++) {
            float targetBalance = i / (float)totalSteps;
            Point targetPosition = getCircularCoordinates(targetBalance, 1);
            printf("%f, %f\n", targetPosition.x, targetPosition.y);
            data->currentListenerPosition.x = targetPosition.x;
            data->currentListenerPosition.y = targetPosition.y;
            Pa_Sleep((testPeriodInSec * 1000) / totalSteps);
        }
    }

    return stream;
}

void endPlayback(PaStream* stream) {
    PaError err = Pa_StopStream(stream);
    checkErr(err);

    err = Pa_CloseStream(stream);
    checkErr(err);

    err = Pa_Terminate();
    checkErr(err);
}