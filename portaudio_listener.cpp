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

typedef std::array<std::vector<float>, CHANNEL_COUNT> AudioBuffer;

void SetOutputDeviceIndex(int index)
{
    gOutputDeviceIndex = index;
}

static void checkErr(PaError err)
{
    if (err != paNoError)
    {
        std::printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        std::fflush(stdout);
        std::exit(EXIT_FAILURE);
    }
}

// read the audio for the number of frames in a buffer from a given file and return it.
static AudioBuffer readAudio(paTestData* data) {
    const size_t samplesNeeded = FRAMES_PER_BUFFER * CHANNEL_COUNT;

    // Prevent out-of-range access
    if (data->readIndex + samplesNeeded > data->audio.size()) {
        // loop
        data->readIndex = 0;
    }

    auto beginIterator = data->audio.begin() + data->readIndex;
    auto endIterator = beginIterator + samplesNeeded;
    data->readIndex += samplesNeeded;
    std::vector<float> interleaved_buffer(beginIterator, endIterator);

    AudioBuffer channelSignals;

    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        channelSignals[ch].resize(FRAMES_PER_BUFFER);
    }

    for (unsigned long i = 0; i < FRAMES_PER_BUFFER; ++i) {
        size_t base_index = i * CHANNEL_COUNT;
        channelSignals[FrontLeft][i] = interleaved_buffer[base_index];
        channelSignals[FrontRight][i] = interleaved_buffer[base_index + 1];
        channelSignals[Centre][i] = interleaved_buffer[base_index + 2];
        channelSignals[Subwoofer][i] = interleaved_buffer[base_index + 3];
        channelSignals[BackLeft][i] = interleaved_buffer[base_index + 4];
        channelSignals[BackRight][i] = interleaved_buffer[base_index + 5];
    }
    
    return channelSignals;
}

static float wrapAngle(float a) {
    while (a >  M_PI) a -= 2 * M_PI;
    while (a < -M_PI) a += 2 * M_PI;
    return a;
}

static void applyRotation(paTestData* data, AudioBuffer* centeredAudioBuffer)
{
    const int SPEAKERS = CHANNEL_COUNT - 1; // exclude subwoofer
    const float TWO_PI = 2 * M_PI;

    AudioBuffer& buf = *centeredAudioBuffer;

    std::array<float, CHANNEL_COUNT> distances =
        calculateSpeakerDistances(data->currentListenerPosition,
                                  data->speakerPositions);

    size_t frameCount = buf[0].size();

    // 1. Compute real speaker angles (excluding subwoofer)
    float realAngles[SPEAKERS];
    for (int ch = 0; ch < SPEAKERS; ++ch) {
        const Point& p = data->speakerPositions[ch];
        realAngles[ch] = -wrapAngle(atan2f(p.y, p.x) - 0.25 * TWO_PI);
    }

    // 2. Define evenly-spaced virtual speakers
    float virtualAngles[CHANNEL_COUNT];
    virtualAngles[Centre] = wrapAngle(TWO_PI * 0 / SPEAKERS);
    virtualAngles[FrontLeft] = wrapAngle(TWO_PI * -1 / SPEAKERS);
    virtualAngles[BackLeft] = wrapAngle(TWO_PI * -2 / SPEAKERS);
    virtualAngles[BackRight] = wrapAngle(TWO_PI * -3 / SPEAKERS);
    virtualAngles[FrontRight] = wrapAngle(TWO_PI * -4 / SPEAKERS);
    virtualAngles[Subwoofer] = 0;

    // 3. Rotate virtual speakers opposite listener yaw
    float rotatedAngles[SPEAKERS];
    for (int v = 0; v < SPEAKERS; ++v)
        rotatedAngles[v] = wrapAngle(virtualAngles[v] - data->listenerYaw * TWO_PI);

    // 4. Compute Gaussian mixing weights
    float weights[SPEAKERS][SPEAKERS];
    const float sigma = 0.7f;

    for (int v = 0; v < SPEAKERS; ++v)
    {
        float sum = 0.0f;
        for (int r = 0; r < SPEAKERS; ++r) {
            float d = wrapAngle(rotatedAngles[v] - realAngles[r]);
            float w = expf(-(d*d)/(2*sigma*sigma));
            weights[v][r] = w;
            sum += w;
        }

        for (int r = 0; r < SPEAKERS; ++r) {
            weights[v][r] /= sum;
        }
    }

    // 5. Allocate output buffer
    AudioBuffer out;
    for (int ch = 0; ch < CHANNEL_COUNT; ++ch)
        out[ch].assign(frameCount, 0.0f);

    // 6. Mix rotated main speakers
    for (size_t i = 0; i < frameCount; ++i) {
        for (int v = 0; v < SPEAKERS; ++v) {
            float in = buf[v][i];
            for (int r = 0; r < SPEAKERS; ++r) {
                float distanceGain = distanceToGain(distances[r]) / data->maxGain;
                out[r][i] += in * weights[v][r] * distanceGain;
            }
        }
        // 7. Copy subwoofer directly (no panning)
        out[Subwoofer][i] = buf[Subwoofer][i];
    }

    buf = out;
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

    AudioBuffer channelSignals = readAudio(data);
    applyRotation(data, &channelSignals);

    for (unsigned long frame = 0; frame < FRAMES_PER_BUFFER; frame++) {
        for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
            out[ch] = channelSignals[ch][frame];
        }
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
                // assume camera is at centre speaker
                Point cameraPosition = data->speakerPositions[Centre];
                data->currentListenerPosition = Point { listenerX + cameraPosition.x, listenerY + cameraPosition.y };
                data->listenerYaw = yaw;
            }
        }
        Pa_Sleep(5); // wait 5 ms between stdin updates
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