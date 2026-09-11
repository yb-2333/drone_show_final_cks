#include "stats.h"
#include "common.h"
#include "trajectory.h"

static void BBoxAdd(Pt* bmin, Pt* bmax, Pt p) {
    if (p.x < bmin->x) bmin->x = p.x;
    if (p.y < bmin->y) bmin->y = p.y;
    if (p.z < bmin->z) bmin->z = p.z;
    if (p.x > bmax->x) bmax->x = p.x;
    if (p.y > bmax->y) bmax->y = p.y;
    if (p.z > bmax->z) bmax->z = p.z;
}

static void StatsInit(Stats* s) {
    memset(s, 0, sizeof(*s));
    s->minLen = 1e9f;
    s->bmin   = (Pt){1e9f, 1e9f, 1e9f};
    s->bmax   = (Pt){-1e9f, -1e9f, -1e9f};
}

static float PlaySpeed(void) {
    float vref = spd * 3.0f;
    if (vref < 0.01f) vref = 0.01f;
    return vref;
}

static void StatAccum(Stats* s, const Drone* d) {
    s->drones++;
    s->waypoints += d->wc;

    float L = PathLen(d);
    s->totalLen += L;
    if (d->wc > 0) {
        if (L > s->maxLen) s->maxLen = L;
        if (L < s->minLen) s->minLen = L;
    }

    BBoxAdd(&s->bmin, &s->bmax, d->start);
    for (int w = 0; w < d->wc; w++)
        BBoxAdd(&s->bmin, &s->bmax, d->wp[w].p);
}

static void StatFinalize(Stats* s) {

    if (s->drones > 0)
        s->avgLen = s->totalLen / s->drones;
    else
        s->minLen = 0;

    s->duration = s->maxLen / PlaySpeed();
}

Stats ComputeStats(void) {
    Stats s;
    StatsInit(&s);

    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;
        StatAccum(&s, d);
    }

    StatFinalize(&s);
    return s;
}
