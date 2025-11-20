#include <cmath>
#include <cstdlib>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <portaudio.h>
#include <cstring>
#include <string>
#include <array>

#define SAMPLE_RATE         (44100)
#define FRAMES_PER_BUFFER   (256)
#define CHANNEL_COUNT       (2)
#ifndef M_PI
#define M_PI                (3.14159265)
#endif
#define TONE_HZ             (200)
#define TABLE_SIZE          (SAMPLE_RATE / TONE_HZ)
#define ITD_MS              (0.5) /* Interaural Time Difference (ITD): The time it takes sound to traverse the distance between your ears */
#define BALANCE_DELTA       (0.001)
#define OUTPUT_DEVICE       (6)
#define REFERENCE_DISTANCE  (1)

typedef struct {
    float x;
    float y;
} Point;

typedef struct
{
    float sine[TABLE_SIZE]; // the signal to play through all channels.
    unsigned int channelPhases[CHANNEL_COUNT]; // the phase of each channel.
    float channelVolumes[CHANNEL_COUNT]; // the volume of each channel, from 0 to 1.
    Point currentListenerPosition; // currently targeted coordinates relative to subjectBounds, in offset metres.
    Point subjectBounds[2]; // bounds for the listener, in metres.
    Point speakerPositions[CHANNEL_COUNT]; // the position of each speaker relative to subjectBounds, in offset metres.
} paTestData;

static void checkErr(PaError err) {
    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }
}

static inline float max(float a, float b) {
    return a > b ? a : b;
}

std::array<float, CHANNEL_COUNT> calculateSpeakerDistances(Point subjectPosition, Point speakerPositions[CHANNEL_COUNT]) {
    std::array<float, CHANNEL_COUNT> distances{};

    for (int i = 0; i < CHANNEL_COUNT; i++) {
        Point currentSpeaker = speakerPositions[i];
        float xDiff = subjectPosition.x - currentSpeaker.x;
        float yDiff = subjectPosition.y - currentSpeaker.y;
        distances[i] = std::sqrt(xDiff * xDiff + yDiff * yDiff);
    }

    return distances;
}

static int paTestCallback(
        const void* inputBuffer,
        void* outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void* userData
) {
    paTestData *data = (paTestData*)userData;
    float* out = (float*)outputBuffer;
    (void)inputBuffer;

    std::array<float, CHANNEL_COUNT> distances = calculateSpeakerDistances(data->currentListenerPosition, data->speakerPositions);
    
    for(unsigned long i = 0; i < framesPerBuffer; i++) {
        for (int i = 0; i < CHANNEL_COUNT; i++) {
            float speakerDistance = distances[i];
            float gainCompensation = speakerDistance / REFERENCE_DISTANCE;
            data->channelVolumes[i] = gainCompensation;

            // Change phase.
            int phaseOffset = data->channelPhases[i];
            data->channelPhases[i] += 1;
            if( data->channelPhases[i] >= TABLE_SIZE ) data->channelPhases[i] -= TABLE_SIZE;

            // Change volume.
            *out++ = data->sine[phaseOffset] * (gainCompensation);
        }
    }

    return paContinue;
}

int startPlayback() {
    PaError err;
    err = Pa_Initialize();
    checkErr(err);

    int outputDevice = OUTPUT_DEVICE;

    int numDevices = Pa_GetDeviceCount();
    printf("Number of devices: %d\n", numDevices);

    if (numDevices < 0) {
        printf("Error getting device count: PaError %d\n", numDevices);
        exit(EXIT_FAILURE);
    } else if (numDevices == 0) {
        printf("No available audio devices found.\n");
        exit(EXIT_SUCCESS);
    }

    const PaDeviceInfo* deviceInfo;
    for (int i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        printf("Device %d:\n", i);
        printf("\tname: %s\n", deviceInfo->name);
        printf("\tmaxInputChannels: %d\n", deviceInfo->maxInputChannels);
        printf("\tmaxOutputChannels: %d\n", deviceInfo->maxOutputChannels);
        printf("\tsdefaultSampleRate: %f\n", deviceInfo->defaultSampleRate);
    }

    /* initialise wavetable */
    paTestData data;
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

    PaStreamParameters outputParameters;

    memset(&outputParameters, 0, sizeof(outputParameters));
    outputParameters.channelCount = CHANNEL_COUNT;
    outputParameters.device = outputDevice;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputDevice)->defaultLowOutputLatency;

    PaStream* stream;
    err = Pa_OpenStream(
        &stream,
        NULL,
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paNoFlag,
        paTestCallback,
        &data
    );
    checkErr(err);

    err = Pa_StartStream(stream);
    checkErr(err);

    // int stepsPerSec = 100;
    // int testPeriodInSec = 10;
    // int totalSteps = stepsPerSec * testPeriodInSec;
    // for (int i = 0; i < totalSteps; i++) {
    //     float targetBalance = i / (float)totalSteps;
    //     data.targetListenerPosition = targetBalance;
    //     Pa_Sleep((testPeriodInSec * 1000) / totalSteps);
    // }

    Pa_Sleep(10000);

    err = Pa_StopStream(stream);
    checkErr(err);

    err = Pa_CloseStream(stream);
    checkErr(err);

    err = Pa_Terminate();
    checkErr(err);

    return EXIT_SUCCESS;
}