
#include "mathf.h"
#include <math.h>

unsigned int gRandomSeed = 1;

int randomInt() {
    gRandomSeed = gRandomSeed * 22695477 + 1;
    return (gRandomSeed >> 16) & 0x7fff;
}

float fsign(float in) {
    if (in > 0.0f) {
        return 1.0f;
    } else if (in < 0.0f) {
        return -1.0f;
    } else {
        return 0.0f;
    }
}

float mathfLerp(float from, float to, float t) {
    return from * (1.0f - t) + to * t;
}

float mathfMoveTowards(float from, float to, float maxMove) {
    float offset = to - from;
    if (fabsf(offset) <= maxMove) {
        return to;
    } else {
        return fsign(offset) * maxMove + from;
    }
}

float mathfBounceBackLerp(float t) {
    return -t + t * t;
}

float mathfRandomFloat() {
    return (float)randomInt() / (float)0x7fff;
}