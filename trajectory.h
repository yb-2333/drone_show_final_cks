#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include "common.h"

float EaseLinear(float u);
float EaseInQuad(float u);
float EaseOutQuad(float u);
float EaseInOutCubic(float u);
float SmoothStep(float u);

float Ease(float u, int mode);

Pt CatmullRom(Pt p0, Pt p1, Pt p2, Pt p3, float u);

float Dist3(Pt a, Pt b);

float PathLen(const Drone* d);

Pt DronePosAt(const Drone* d, float s);

#endif
