#include "trajectory.h"
#include "common.h"

float EaseLinear(float u) {
    return u;
}

float EaseInQuad(float u) {
    return u * u;
}

float EaseOutQuad(float u) {
    return u * (2.0f - u);
}

float EaseInOutCubic(float u) {
    if (u < 0.5f)
        return 4.0f * u * u * u;
    float t = 1.0f - u;
    return 1.0f - 4.0f * t * t * t;
}

float SmoothStep(float u) {
    return u * u * (3.0f - 2.0f * u);
}

float Ease(float u, int mode) {
    switch (mode) {
        case PM_EASED:  return SmoothStep(u);
        case PM_LINEAR: return EaseLinear(u);
        case PM_SPLINE: return EaseLinear(u);
        default:        return EaseLinear(u);
    }
}

static float CR(float p0, float p1, float p2, float p3, float u) {
    float u2 = u * u;
    float u3 = u2 * u;
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * u +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u3
    );
}

Pt CatmullRom(Pt p0, Pt p1, Pt p2, Pt p3, float u) {
    return (Pt){
        CR(p0.x, p1.x, p2.x, p3.x, u),
        CR(p0.y, p1.y, p2.y, p3.y, u),
        CR(p0.z, p1.z, p2.z, p3.z, u)
    };
}

float Dist3(Pt a, Pt b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static float ClampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float LerpF(float a, float b, float t) {
    return a + (b - a) * t;
}

static Pt LerpPt(Pt a, Pt b, float t) {
    return (Pt){
        LerpF(a.x, b.x, t),
        LerpF(a.y, b.y, t),
        LerpF(a.z, b.z, t)
    };
}

float PathLen(const Drone* d) {
    float len = 0;
    Pt cur = d->start;

    for (int w = 0; w < d->wc; w++) {
        Pt next = d->wp[w].p;
        len += Dist3(cur, next);
        cur = next;
    }
    return len;
}

static Pt PathPoint(const Drone* d, int idx) {
    if (idx <= 0) return d->start;
    if (idx > d->wc) return d->wp[d->wc - 1].p;
    return d->wp[idx - 1].p;
}

Pt DronePosAt(const Drone* d, float s) {
    int mode = d->pm;

    if (d->wc <= 0) return d->start;

    Pt  cur = d->start;
    int seg = 0;

    for (int w = 0; w < d->wc; w++) {
        Pt next = d->wp[w].p;
        float L = Dist3(cur, next);

        if (s <= L || w == d->wc - 1) {
            float u;
            if (L < 1e-4f) u = 1.0f;
            else
                u = ClampF(s / L, 0.0f, 1.0f);

            if (mode == PM_SPLINE)
                return CatmullRom(PathPoint(d, seg - 1),
                                  cur,
                                  next,
                                  PathPoint(d, seg + 2),
                                  u);

            return LerpPt(cur, next, Ease(u, mode));
        }

        s -= L;
        cur = next;
        seg++;
    }

    return cur;
}
