#pragma once

#include "portaudio.h"
#include "structs.h"

Point getCircularCoordinates(float circularPosition, float radius);

PaStream* startPlayback(paTestData* data);

void endPlayback(PaStream* stream);

void SetOutputDeviceIndex(int index);  // PaDeviceIndex, or paNoDevice for default