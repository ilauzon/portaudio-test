#pragma once
#include <array>
#include <sndfile.h>
#include <string>
#define TABLE_SIZE          (SAMPLE_RATE / TONE_HZ)
#define TONE_HZ             (200)
#define SAMPLE_RATE         (44100)
#define CHANNEL_COUNT       (6)
// (0) FL, (1) FR, (2) LR, (3) BR, (4) CEN, (5) SUB


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
    Point subjectBounds[2]; // bounds for the listener, in metres. (0) bottom left - min x and y, (1) top right - max x and y.
    Point speakerPositions[CHANNEL_COUNT]; // the position of each speaker relative to subjectBounds, in offset metres.
    float maxGain; // the maximum gain that can be applied to the signal of each speaker.
    SNDFILE* file;
    SF_INFO* info;
} paTestData;

std::array<float, CHANNEL_COUNT> calculateSpeakerDistances(
    Point subjectPosition, 
    const Point speakerPositions[CHANNEL_COUNT]
);

float distanceToGain(float distance);

void setMaxGain(paTestData* data);

Point getCircularCoordinates(float circularPosition, float radius);

std::string getSixChannelName(int channel);
