
#ifndef _MATH_MATHF_H
#define _MATH_MATHF_H

int randomInt();

float fsign(float in);
float mathfLerp(float from, float to, float t);
float mathfMoveTowards(float from, float to, float maxMove);
float mathfBounceBackLerp(float t);
float mathfRandomFloat();

#endif