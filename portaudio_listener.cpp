#include "six_channel.h"
#include "utils.h"
#include "portaudio.h"
#include <array>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <math.h>
#include <portaudio.h>
#include <stdlib.h>
#include <sndfile.h>
#include <vector>

#define FRAMES_PER_BUFFER   (256)
#ifndef M_PI
#define M_PI (3.14159265)
#endif

static int gOutputDeviceIndex = paNoDevice;

void SetOutputDeviceIndex(int index)
{
    gOutputDeviceIndex = index;
}

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
    // std::array<float, CHANNEL_COUNT> distances =
    //     calculateSpeakerDistances(data->currentListenerPosition,
    //                               data->speakerPositions);

    // for (unsigned long i = 0; i < framesPerBuffer; ++i)
    // {
    //     for (int c = 0; c < CHANNEL_COUNT; ++c)
    //     {
    //             float speakerDistance = distances[c];
    
    //         // Simple distance → gain mapping:
    //         // closer => louder, farther => quieter
    //         float gain = distanceToGain(speakerDistance) / data->maxGain;

    //         // Store for GUI visualization
    //             data->channelVolumes[c] = gain;

    //         // Phase update
    //         int phaseOffset = data->channelPhases[c];
    //         data->channelPhases[c] += 1;
    //         if (data->channelPhases[c] >= TABLE_SIZE) {
    //             data->channelPhases[c] -= TABLE_SIZE;
    //         }

    //         float sample = data->sine[phaseOffset] * gain;

    //         *out++ = sample;
    //     }
    // }

    std::vector<float> interleaved_buffer(framesPerBuffer * CHANNEL_COUNT);

    // Read all the frames into the interleaved buffer
    sf_count_t frames_read = sf_readf_float(data->file, interleaved_buffer.data(), framesPerBuffer);

    std::array<std::vector<float>, CHANNEL_COUNT> channelSignals;
    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        channelSignals[ch].resize(framesPerBuffer);
    }

    for (sf_count_t i = 0; i < framesPerBuffer; ++i) {
        size_t base_index = i * CHANNEL_COUNT;
        channelSignals[0][i] = interleaved_buffer[base_index];
        channelSignals[1][i] = interleaved_buffer[base_index + 1];
        channelSignals[2][i] = interleaved_buffer[base_index + 2];
        channelSignals[3][i] = interleaved_buffer[base_index + 3];
        channelSignals[4][i] = interleaved_buffer[base_index + 4];
        channelSignals[5][i] = interleaved_buffer[base_index + 5];
    }

    for (unsigned long frame = 0; frame < framesPerBuffer; frame++) {
        out[FrontLeft] = channelSignals[0][frame];
        out[FrontRight] = channelSignals[1][frame];
        out[Centre] = channelSignals[2][frame];
        out[Subwoofer] = channelSignals[3][frame];
        out[BackLeft] = channelSignals[4][frame];
        out[BackRight] = channelSignals[5][frame];
        out += CHANNEL_COUNT;   // advance to next interleaved frame
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
    // const int stepsPerSec     = 100;
    // const int testPeriodInSec = 10;
    // const int totalSteps      = stepsPerSec * testPeriodInSec;
    // const int sleepMs         = (testPeriodInSec * 1000) / totalSteps;

    while (true) {
        std::string line;

        std::getline(std::cin, line);

        if (!line.empty()) { // Check if any line was read
            printf("%s\n",line.c_str());
            char name[50];
            int age;
            float listenerX;
            float listenerY;
            float yaw;

            if (sscanf(line.c_str(), "%f,%f,%f", &listenerX, &listenerY, &yaw) == 3) {
                data->currentListenerPosition = Point { listenerX, listenerY };
            }

        }
        
        Pa_Sleep(5);
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