#include "render.h"
#include "common.h"
#include "drone.h"
#include "safety.h"

static void DrawGround(void) {
    float S = GROUND;
    DrawPlane((Vector3){S / 2, -0.01f, S / 2},
              (Vector2){S, S},
              (Color){35, 40, 50, 90});
}

static void DrawOrigin(void) {
    DrawSphere((Vector3){0, 0.02f, 0}, 0.25f, Rd);
}

static void DrawGroundGrid(void) {
    float S = GROUND;

    for (int i = 0; i <= GRID_LINES; i++) {
        float v = i * S / GRID_LINES;

        DrawLine3D((Vector3){v, 0, 0}, (Vector3){v, 0, S}, Fade(Gr, 0.25f));

        DrawLine3D((Vector3){0, 0, v}, (Vector3){S, 0, v}, Fade(Gr, 0.25f));
    }

    DrawOrigin();
}

static void DrawAxes(void) {
    float S = GROUND;

    DrawLine3D((Vector3){0, 0, 0}, (Vector3){S, 0, 0}, Rd);
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, 0, S}, Gn);
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, S, 0}, Bl);
}

static void DrawBoxBottom(float S, float y0, Color bc) {
    DrawLine3D((Vector3){0, y0, 0}, (Vector3){S, y0, 0}, bc);
    DrawLine3D((Vector3){S, y0, 0}, (Vector3){S, y0, S}, bc);
    DrawLine3D((Vector3){S, y0, S}, (Vector3){0, y0, S}, bc);
    DrawLine3D((Vector3){0, y0, S}, (Vector3){0, y0, 0}, bc);
}

static void DrawBoxTop(float S, float y1, Color bc) {
    DrawLine3D((Vector3){0, y1, 0}, (Vector3){S, y1, 0}, bc);
    DrawLine3D((Vector3){S, y1, 0}, (Vector3){S, y1, S}, bc);
    DrawLine3D((Vector3){S, y1, S}, (Vector3){0, y1, S}, bc);
    DrawLine3D((Vector3){0, y1, S}, (Vector3){0, y1, 0}, bc);
}

static void DrawBoxEdges(float S, float y0, float y1, Color bc) {
    DrawLine3D((Vector3){0, y0, 0}, (Vector3){0, y1, 0}, bc);
    DrawLine3D((Vector3){S, y0, 0}, (Vector3){S, y1, 0}, bc);
    DrawLine3D((Vector3){S, y0, S}, (Vector3){S, y1, S}, bc);
    DrawLine3D((Vector3){0, y0, S}, (Vector3){0, y1, S}, bc);
}

static void DrawBoundaryBox(void) {
    float S  = GROUND;
    float y0 = 0.0f, y1 = 40.0f;
    Color bc = Fade(Ye, 0.35f);

    DrawBoxBottom(S, y0, bc);
    DrawBoxTop(S, y1, bc);
    DrawBoxEdges(S, y0, y1, bc);
}

void Draw3D(void) {
    BeginMode3D(Cam);

    DrawGround();
    DrawGroundGrid();
    DrawAxes();
    DrawBoundaryBox();

    for (int i = 0; i < N; i++)
        DD(&D[i]);

    EndMode3D();
}

static int PickRaycast(Ray r) {
    float b = 3.5f;
    int   h = -1;

    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;

        RayCollision rc = GetRayCollisionSphere(r,
            (Vector3){D[i].pos.x, D[i].pos.z, D[i].pos.y},
            DR * 4);

        if (rc.hit && rc.distance < b) {
            b = rc.distance;
            h = i;
        }
    }

    return h;
}

int Pick(void) {
    if (alertActive) return -1;

    Vector2 m = GetMousePosition();

    if (m.x > GetScreenWidth() - PW)
        return -1;

    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return -1;

    Ray r = GetMouseRay(m, Cam);
    return PickRaycast(r);
}

static float RayGroundT(Ray r, float y) {
    if (fabsf(r.direction.y) < 1e-6f) return -1.0f;

    float t = (y - r.position.y) / r.direction.y;
    if (t < 0) t = 0;
    return t;
}

Pt MouseGround(float y) {
    Ray r = GetMouseRay(GetMousePosition(), Cam);

    float t = RayGroundT(r, y);
    if (t < 0)
        return (Pt){GROUND / 2, y, GROUND / 2};

    Vector3 p = {
        r.position.x + r.direction.x * t,
        y,
        r.position.z + r.direction.z * t
    };
    return (Pt){p.x, p.z, p.y};
}
