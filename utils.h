#pragma once
#include <array>
#include <sndfile.h>
#include <string>
#include <vector>
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
    float channelGains[CHANNEL_COUNT]; // the gain on each channel, from 0 to 1.
    Point currentListenerPosition; // currently targeted coordinates relative to subjectBounds, in offset metres.
    float listenerYaw; // the yaw of the listener's head, with 0 pointing towards the centre speaker and 0.2 pointing towards the front-left speaker.
    Point subjectBounds[2]; // bounds for the listener, in metres. (0) bottom left - min x and y, (1) top right - max x and y.
    Point speakerPositions[CHANNEL_COUNT]; // the position of each speaker relative to subjectBounds, in offset metres.
    float maxGain; // the maximum gain that can be applied to the signal of each speaker.
    std::vector<float> audio;
    unsigned long readIndex;
} paTestData;

std::array<float, CHANNEL_COUNT> calculateSpeakerDistances(
    Point subjectPosition, 
    const Point speakerPositions[CHANNEL_COUNT]
);

float distanceToGain(float distance);

void setMaxGain(paTestData* data);

Point getCircularCoordinates(float circularPosition, float radius);

std::string getSixChannelName(int channel);
