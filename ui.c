#include "ui.h"
#include "common.h"
#include "utils.h"
#include "drone.h"
#include "safety.h"

#define PX (GetScreenWidth() - PW)
#define X  (PX + 10)
#define W  (PW - 20)

static void DrawStartTitle(int sw, int sh) {

    int tw = MeasureText("Drone Light Show", 48);
    DrawText("Drone Light Show",
             sw / 2 - tw / 2, sh / 2 - 80, 48, Ye);

    int subw = MeasureText("Drone Formation Light Show Simulator", 22);
    DrawText("Drone Formation Light Show Simulator",
             sw / 2 - subw / 2, sh / 2 - 16, 22, Wh);
}

static void DrawStartHints(int sw, int sh) {
    DrawText("Press any key to start",
             sw / 2 - 140, sh / 2 + 40, 20, Gn);
    DrawText("F1=Setup  F2=Edit  F3=Show",
             sw / 2 - 145, sh / 2 + 90, 16, Gr);
}

void DrawStartScreen(void) {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawStartTitle(sw, sh);
    DrawStartHints(sw, sh);
}

static int DrawModeTabs(int x, int w, int y) {
    const char* ms[] = {"1.Setup", "2.Edit", "3.Show"};
    const Mode  mm[] = {M_SETUP, M_EDIT, M_SHOW};
    Color       mc[] = {Bl, Gn, Ye};
    float       bw   = (w - 10) / 3.0f;

    for (int i = 0; i < 3; i++) {
        Color bg = (M == mm[i]) ? mc[i] : Bt;
        if (Btn((Rectangle){x + i * (bw + 4), (float)y, bw, 26}, ms[i], bg)) {
            M = mm[i];
            if (M == M_SHOW) Rst();
        }
    }
    y += 34;
    Sep(x, y, w);
    return y + 8;
}

void DrawUI(void) {
    int sh = GetScreenHeight();
    int px = PX;
    int x  = X;
    int w  = W;
    int y  = 8;

    DrawRectangle(px, 0, PW, sh, Pn);
    DrawLine(px, 0, px, sh, Br);

    DrawText("Drone Light Show", x, y, 16, Bl);
    y += 22;

    y = DrawModeTabs(x, w, y);

    if (M == M_SETUP)         DrawSetupPanel(x, w, y);
    else if (M == M_EDIT)     DrawEditPanel(x, w, y);
    else if (M == M_SHOW)     DrawShowPanel(x, w, y);
}

static int DrawAlertBox(Rectangle box) {
    DrawRectangleRec(box, (Color){42, 44, 60, 255});
    DrawRectangleLinesEx(box, 2, Rd);

    DrawText("Safety Warning", (int)box.x + 20, (int)box.y + 16, 20, Rd);

    Sep((int)box.x + 20, (int)box.y + 48, (int)box.width - 40);

    DrawText(alertMsg, (int)box.x + 20, (int)box.y + 60, 15, Wh);

    Rectangle ok = { box.x + box.width - 90, box.y + box.height - 40, 70, 26 };
    return Btn(ok, "OK", Gn);
}

void DrawAlert(void) {
    if (!alertActive) return;

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));

    Rectangle box = { sw / 2.0f - 210, sh / 2.0f - 100, 420, 200 };
    if (DrawAlertBox(box)) {
        alertActive = false;
        Rst();
    }
}
