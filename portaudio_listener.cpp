#include "six_channel.h"
#include "utils.h"
#include "portaudio_listener.h"
#include <array>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <math.h>
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

// In portaudio_listener.cpp

static int paTestCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData)
{
    paTestData *data = (paTestData *)userData;
    float *out = (float *)outputBuffer;
    const float *in = (const float *)inputBuffer; // Cast input buffer

    // Temporary buffer to hold the 6-channel upmixed audio
    AudioBuffer channelSignals;
    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        channelSignals[ch].resize(framesPerBuffer);
    }

    // 1. LIVE UPMIXING (Stereo Input -> 6 Channel Internal)
    if (in != nullptr) {
        for (unsigned long i = 0; i < framesPerBuffer; i++) {
            // Input is Stereo (L, R, L, R...)
            // float left = in[i * 2]; 
            // float right = in[i * 2 + 1];

            // Same logic as your start.cpp, but live:
            channelSignals[FrontLeft][i]  = in[i*6];
            channelSignals[FrontRight][i] = in[i*6+1];
            channelSignals[Centre][i]     = in[i*6+4];
            channelSignals[Subwoofer][i]  = in[i*6+5];
            channelSignals[BackLeft][i]   = in[i*6+2];
            channelSignals[BackRight][i]  = in[i*6+3];
        }
    } else {
        // Silence if no input
        for (auto& ch : channelSignals) std::fill(ch.begin(), ch.end(), 0.0f);
    }

    // 2. APPLY ROTATION (Process the audio)
    // We pass our live buffer to your existing algorithm
    // applyRotation(data, &channelSignals);

    // 3. INTERLEAVE OUTPUT (6 Channel Internal -> Output Device)
    for (unsigned long frame = 0; frame < framesPerBuffer; frame++) {
        for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
            *out++ = channelSignals[ch][frame];
        }
    }

    return paContinue;
}

// ------------ Start / end playback ------------

// In portaudio_listener.cpp

// In portaudio_listener.cpp

// ... includes and paTestCallback remain the same ...

PaStream* startPlayback(paTestData *data)
{
    PaError err = Pa_Initialize();
    checkErr(err);

    int numDevices = Pa_GetDeviceCount();
    int inputDevice = paNoDevice;
    int outputDevice = paNoDevice;

    std::printf("\n==========================================\n");
    std::printf("     AVAILABLE AUDIO DEVICES\n");
    std::printf("==========================================\n");

    // 1. PRINT ALL DEVICES
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        std::printf("[%d] %s\n      (In: %d | Out: %d)\n", 
            i, info->name, info->maxInputChannels, info->maxOutputChannels);
    }
    std::printf("==========================================\n");

    // 2. HARDCODED SEARCH (You must edit 'outputSearch'!)
    // We search for "BlackHole" for input (Source)
    // We search for your "USB" or "Surround" device for output (Speakers)
    
    const char* inputSearch  = "BlackHole"; 
    const char* outputSearch = "USB"; // <--- CHANGE THIS IF YOUR SPEAKER NAME IS DIFFERENT

    // Find Input
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        if (strstr(info->name, inputSearch) && info->maxInputChannels >= 2) {
            inputDevice = i;
            break;
        }
    }

    // Find Output
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        // Look for name match AND 6 channels
        if (strstr(info->name, outputSearch) && info->maxOutputChannels >= CHANNEL_COUNT) {
            outputDevice = i;
            break;
        }
    }

    // 3. SAFETY CHECK
    if (inputDevice == paNoDevice) {
        std::printf("ERROR: Could not find Input device '%s'.\n", inputSearch);
        return nullptr;
    }
    
    if (outputDevice == paNoDevice) {
        std::printf("ERROR: Could not find Output device '%s' with %d channels.\n", outputSearch, CHANNEL_COUNT);
        std::printf("TIP: Check the list above. Change 'outputSearch' string in code to match your speaker name.\n");
        return nullptr; // STOP HERE so we don't accidentally use BlackHole as output
    }

    const PaDeviceInfo *inInfo = Pa_GetDeviceInfo(inputDevice);
    const PaDeviceInfo *outInfo = Pa_GetDeviceInfo(outputDevice);

    std::printf("\n>>> ROUTING CONFIRMED <<<\n");
    std::printf("IN (Spotify): %s\n", inInfo->name);
    std::printf("OUT (Sound):  %s\n", outInfo->name);
    std::printf("-------------------------\n");

    // 4. OPEN STREAM
    PaStreamParameters inputParams;
    std::memset(&inputParams, 0, sizeof(inputParams));
    inputParams.device = inputDevice;
    inputParams.channelCount = 6; // Read Stereo from BlackHole
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency = inInfo->defaultLowInputLatency;

    PaStreamParameters outputParams;
    std::memset(&outputParams, 0, sizeof(outputParams));
    outputParams.device = outputDevice;
    outputParams.channelCount = CHANNEL_COUNT; // Write 5.1 to Speakers
    outputParams.sampleFormat = paFloat32;
    outputParams.suggestedLatency = outInfo->defaultLowOutputLatency;

    PaStream* stream = nullptr;
    err = Pa_OpenStream(&stream, &inputParams, &outputParams, SAMPLE_RATE, FRAMES_PER_BUFFER, paNoFlag, paTestCallback, data);
    checkErr(err);

    err = Pa_StartStream(stream);
    checkErr(err);

    while (true) {
         std::string line;
         std::getline(std::cin, line);
         if (!line.empty()) {
             float listenerX, listenerY, yaw;
             if (sscanf(line.c_str(), "%f,%f,%f", &listenerX, &listenerY, &yaw) == 3) {
                data->currentListenerPosition = Point { listenerX, listenerY };
                data->listenerYaw = yaw;
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

