
#ifndef PORTAUDIO_LISTENER_H
#define PORTAUDIO_LISTENER_H

#include "portaudio.h"

#define TABLE_SIZE          (SAMPLE_RATE / TONE_HZ)
#define TONE_HZ             (200)
#define SAMPLE_RATE         (44100)
#define CHANNEL_COUNT       (6)

typedef struct {
    float x;
    float y;
} Point;

typedef struct
{
    float sine[TABLE_SIZE]; // the signal to play through all channels.
    int channelPhases[CHANNEL_COUNT]; // the phase of each channel.
    float channelVolumes[CHANNEL_COUNT]; // the volume of each channel, from 0 to 1.
    Point currentListenerPosition; // currently targeted coordinates relative to subjectBounds, in offset metres.
    Point subjectBounds[2]; // bounds for the listener, in metres.
    Point speakerPositions[CHANNEL_COUNT]; // the position of each speaker relative to subjectBounds, in offset metres.
} paTestData;

PaStream* startPlayback(paTestData* data);

void endPlayback(PaStream* stream);

#endif