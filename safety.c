#include "safety.h"
#include "common.h"
#include "trajectory.h"

#define SAFE_DIST   0.8f
#define AIR_X_MIN   0.0f
#define AIR_X_MAX   GROUND
#define AIR_Y_MIN   0.0f
#define AIR_Y_MAX   GROUND
#define AIR_Z_MIN   0.0f
#define AIR_Z_MAX   GROUND

bool alertActive = false;
char alertMsg[256] = "";

int InAirspace(Pt p) {
    return p.x >= AIR_X_MIN && p.x <= AIR_X_MAX &&
           p.y >= AIR_Y_MIN && p.y <= AIR_Y_MAX &&
           p.z >= AIR_Z_MIN && p.z <= AIR_Z_MAX;
}

void SetAlert(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    vsnprintf(alertMsg, sizeof(alertMsg), fmt, a);
    va_end(a);
    alertActive = true;
}

static Pt DroneEnd(const Drone* d) {
    if (d->wc > 0) return d->wp[d->wc - 1].p;
    return d->start;
}

int CheckOverlap(int i) {
    if (i < 0 || i >= N || !D[i].act) return 0;

    Pt s1 = D[i].start;
    Pt e1 = DroneEnd(&D[i]);

    for (int j = 0; j < N; j++) {
        if (j == i || !D[j].act) continue;

        Pt s2 = D[j].start;
        Pt e2 = DroneEnd(&D[j]);

        if (Dist3(s1, s2) < SAFE_DIST) {
            SetAlert("%s and %s start too close (%.2f m) - will collide!",
                     D[i].name, D[j].name, Dist3(s1, s2));
            return 1;
        }
        if (Dist3(e1, e2) < SAFE_DIST) {
            SetAlert("%s and %s end too close (%.2f m) - will collide!",
                     D[i].name, D[j].name, Dist3(e1, e2));
            return 1;
        }
    }
    return 0;
}

static int LiveCheckBounds(void) {
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;

        Pt p = d->pos;
        if (p.x < AIR_X_MIN || p.x > AIR_X_MAX ||
            p.y < AIR_Y_MIN || p.y > AIR_Y_MAX ||
            p.z < AIR_Z_MIN || p.z > AIR_Z_MAX) {
            snprintf(alertMsg, sizeof(alertMsg),
                     "%s out of bounds (%.0f, %.0f, %.0f)",
                     d->name, p.x, p.y, p.z);
            alertActive = true;
            return 1;
        }
    }
    return 0;
}

static int LiveCheckCollide(void) {
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        for (int j = i + 1; j < N; j++) {
            if (!D[j].act) continue;
            float d = Dist3(D[i].pos, D[j].pos);
            if (d < SAFE_DIST) {
                snprintf(alertMsg, sizeof(alertMsg),
                         "%s too close to %s (%.2f m)",
                         D[i].name, D[j].name, d);
                alertActive = true;
                return 1;
            }
        }
    }
    return 0;
}

int LiveCheck(void) {
    if (LiveCheckBounds()) return 1;
    return LiveCheckCollide();
}
