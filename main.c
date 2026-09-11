#include "common.h"
#include "utils.h"
#include "drone.h"
#include "render.h"
#include "ui.h"
#include "input.h"
#include "json.h"

static void CamInit(float* dist, float* yaw, float* pitch) {

    Cam.position   = (Vector3){55, 48, 70};
    Cam.target     = (Vector3){20, 0, 15};
    Cam.up         = (Vector3){0, 1, 0};
    Cam.fovy       = 50;
    Cam.projection = CAMERA_PERSPECTIVE;

    Vector3 off0 = Vector3Subtract(Cam.position, Cam.target);
    *dist  = Vector3Length(off0);
    *yaw   = atan2f(off0.x, off0.z);
    *pitch = asinf(off0.y / *dist);
}

static void CamRecompute(float dist, float yaw, float pitch) {
    float cy = cosf(pitch);
    Vector3 off = {
        dist * cy * sinf(yaw),
        dist * sinf(pitch),
        dist * cy * cosf(yaw)
    };
    Cam.position = Vector3Add(Cam.target, off);
}

static void CamZoom(float* dist) {
    float wh = GetMouseWheelMove();
    if (wh != 0) {
        if (wh > 0) *dist *= 0.9f;
        else        *dist *= 1.1f;
        if (*dist < CAM_DIST_MIN)  *dist = CAM_DIST_MIN;
        if (*dist > CAM_DIST_MAX)  *dist = CAM_DIST_MAX;
    }
}

static void CamRotate(float* yaw, float* pitch) {
    if (!IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) return;

    Vector2 d = GetMouseDelta();
    *yaw   -= d.x * 0.01f;
    *pitch += d.y * 0.01f;
    if (*pitch >  CAM_PITCH_MAX) *pitch =  CAM_PITCH_MAX;
    if (*pitch < -CAM_PITCH_MAX) *pitch = -CAM_PITCH_MAX;
}

static void CamHandleInput(float* dist, float* yaw, float* pitch) {
    CamZoom(dist);
    CamRotate(yaw, pitch);
}

static void DrawHelpPanel(void) {
    int hy = GetScreenHeight() - 100;
    DrawRectangle(6, hy, 210, 96, Fade(BLACK, 0.7f));

    DrawText("Help", 12, hy + 4, 13, Bl);
    DrawText("F1=Setup F2=Edit F3=Show", 12, hy + 20, 11, Gr);
    DrawText("Tab=Next  1/2/3=Light",    12, hy + 34, 11, Gr);
    DrawText("Click=Select  Enter=Move", 12, hy + 48, 11, Gr);
    DrawText("Scroll=Zoom  R-Drag=Rotate", 12, hy + 62, 11, Gr);
}

static void DrawStatusBar(void) {
    int hy = GetScreenHeight() - 100;

    DrawText(TextFormat("Mode:%s  Drones:%d",
        M == M_INTRO ? "Intro" :
        M == M_SETUP ? "Setup" :
        M == M_EDIT  ? "Edit"  : "Show",
        N),
        12, hy + 78, 11, Ye);
}

static void DrawHelpOverlay(void) {
    DrawHelpPanel();
    DrawStatusBar();

    if (mt > 0)
        DrawText(msg, GetScreenWidth() / 2 - 150, 6, 13, Gn);

    DrawText(TextFormat("FPS:%d", GetFPS()),
             GetScreenWidth() - PW - 50, 6, 11, Gr);
}

int main(void) {

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("=== Drone Light Show Simulator ===\n");
    printf("Flow: Setup -> Edit -> Show   (F1/F2/F3)\n\n");

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Drone Light Show");
    SetTargetFPS(60);

    float camDist, camYaw, camPitch;
    CamInit(&camDist, &camYaw, &camPitch);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        Keys();

        CamHandleInput(&camDist, &camYaw, &camPitch);

        CamRecompute(camDist, camYaw, camPitch);

        if (M == M_INTRO) {
            Update(dt);
            BeginDrawing();
            ClearBackground(Bg);
            DrawStartScreen();
            EndDrawing();
            continue;
        }

        int pk = Pick();
        if (pk >= 0) {
            if (S >= 0) D[S].sel = 0;
            S = pk;
            D[S].sel = 1;
            if (M == M_SETUP) M = M_EDIT;
            PrintDrone(&D[S]);
        }

        Update(dt);
        BeginDrawing();
        ClearBackground(Bg);
        Draw3D();
        DrawUI();

        DrawHelpOverlay();

        DrawAlert();

        EndDrawing();
    }

    if (SaveShow("show.json"))
        printf("Show saved (Ctrl+L to load)\n");

    CloseWindow();
    return 0;
}
