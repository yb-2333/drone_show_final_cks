#include "drone.h"
#include "common.h"
#include "utils.h"
#include "safety.h"
#include "trajectory.h"

const char* LightName(Light l) {
    switch (l) {
        case L_OFF:     return "Off";
        case L_ON:      return "On";
        case L_BLINK:   return "Blink";
        case L_PULSE:   return "Pulse";
        case L_CHASE:   return "Chase";
        case L_RAINBOW: return "Rainbow";
        default:        return "?";
    }
}

void PrintDrone(const Drone* d) {
    printf("[%s] pos=(%.0f, %.0f, %.0f)  color=%s  light=%s\n",
           d->name, d->pos.x, d->pos.y, d->pos.z,
           LCN[d->color], LightName(d->light));
    fflush(stdout);
}

void FillCoords(const Drone* d) {
    snprintf(sx, sizeof(sx), "%.0f", d->start.x);
    snprintf(sy, sizeof(sy), "%.0f", d->start.y);
    snprintf(sz, sizeof(sz), "%.0f", d->start.z);
}

static void MoveDroneTo(int idx, Pt p) {
    D[idx].start = p;
    D[idx].pos   = p;
    D[idx].h     = p.z;
}

void ApplySetupCoords(void) {
    if (S < 0 || S >= N || !D[S].act) return;

    Pt p = (Pt){ (float)atof(sx), (float)atof(sy), (float)atof(sz) };
    if (!InAirspace(p)) {
        SetAlert("Position out of range (%.0f, %.0f, %.0f)", p.x, p.y, p.z);
        return;
    }

    MoveDroneTo(S, p);
    CheckOverlap(S);
    Msg("%s moved to (%.0f, %.0f, %.0f)", D[S].name, p.x, p.y, p.z);
    PrintDrone(&D[S]);
}

static void InitDrone(Drone* d, Pt sp, int index) {
    memset(d, 0, sizeof(Drone));
    d->act    = 1;
    d->light  = L_ON;
    d->color  = ic;
    d->bon    = 1;
    d->h      = sp.z;
    d->espeed = 1.0f;
    d->pm     = PM_EASED;
    d->ph     = (float)index;

    d->start = sp;
    d->pos   = sp;
}

void MakeDrone(void) {

    if (N >= MAX_DRONES) {
        Msg("Max %d drones!", MAX_DRONES);
        return;
    }

    float px = (float)atof(sx);
    float py = (float)atof(sy);
    float pz = (float)atof(sz);

    Pt sp = (Pt){px, py, pz};
    if (!InAirspace(sp)) {
        SetAlert("Start out of range (%.0f, %.0f, %.0f)", px, py, pz);
        return;
    }

    Drone* d = &D[N];
    InitDrone(d, sp, N);

    snprintf(d->name, MAX_NAME, "D-%d", N + 1);
    N++;
    S = N - 1;

    Msg("Created %s (%.0f,%.0f,%.0f) [%s]",
        d->name, px, py, pz, LCN[ic]);

    CheckOverlap(N - 1);
    PrintDrone(d);
}

void DelDrone(int i) {
    if (i < 0 || i >= N) return;

    for (int j = i; j < N - 1; j++)
        D[j] = D[j + 1];

    N--;

    if (S >= N) S = N - 1;
}

static Color ColorMul(Color c, float k) {
    int r = (int)(c.r * k); if (r > 255) r = 255;
    int g = (int)(c.g * k); if (g > 255) g = 255;
    int b = (int)(c.b * k); if (b > 255) b = 255;
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, c.a };
}

static Color RcPulse(Drone* d) {
    float k = 0.5f + 0.5f * sinf(d->ph);
    float t = 0.2f + 0.8f * k;
    return ColorMul(LC[d->color], t);
}

static Color RcChase(Drone* d) {
    float k = 0.5f + 0.5f * sinf(d->ph);
    return (k > 0.4f) ? LC[d->color] : (Color){35, 35, 45, 255};
}

static Color RcRainbow(Drone* d) {
    float h = fmodf(d->ph * 0.5f, 1.0f);
    return Hsv2Rgb(h, 1.0f, 1.0f);
}

static Color RcLight(Drone* d) {
    switch (d->light) {
        case L_OFF:
            return (Color){50, 50, 60, 255};
        case L_ON:
            return LC[d->color];
        case L_BLINK:

            return d->bon ? LC[d->color] : (Color){35, 35, 45, 255};
        case L_PULSE:
            return RcPulse(d);
        case L_CHASE:
            return RcChase(d);
        case L_RAINBOW:
            return RcRainbow(d);
        default:
            return GRAY;
    }
}

static Color RcHighlight(Color b) {
    b.r = (unsigned char)(b.r * 1.3f > 255 ? 255 : b.r * 1.3f);
    b.g = (unsigned char)(b.g * 1.3f > 255 ? 255 : b.g * 1.3f);
    b.b = (unsigned char)(b.b * 1.3f > 255 ? 255 : b.b * 1.3f);
    return b;
}

Color RC(Drone* d) {
    Color b = RcLight(d);
    if (d->sel) b = RcHighlight(b);
    return b;
}

static void DdPath(Drone* d) {
    if (d->wc <= 0) return;

    for (int i = 0; i < d->wc; i++) {

        DrawSphere((Vector3){d->wp[i].p.x, d->wp[i].p.z, d->wp[i].p.y},
                   DR * 0.7f, Ye);

        Pt pr = (i == 0) ? d->start : d->wp[i - 1].p;
        DrawLine3D((Vector3){pr.x, pr.z, pr.y},
                   (Vector3){d->wp[i].p.x, d->wp[i].p.z, d->wp[i].p.y},
                   Fade(Ye, 0.5f));
    }

    float L = PathLen(d);
    int   n = 48;
    Pt    prev = d->start;
    for (int k = 1; k <= n; k++) {
        float s = L * k / n;
        Pt    p = DronePosAt(d, s);
        DrawLine3D((Vector3){prev.x, prev.z, prev.y},
                   (Vector3){p.x, p.z, p.y}, Fade(Bl, 0.6f));
        prev = p;
    }
}

void DD(Drone* d) {
    if (!d->act) return;

    Pt    p = d->pos;
    Color c = RC(d);

    DrawSphere((Vector3){p.x, p.z, p.y}, DR * 1.3f, c);

    DrawSphere((Vector3){p.x, p.z, p.y}, DR * 1.9f, Fade(c, 0.3f));

    DrawSphere((Vector3){p.x, p.z, p.y}, DR * 0.5f, (Color){28, 28, 36, 255});

    if (d->sel)
        DrawCircle3D((Vector3){p.x, p.z, p.y}, DR * 2.0f,
                     (Vector3){0, 1, 0}, 0, Bl);

    if (M == M_EDIT)
        DdPath(d);
}

static void ResetDrone(Drone* d) {
    d->pos   = d->start;
    d->ci    = 0;
    d->fin   = 0;
    d->flown = 0;
}

void Rst(void) {
    for (int i = 0; i < N; i++)
        ResetDrone(&D[i]);

    play  = false;
    pause = false;
    prog  = 0;
}

static int AdvanceDrones(float dt) {
    int done = 0;

    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];

        if (!d->act || d->fin) { done++; continue; }

        float L = PathLen(d);
        if (L < 1e-4f) { d->fin = 1; done++; continue; }

        d->flown += spd * 3.0f * dt;

        if (d->flown >= L) { d->flown = L; d->fin = 1; }

        d->pos = DronePosAt(d, d->flown);
    }

    return done;
}

static float ComputeProgress(void) {
    float total = 0, cur = 0;
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        float L = PathLen(&D[i]);
        if (L < 1e-4f) { total += 1; cur += 1; continue; }
        total += 1.0f;
        cur   += D[i].flown / L;
    }
    return total > 0 ? cur / total : 0;
}

void Upd(float dt) {
    if (!play || pause) return;

    int done = AdvanceDrones(dt);
    prog = ComputeProgress();

    if (done >= N) {
        play = 0;
        Msg("Show finished!");
    }
}

static float Clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void FormTargetCircle(Pt out[MAX_DRONES]) {
    float cx = GROUND / 2;
    float cy = GROUND / 2;
    float R  = 3.0f + N * 0.6f;
    if (R > GROUND / 2 - 1) R = GROUND / 2 - 1;
    float h  = 5.0f;

    for (int i = 0; i < N; i++) {
        float a = (float)i / N * 2.0f * PI;
        out[i] = (Pt){
            Clampf(cx + cosf(a) * R, 0.5f, GROUND - 0.5f),
            Clampf(cy + sinf(a) * R, 0.5f, GROUND - 0.5f),
            h
        };
    }
}

static void FormTargetLine(Pt out[MAX_DRONES]) {
    float spacing = (N <= 1) ? 0 : (GROUND - 2) / (N - 1);
    float h = 5.0f, y = GROUND / 2;

    for (int i = 0; i < N; i++)
        out[i] = (Pt){ Clampf(1.0f + i * spacing, 0.5f, GROUND - 0.5f), y, h };
}

static void FormTargetGrid(Pt out[MAX_DRONES]) {
    int cols = (int)ceilf(sqrtf((float)N));
    int rows = (N + cols - 1) / cols;
    float h  = 5.0f;
    float gx = GROUND / (cols + 1);
    float gy = GROUND / (rows + 1);

    for (int i = 0; i < N; i++) {
        int c = i % cols, r = i / cols;
        out[i] = (Pt){ gx * (c + 1), gy * (r + 1), h };
    }
}

static void FormTarget(int type, Pt out[MAX_DRONES]) {
    if (type == 0)      FormTargetCircle(out);
    else if (type == 1) FormTargetLine(out);
    else                FormTargetGrid(out);
}

static void FormReset(const char* name) {
    for (int i = 0; i < N; i++) {
        D[i].flown = 0;
        D[i].fin   = 0;
        D[i].ci    = 0;
    }
    Msg("Formation: %s (%d drones)", name, N);
}

void FormCircle(void) {
    if (N <= 0) return;
    FormReset("Circle");
    Pt t[MAX_DRONES];
    FormTarget(0, t);
    for (int i = 0; i < N; i++) {
        D[i].start = t[i];
        D[i].pos   = t[i];
        D[i].h     = 5.0f;
    }
}

void FormLine(void) {
    if (N <= 0) return;
    FormReset("Line");
    Pt t[MAX_DRONES];
    FormTarget(1, t);
    for (int i = 0; i < N; i++) {
        D[i].start = t[i];
        D[i].pos   = t[i];
        D[i].h     = 5.0f;
    }
}

void FormGrid(void) {
    if (N <= 0) return;
    FormReset("Grid");
    Pt t[MAX_DRONES];
    FormTarget(2, t);
    for (int i = 0; i < N; i++) {
        D[i].start = t[i];
        D[i].pos   = t[i];
        D[i].h     = 5.0f;
    }
}

void FormTransition(int type) {
    if (N <= 0) return;
    Pt t[MAX_DRONES];
    FormTarget(type, t);

    int added = 0;
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        if (D[i].wc >= MAX_WP) continue;
        D[i].wp[D[i].wc].p = t[i];
        D[i].wc++;
        added++;
    }

    const char* name = type == 0 ? "Circle" : type == 1 ? "Line" : "Grid";
    Msg("Fly to %s: %d waypoints added", name, added);
}

void MakeDemo(void) {
    N = 0;
    S = -1;

    int count = 12;
    for (int i = 0; i < count; i++) {
        snprintf(sx, sizeof(sx), "%.0f", 5.0f + i);
        snprintf(sy, sizeof(sy), "20");
        snprintf(sz, sizeof(sz), "5");
        ic = i % 8;
        MakeDrone();
    }

    FormCircle();

    Light lights[] = {L_ON, L_BLINK, L_PULSE, L_CHASE, L_RAINBOW};
    for (int i = 0; i < N; i++)
        D[i].light = lights[i % 5];

    FormTransition(1);
    FormTransition(2);
    Rst();

    Msg("Demo loaded: %d drones - go to Show and Play", N);
}

static void OffsetDroneX(Drone* d, float dx) {
    d->start.x += dx;
    for (int w = 0; w < d->wc; w++)
        d->wp[w].p.x += dx;
}

void DuplicateDrone(int i) {
    if (i < 0 || i >= N || !D[i].act) return;
    if (N >= MAX_DRONES) { Msg("Max %d drones!", MAX_DRONES); return; }

    if (S >= 0) D[S].sel = 0;

    Drone* d = &D[N];
    *d = D[i];

    OffsetDroneX(d, 1.0f);

    snprintf(d->name, MAX_NAME, "D-%d", N + 1);
    d->ph = (float)N;
    d->sel = 0;

    N++;
    S = N - 1;
    d->sel = 1;

    Msg("Duplicated -> %s", d->name);
    PrintDrone(d);
}
