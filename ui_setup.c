#include "ui.h"
#include "common.h"
#include "utils.h"
#include "drone.h"

static void DrawCoordInput(int x, int y, const char* label, char* buf, Color c) {
    DrawText(label, x, y + 3, 14, c);
    Txt((Rectangle){x + 14, (float)y, 60, 24}, buf, 15, "");
}

static int DrawSetupPosition(int x, int w, int y) {
    DrawText("Position:", x, y, 12, Gr);
    y += 14;

    DrawCoordInput(x,       y, "X", sx, Rd);
    DrawCoordInput(x + 80,  y, "Y", sy, Gn);
    DrawCoordInput(x + 160, y, "Z", sz, Bl);
    return y + 30;
}

static void DrawColorSwatch(int x, int y, int i) {
    int row = i / 4;
    int col = i % 4;
    Rectangle cr = { x + col * 52.0f, (float)(y + row * 22), 48, 20 };

    DrawRectangleRec(cr, LC[i]);
    if (ic == i)
        DrawRectangleLinesEx(cr, 2.5f, Wh);
    else
        DrawRectangleLinesEx(cr, 1, Br);
    DrawText(LCN[i], (int)cr.x + 4, (int)cr.y + 2, 12, Wh);

    if (In(cr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ic = i;
        if (S >= 0 && S < N && D[S].act) {
            D[S].color = i;
            PrintDrone(&D[S]);
        }
    }
}

static int DrawSetupColors(int x, int w, int y) {
    DrawText("Color:", x, y, 12, Gr);
    y += 14;

    for (int i = 0; i < COLOR_COUNT; i++)
        DrawColorSwatch(x, y, i);

    return y + 2 * 22 + 6;
}

static int DrawDroneRow(int x, int w, int y, int i, Drone* d) {

    Color lc = LC[d->color];
    DrawRectangle(x + 4, (int)y + 4, 10, 10, lc);
    DrawRectangleLines(x + 4, (int)y + 4, 10, 10, Wh);

    DrawText(TextFormat("#%d %s (%.0f,%.0f,%.0f)",
        i + 1, d->name, d->start.x, d->start.y, d->start.z),
        x + 18, y + 3, 12, d->sel ? Wh : Gr);

    Rectangle row = {x, (float)y, w - 26, 20};
    if (In(row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (S >= 0) D[S].sel = 0;
        S = i;
        d->sel = 1;
        ic = d->color;
        FillCoords(d);
        PrintDrone(d);
    }

    if (Btn((Rectangle){x + w - 20, (float)y, 20, 20}, "X", Rd)) {
        DelDrone(i);
        return 1;
    }
    return 0;
}

static int DrawSetupList(int x, int w, int y) {
    DrawText(TextFormat("Drones: %d", N), x, y, 13, Ye);
    y += 16;

    for (int i = 0; i < N; i++) {
        if (DrawDroneRow(x, w, y, i, &D[i])) break;
        y += 20;
    }

    if (N == 0) {
        DrawText("No drones yet.", x, y, 12, Gr);
        y += 20;
    }
    return y;
}

static int DrawSetupFormation(int x, int w, int y) {
    Sep(x, y, w);
    y += 6;
    DrawText("Formation:", x, y, 12, Gr);
    y += 14;

    float fw = (w - 8) / 3.0f;
    if (N > 0 && Btn((Rectangle){x, (float)y, fw, 20}, "Circle", Gn)) FormCircle();
    if (N > 0 && Btn((Rectangle){x + fw + 3, (float)y, fw, 20}, "Line", Gn)) FormLine();
    if (N > 0 && Btn((Rectangle){x + 2 * (fw + 3), (float)y, fw, 20}, "Grid", Gn)) FormGrid();
    y += 24;

    Sep(x, y, w);
    y += 6;

    if (N > 0 && Btn((Rectangle){x, (float)y, w, 22}, "-> Edit Track (F2)", Bl))
        M = M_EDIT;

    return y + 28;
}

void DrawSetupPanel(int x, int w, int y) {
    DrawText("[ Setup ] Create Drones", x, y, 14, Bl);
    y += 18;

    if (Btn((Rectangle){x, (float)y, w, 24}, "Load Demo Show", Ye))
        MakeDemo();
    y += 28;

    y = DrawSetupPosition(x, w, y);

    y = DrawSetupColors(x, w, y);

    if (Btn((Rectangle){x, (float)y, w, 26}, "+ Create Drone", Gn))
        MakeDrone();
    y += 30;
    Sep(x, y, w);
    y += 6;

    y = DrawSetupList(x, w, y);

    DrawSetupFormation(x, w, y);
}
