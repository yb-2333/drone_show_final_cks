#ifndef UTILS_H
#define UTILS_H

#include "common.h"

void Msg(const char* f, ...);

int In(Rectangle r);

int Btn(Rectangle r, const char* t, Color c);

int Txt(Rectangle r, char* buf, int max, const char* label);

void Sep(int x, int y, int w);

float Sld(Rectangle r, float v, float lo, float hi, const char* f);

Color Hsv2Rgb(float h, float s, float v);

#endif
