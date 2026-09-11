#ifndef SAFETY_H
#define SAFETY_H

#include "common.h"

int InAirspace(Pt p);

int CheckOverlap(int i);

extern bool alertActive;

extern char alertMsg[256];

void SetAlert(const char* fmt, ...);

int LiveCheck(void);

#endif
