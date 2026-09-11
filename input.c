#include "input.h"
#include "common.h"
#include "drone.h"
#include "safety.h"
#include "json.h"
#include "utils.h"
#include "render.h"

static void KeysTab(void) {
    if (IsKeyPressed(KEY_TAB) && N > 0) {
        if (S >= 0) D[S].sel = 0;
        S = (S + 1) % N;
        D[S].sel = 1;
        PrintDrone(&D[S]);
    }
}

static void KeysSetup(void) {

    if (IsKeyPressed(KEY_ENTER) && M == M_SETUP && S >= 0)
        ApplySetupCoords();

    if (IsKeyPressed(KEY_A) && M == M_SETUP && !txtFocus)
        MakeDrone();

    if (IsKeyPressed(KEY_DELETE) && S >= 0)
        DelDrone(S);
}

static void KeysSwitchLight(Drone* d) {
    if (IsKeyPressed(KEY_ONE))   { d->light = L_OFF;   PrintDrone(d); }
    if (IsKeyPressed(KEY_TWO))   { d->light = L_ON;    PrintDrone(d); }
    if (IsKeyPressed(KEY_THREE)) { d->light = L_BLINK; PrintDrone(d); }
}

static void KeysNudge(Drone* d) {
    float st = IsKeyDown(KEY_LEFT_SHIFT) ? 2 : 0.5f;
    float ft = GetFrameTime() * 20;

    if (IsKeyDown(KEY_UP))    d->pos.y -= st * ft;
    if (IsKeyDown(KEY_DOWN))  d->pos.y += st * ft;
    if (IsKeyDown(KEY_LEFT))  d->pos.x -= st * ft;
    if (IsKeyDown(KEY_RIGHT)) d->pos.x += st * ft;
}

static void KeysLightMove(void) {
    if (S >= 0 && S < N && !txtFocus) {
        Drone* d = &D[S];
        KeysSwitchLight(d);
        KeysNudge(d);
    }
}

static void KeysIntro(void) {
    if (M == M_INTRO) {
        for (int k = 0; k < 512; k++) {
            if (IsKeyPressed(k)) {
                M = M_SETUP;
                break;
            }
        }
    }
}

static void KeysShow(void) {
    if (M == M_SHOW) {
        if (IsKeyPressed(KEY_SPACE)) {
            if (!play) { Rst(); play = 1; }
            else pause = !pause;
        }
        if (IsKeyPressed(KEY_ESCAPE))
            Rst();
    }
}

static void KeysFocus(void) {
    if (IsKeyPressed(KEY_F) && S >= 0)
        Cam.target = (Vector3){D[S].pos.x, D[S].pos.z, D[S].pos.y};
}

static void KeysDrag(void) {
    bool over3D = GetMousePosition().x <= GetScreenWidth() - PW;
    bool down = M == M_EDIT && S >= 0 && over3D && !alertActive &&
                IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    if (down) {
        Pt g = MouseGround(D[S].pos.z);
        if (g.x < 0)          g.x = 0;
        if (g.x > GROUND)     g.x = GROUND;
        if (g.y < 0)          g.y = 0;
        if (g.y > GROUND)     g.y = GROUND;
        D[S].pos.x   = g.x;
        D[S].pos.y   = g.y;
        D[S].start.x = g.x;
        D[S].start.y = g.y;
    }
}

static void KeysMode(void) {
    if (IsKeyPressed(KEY_F1)) M = M_SETUP;
    if (IsKeyPressed(KEY_F2)) M = M_EDIT;
    if (IsKeyPressed(KEY_F3)) { M = M_SHOW; Rst(); }
}

static void KeysCtrl(void) {
    if (!txtFocus && IsKeyDown(KEY_LEFT_CONTROL)) {
        if (IsKeyPressed(KEY_S)) {
            if (SaveShow("show.json")) Msg("Saved to show.json");
            else                       Msg("Save failed!");
        }
        if (IsKeyPressed(KEY_L)) {
            if (LoadShow("show.json")) { Rst(); Msg("Loaded from show.json"); }
            else                        Msg("Load failed (no show.json?)");
        }
    }
}

void Keys(void) {
    KeysTab();
    KeysSetup();
    KeysLightMove();
    KeysIntro();
    KeysShow();
    KeysFocus();
    KeysDrag();
    KeysMode();
    KeysCtrl();
}

static void UpdateLights(float dt) {
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;

        if (d->light == L_BLINK) {
            d->bt += dt;
            if (d->bt >= 0.5f) {
                d->bt -= 0.5f;
                d->bon = !d->bon;
            }
        } else if (d->light == L_PULSE || d->light == L_CHASE ||
                   d->light == L_RAINBOW) {

            d->ph += dt * d->espeed * 2.0f;
        }
    }
}

static void UpdatePlayback(float dt) {
    if (M != M_SHOW) return;

    Upd(dt);

    if (play && !pause && LiveCheck())
        pause = true;

    if (play && !pause) {
        static float termTimer = 0;
        termTimer += dt;
        if (termTimer >= 0.5f) {
            termTimer = 0;
            printf("---- live ----\n");
            for (int i = 0; i < N; i++)
                if (D[i].act) PrintDrone(&D[i]);
        }
    }
}

void Update(float dt) {
    txtFocus = 0;

    if (mt > 0) mt -= dt;

    UpdateLights(dt);
    UpdatePlayback(dt);
}
