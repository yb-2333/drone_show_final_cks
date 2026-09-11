#include "ui.h"
#include "common.h"
#include "utils.h"
#include "drone.h"
#include "safety.h"
#include "trajectory.h"

static Waypoint clip;
static bool     clipSet = false;

static int DrawEditLights(int x, int w, int y, Drone* d) {
    DrawText(TextFormat("Selected: %s", d->name), x, y, 13, Wh);
    y += 17;

    float lw = (w - 8) / 3.0f;
    const char* lnames[6] = {"OFF", "ON", "Blink", "Pulse", "Chase", "Rainbow"};
    Light  lvals[6] = {L_OFF, L_ON, L_BLINK, L_PULSE, L_CHASE, L_RAINBOW};
    Color  lcols[6] = {(Color){100,100,110,255}, Gn, Ye, Bl, Rd,
                       (Color){200, 80, 220, 255}};

    for (int k = 0; k < 6; k++) {
        int r = k / 3, c = k % 3;
        Rectangle br = { x + c * (lw + 3), (float)(y + r * 26), lw, 22 };
        if (Btn(br, lnames[k], d->light == lvals[k] ? lcols[k] : Bt)) {
            d->light = lvals[k];
            PrintDrone(d);
        }
    }
    return y + 2 * 26 + 4;
}

static void TryAddWaypoint(Drone* d) {
    if (d->wc < MAX_WP) {
        float px = (float)atof(wx);
        float py = (float)atof(wy);
        float pz = (float)atof(wz);
        Pt wp = (Pt){px, py, pz};

        if (!InAirspace(wp)) {
            SetAlert("Waypoint out of range (%.0f, %.0f, %.0f)", px, py, pz);
        } else {
            d->wp[d->wc].p = wp;
            d->wc++;

            if (CheckOverlap(S))
                d->wc--;
        }
    } else {
        Msg("Max waypoints!");
    }
}

static int DrawEditAddWaypoint(int x, int w, int y, Drone* d) {
    DrawText("Add Waypoint:", x, y, 12, Gr);
    y += 14;

    DrawText("X", x,         y + 3, 14, Rd);
    Txt((Rectangle){x + 14,  (float)y, 60, 24}, wx, 15, "");
    DrawText("Y", x + 80,    y + 3, 14, Gn);
    Txt((Rectangle){x + 94,  (float)y, 60, 24}, wy, 15, "");
    DrawText("Z", x + 160,   y + 3, 14, Bl);
    Txt((Rectangle){x + 174, (float)y, 60, 24}, wz, 15, "");
    y += 30;

    if (Btn((Rectangle){x, (float)y, w, 24}, "+ Add Waypoint", Bl))
        TryAddWaypoint(d);

    return y + 26;
}

static int DrawWpTools(int x, int w, int y, Drone* d) {
    DrawText(TextFormat("Waypoints: %d  Len: %.1fm", d->wc, PathLen(d)), x, y, 12, Gr);
    y += 15;

    if (Btn((Rectangle){x, (float)y, w / 2 - 3, 20},
            "Copy", d->wc > 0 ? Bl : Bt)) {
        if (d->wc > 0) { clip = d->wp[d->wc - 1]; clipSet = true; Msg("Copied last waypoint"); }
    }
    if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 20},
            "Paste", clipSet ? Gn : Bt)) {
        if (clipSet && d->wc < MAX_WP) { d->wp[d->wc] = clip; d->wc++; }
    }
    return y + 24;
}

static int DrawWpRows(int x, int w, int y, Drone* d) {
    for (int i = 0; i < d->wc && i < 6; i++) {
        DrawText(TextFormat("#%d %.0f,%.0f,%.0f", i + 1,
            d->wp[i].p.x, d->wp[i].p.y, d->wp[i].p.z),
            x + 2, y + 2, 11, Wh);

        float bx = x + w - 58;
        if (Btn((Rectangle){bx,      (float)y, 18, 18}, "^", i > 0 ? Gn : Bt)) {
            if (i > 0) { Waypoint t = d->wp[i]; d->wp[i] = d->wp[i - 1]; d->wp[i - 1] = t; }
        }
        if (Btn((Rectangle){bx + 20, (float)y, 18, 18}, "v", i < d->wc - 1 ? Gn : Bt)) {
            if (i < d->wc - 1) { Waypoint t = d->wp[i]; d->wp[i] = d->wp[i + 1]; d->wp[i + 1] = t; }
        }
        if (Btn((Rectangle){bx + 40, (float)y, 18, 18}, "X", Rd)) {
            for (int j = i; j < d->wc - 1; j++)
                d->wp[j] = d->wp[j + 1];
            d->wc--;
            break;
        }
        y += 20;
    }

    if (d->wc > 6)
        DrawText("... more ...", x + 4, y, 11, Gr);

    return y;
}

static int DrawEditWaypointList(int x, int w, int y, Drone* d) {
    y = DrawWpTools(x, w, y, d);
    y = DrawWpRows(x, w, y, d);
    return y;
}

static void DrawEditPathStyle(int x, int w, int y, Drone* d) {

    DrawText("Path style:", x, y, 12, Gr);
    y += 14;
    float pmw = (w - 8) / 2.0f;
    if (Btn((Rectangle){x, (float)y, pmw, 20}, "Eased", d->pm == PM_EASED ? Gn : Bt)) d->pm = PM_EASED;
    if (Btn((Rectangle){x + pmw + 3, (float)y, pmw, 20}, "Spline", d->pm == PM_SPLINE ? Gn : Bt)) d->pm = PM_SPLINE;
    y += 24;

    Sep(x, y, w);
    y += 6;

    if (Btn((Rectangle){x, (float)y, w / 2 - 3, 22}, "Duplicate", Bl))
        DuplicateDrone(S);
    if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 22}, "<- Setup", Bt))
        M = M_SETUP;
}

void DrawEditPanel(int x, int w, int y) {
    DrawText("[ Edit ] Light & Trajectory", x, y, 14, Gn);
    y += 18;

    if (S >= 0 && S < N && D[S].act) {
        Drone* d = &D[S];

        y = DrawEditLights(x, w, y, d);

        Sep(x, y, w);
        y += 6;

        y = DrawEditAddWaypoint(x, w, y, d);

        y = DrawEditWaypointList(x, w, y, d);

        DrawEditPathStyle(x, w, y, d);

    } else {

        DrawText("No drone selected.", x, y, 12, Gr);
        y += 14;
        DrawText("Click in 3D or go Setup.", x, y, 12, Gr);
        y += 14;
        if (Btn((Rectangle){x, (float)y, w, 22}, "<- Back to Setup", Bt))
            M = M_SETUP;
        y += 26;
    }
}
