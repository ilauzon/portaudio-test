#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <portaudio.h>
#include <cstring>
#include <string>

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
#define OUTPUT_DEVICE       (5)

typedef struct
{
    float sine[TABLE_SIZE];
    int left_phase;
    int right_phase;
    float targetBalance; // 0.0 = left, 1.0 = right
    float currentBalance;
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
    
    for(unsigned long i = 0; i < framesPerBuffer; i++) {
        // Smoothly pan between left and right.
        // if( data->currentBalance < data->targetBalance )
        // {
        //     data->currentBalance += BALANCE_DELTA;
        // }
        // else if( data->currentBalance > data->targetBalance )
        // {
        //     data->currentBalance -= BALANCE_DELTA;
        // }

        data->currentBalance = data->targetBalance;
        // Apply left/right balance.
        // Change phase.
        int leftPhaseOffset = (int)(data->left_phase + (SAMPLE_RATE / 1000.0f * ITD_MS)) % TABLE_SIZE;
        int rightPhaseOffset = (int)(data->right_phase + (SAMPLE_RATE / 1000.0f * ITD_MS)) % TABLE_SIZE;
        // leftPhaseOffset = data->left_phase;
        // rightPhaseOffset = data->left_phase;
        // Change volume.
        *out++ = data->sine[leftPhaseOffset] * (0.75f - data->currentBalance * 0.5f);  /* left */
        *out++ = data->sine[rightPhaseOffset] * (0.25f + data->currentBalance * 0.5f);  /* right */

        data->left_phase += 1;
        if( data->left_phase >= TABLE_SIZE ) data->left_phase -= TABLE_SIZE;
        data->right_phase += 1;
        if( data->right_phase >= TABLE_SIZE ) data->right_phase -= TABLE_SIZE;
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
    data.left_phase = data.right_phase = 0;
    data.currentBalance = 0.0;
    data.targetBalance = 0.0;

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

    int stepsPerSec = 100;
    int testPeriodInSec = 10;
    int totalSteps = stepsPerSec * testPeriodInSec;
    for (int i = 0; i < totalSteps; i++) {
        float targetBalance = i / (float)totalSteps;
        data.targetBalance = targetBalance;
        Pa_Sleep((testPeriodInSec * 1000) / totalSteps);
    }

    err = Pa_StopStream(stream);
    checkErr(err);

    err = Pa_CloseStream(stream);
    checkErr(err);

    err = Pa_Terminate();
    checkErr(err);

    return EXIT_SUCCESS;
}