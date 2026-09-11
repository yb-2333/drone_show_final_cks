#include "ui.h"
#include "common.h"
#include "utils.h"
#include "drone.h"
#include "stats.h"

static int DrawShowHeader(int x, int w, int y) {
    DrawText("[ Show ] Playback", x, y, 14, Ye);
    y += 18;

    int h = 0;
    for (int i = 0; i < N; i++)
        if (D[i].wc > 0) h++;

    DrawText(TextFormat("Ready: %d/%d drones", h, N), x, y, 12, Gr);
    return y + 17;
}

static int DrawShowTransport(int x, int w, int y) {

    spd = Sld((Rectangle){x, (float)y, (float)w, 22}, spd, 0.5f, 8, "Speed: %.1fx");
    y += 28;

    float pw = (w - 8) / 3.0f;

    if (Btn((Rectangle){x, (float)y, pw, 26}, "Play", Gn)) {
        if (!play) { Rst(); play = 1; pause = 0; }
        else       pause = 0;
    }

    if (Btn((Rectangle){x + pw + 3, (float)y, pw, 26}, "Pause", Ye))
        pause = 1;

    if (Btn((Rectangle){x + 2 * (pw + 3), (float)y, pw, 26}, "Stop", Rd))
        Rst();
    y += 32;

    DrawRectangle(x, y, w, 10, (Color){40, 40, 55, 255});
    DrawRectangle(x, y, (int)(w * prog), 10, Ye);
    DrawText(TextFormat("%.0f%%", prog * 100), x, y + 14, 12, Gr);
    y += 26;

    DrawText("Space=Play/Pause  Esc=Stop", x, y, 11, Gr);
    return y + 15;
}

static int DrawShowControls(int x, int w, int y) {
    y = DrawShowHeader(x, w, y);
    y = DrawShowTransport(x, w, y);
    return y;
}

static void DrawShowStats(int x, int w, int y) {
    Sep(x, y, w);
    y += 6;
    DrawText("Statistics:", x, y, 12, Bl);
    y += 14;

    Stats st = ComputeStats();
    DrawText(TextFormat("Drones: %d   Waypoints: %d",
        st.drones, st.waypoints), x, y, 11, Wh);
    y += 13;
    DrawText(TextFormat("Total path: %.1f m", st.totalLen), x, y, 11, Gr);
    y += 13;
    DrawText(TextFormat("Max / avg: %.1f / %.1f m", st.maxLen, st.avgLen),
        x, y, 11, Gr);
    y += 13;
    DrawText(TextFormat("Est. duration: %.1f s", st.duration), x, y, 11, Gr);
    y += 13;

    if (play)
        DrawText(TextFormat("Elapsed: %.1f s", prog * st.duration), x, y, 11, Ye);
    else
        DrawText("Elapsed: --", x, y, 11, Gr);
    y += 13;

    DrawText(TextFormat("Bounds X: %.0f..%.0f  Y: %.0f..%.0f",
        st.bmin.x, st.bmax.x, st.bmin.y, st.bmax.y), x, y, 11, Gr);
}

void DrawShowPanel(int x, int w, int y) {
    y = DrawShowControls(x, w, y);
    DrawShowStats(x, w, y);
}
