#include <stdlib.h>
#include <stdio.h>
#include <portaudio.h>
#include <cstring>
#include <array>

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 256
#define CHANNEL_COUNT 2

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
    float* inputBuf = (float*)inputBuffer;
    (void)outputBuffer;
    
    int dispSize = 100;
    printf("\r");

    std::array<float, CHANNEL_COUNT> volumePerChannel;

    for (unsigned long i = 0; i < framesPerBuffer * CHANNEL_COUNT; i += CHANNEL_COUNT) {
        for (unsigned long j = i; j < CHANNEL_COUNT; j++) {
            volumePerChannel[j / framesPerBuffer] = max(volumePerChannel[j / framesPerBuffer], std::abs(inputBuf[i]));
        }
    }

    for (int i = 0; i < dispSize; i++) {
        float barProportion = i / (float)dispSize;
        if (volumePerChannel[0] >= barProportion) {
            printf("█");
        } else {
            printf(" ");
        }
    }

    fflush(stdout);
    return paContinue;
}

int main() {
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

    const PaDeviceInfo* deviceInfo;
    for (int i = 0; i < numDevices; i++) {
        deviceInfo = Pa_GetDeviceInfo(i);
        printf("Device %d:\n", i);
        printf("\tname: %s\n", deviceInfo->name);
        printf("\tmaxInputChannels: %d\n", deviceInfo->maxInputChannels);
        printf("\tmaxOutputChannels: %d\n", deviceInfo->maxOutputChannels);
        printf("\tsdefaultSampleRate: %f\n", deviceInfo->defaultSampleRate);
    }

    int inputDevice = 4;
    int outputDevice = 5;

    PaStreamParameters inputParameters;
    PaStreamParameters outputParameters;

    memset(&inputParameters, 0, sizeof(inputParameters));
    inputParameters.channelCount = CHANNEL_COUNT;
    inputParameters.device = inputDevice;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputDevice)->defaultLowInputLatency;

    memset(&outputParameters, 0, sizeof(outputParameters));
    outputParameters.channelCount = CHANNEL_COUNT;
    outputParameters.device = outputDevice;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputDevice)->defaultLowOutputLatency;

    PaStream* stream;
    err = Pa_OpenStream(
        &stream,
        &inputParameters,
        &outputParameters,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        paNoFlag,
        paTestCallback,
        NULL
    );
    checkErr(err);

    err = Pa_StartStream(stream);
    checkErr(err);

    Pa_Sleep(10 * 1000);

    err = Pa_StopStream(stream);
    checkErr(err);

    err = Pa_CloseStream(stream);
    checkErr(err);

    err = Pa_Terminate();
    checkErr(err);

    return EXIT_SUCCESS;
}