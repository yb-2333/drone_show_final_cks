#ifndef STATS_H
#define STATS_H

#include "common.h"

typedef struct {
    int   drones;
    int   waypoints;
    float totalLen;
    float maxLen;
    float minLen;
    float avgLen;
    float duration;
    Pt    bmin;
    Pt    bmax;
} Stats;

Stats ComputeStats(void);

#endif
