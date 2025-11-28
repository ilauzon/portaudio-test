#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include "utils.h"
#include "six_channel.h"


// distance from listener to each speaker
std::array<float, CHANNEL_COUNT>
calculateSpeakerDistances(Point subjectPosition,
                          const Point speakerPositions[CHANNEL_COUNT])
{
    std::array<float, CHANNEL_COUNT> distances{};

    for (int i = 0; i < CHANNEL_COUNT; ++i)
    {
        Point currentSpeaker = speakerPositions[i];
        float xDiff = subjectPosition.x - currentSpeaker.x;
        float yDiff = subjectPosition.y - currentSpeaker.y;
        distances[i] = std::sqrt(xDiff * xDiff + yDiff * yDiff);
    }

    return distances;
}

float distanceToGain(float distance) {
    return distance;
}

/**
 * Calculates the maximum possible gain that can be applied to each 
 * speaker, and sets data->maxGain to that value.
 */
void setMaxGain(paTestData* data) {
    float minX = data->subjectBounds[0].x;
    float minY = data->subjectBounds[0].y;
    float maxX = data->subjectBounds[1].x;
    float maxY = data->subjectBounds[1].y;
    std::array<Point, 4> corners = {
        Point {minX, minY}, 
        Point {minX, maxY}, 
        Point {maxX, minY}, 
        Point {maxX, maxY}, 
    };

    float maxDistance = 0;
    for (auto corner : corners) {
        auto cornerDistances = calculateSpeakerDistances(corner, data->speakerPositions);
        auto maxCornerDistance = *std::max_element(cornerDistances.begin(), cornerDistances.end());

        if (maxCornerDistance > maxDistance) maxDistance = maxCornerDistance;
    }

    float maxGain = distanceToGain(maxDistance);
    data->maxGain = maxGain;
}


// Get the point in 2D space that corresponds to a single-value position
// around the circle's circumference.
Point getCircularCoordinates(float circularPosition, float radius)
{
    float angle = circularPosition * 2.0f * M_PI;
    Point p;
    p.x = radius * std::cos(angle);
    p.y = radius * std::sin(angle);
    return p;
}

std::string getSixChannelName(int channel) {
    switch(channel) {
        case SixChannelSetup::BackLeft:     return "BackLeft";
        case SixChannelSetup::BackRight:    return "BackRight";
        case SixChannelSetup::FrontLeft:    return "FrontLeft";
        case SixChannelSetup::FrontRight:   return "FrontRight";
        case SixChannelSetup::Centre:       return "Centre";
        case SixChannelSetup::Subwoofer:    return "Subwoofer";
        default: exit(EXIT_FAILURE);
    }
}
