/******************************************************************************
 *  render.c  -  3D渲染模块实现
 *
 *  负责3D场景的绘制（地面、网格、坐标轴、无人机）和鼠标3D拾取。
 ******************************************************************************/
#include "render.h"     // 自己的头文件
#include "common.h"     // 全局变量：Cam, GROUND, N, D, PW, Gr, Rd, Gn, Bl
#include "drone.h"      // DD() 绘制无人机函数
#include "safety.h"     // alertActive（告警弹窗显示时不拾取）

/* ================================================================
 *  DrawGround() - 绘制地面平面
 *
 *  一个半透明的深色平面，铺满第一象限（40×40米），作为场景的"地板"。
 * ================================================================ */
static void DrawGround(void) {
    float S = GROUND;                       // S = 40米
    DrawPlane((Vector3){S / 2, -0.01f, S / 2},  // 平面中心在第一象限中央
              (Vector2){S, S},                   // 40×40米
              (Color){35, 40, 50, 90});          // 半透明深色
}

/* ================================================================
 *  DrawOrigin() - 绘制原点标记（红色小圆球）
 * ================================================================ */
static void DrawOrigin(void) {
    DrawSphere((Vector3){0, 0.02f, 0}, 0.25f, Rd);    // 红色小圆球
}

/* ================================================================
 *  DrawGroundGrid() - 绘制地面网格与原点标记
 *
 *  画 20×20 的淡色网格线，帮助用户估算位置；原点处放一个红色小球。
 * ================================================================ */
static void DrawGroundGrid(void) {
    float S = GROUND;                       // S = 40米

    /* 网格线（GRID_LINES × GRID_LINES 格子） */
    for (int i = 0; i <= GRID_LINES; i++) {
        float v = i * S / GRID_LINES;       // 网格线位置（0, 2, 4, ..., 40）

        /* X方向网格线 */
        DrawLine3D((Vector3){v, 0, 0}, (Vector3){v, 0, S}, Fade(Gr, 0.25f));
        /* Z方向网格线 */
        DrawLine3D((Vector3){0, 0, v}, (Vector3){S, 0, v}, Fade(Gr, 0.25f));
    }

    DrawOrigin();                           // 原点标记
}

/* ================================================================
 *  DrawAxes() - 绘制三色坐标轴
 *
 *  Z 轴向上，X/Y 为水平面，颜色与右侧输入框一致。
 * ================================================================ */
static void DrawAxes(void) {
    float S = GROUND;                       // S = 40米

    DrawLine3D((Vector3){0, 0, 0}, (Vector3){S, 0, 0}, Rd);  // X轴=红色（水平）
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, 0, S}, Gn);  // Y轴=绿色（水平）
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, S, 0}, Bl);  // Z轴=蓝色（竖直向上）
}

/* DrawBoxBottom() - 边界框底面四边（y0 高度） */
static void DrawBoxBottom(float S, float y0, Color bc) {
    DrawLine3D((Vector3){0, y0, 0}, (Vector3){S, y0, 0}, bc);
    DrawLine3D((Vector3){S, y0, 0}, (Vector3){S, y0, S}, bc);
    DrawLine3D((Vector3){S, y0, S}, (Vector3){0, y0, S}, bc);
    DrawLine3D((Vector3){0, y0, S}, (Vector3){0, y0, 0}, bc);
}

/* DrawBoxTop() - 边界框顶面四边（y1 高度） */
static void DrawBoxTop(float S, float y1, Color bc) {
    DrawLine3D((Vector3){0, y1, 0}, (Vector3){S, y1, 0}, bc);
    DrawLine3D((Vector3){S, y1, 0}, (Vector3){S, y1, S}, bc);
    DrawLine3D((Vector3){S, y1, S}, (Vector3){0, y1, S}, bc);
    DrawLine3D((Vector3){0, y1, S}, (Vector3){0, y1, 0}, bc);
}

/* DrawBoxEdges() - 边界框四条竖边（连接底面与顶面的四个角） */
static void DrawBoxEdges(float S, float y0, float y1, Color bc) {
    DrawLine3D((Vector3){0, y0, 0}, (Vector3){0, y1, 0}, bc);
    DrawLine3D((Vector3){S, y0, 0}, (Vector3){S, y1, 0}, bc);
    DrawLine3D((Vector3){S, y0, S}, (Vector3){S, y1, S}, bc);
    DrawLine3D((Vector3){0, y0, S}, (Vector3){0, y1, S}, bc);
}

/* ================================================================
 *  DrawBoundaryBox() - 绘制空域边界框（半透明）
 *
 *  空域 = X/Y/Z 0~40 米（40×40×40 立方体）。用 12 条边画出一个长方体，
 *  让用户一眼看出无人机能飞的空间有多大。
 * ================================================================ */
static void DrawBoundaryBox(void) {
    float S  = GROUND;                      // S = 40米
    float y0 = 0.0f, y1 = 40.0f;            // 最低/最高飞行高度（0~40 米）
    Color bc = Fade(Ye, 0.35f);             // 边界框颜色（淡黄）

    DrawBoxBottom(S, y0, bc);               // 底面四边
    DrawBoxTop(S, y1, bc);                  // 顶面四边
    DrawBoxEdges(S, y0, y1, bc);            // 四条竖边
}

/* ================================================================
 *  Draw3D() - 绘制整个3D场景
 *
 *  包括：地面平面、网格线、原点标记、三色坐标轴、所有无人机。
 *  只在第一象限绘制（X≥0, Z≥0），地面大小40×40米。
 * ================================================================ */
void Draw3D(void) {
    BeginMode3D(Cam);                       // 进入3D渲染模式（使用全局相机）

    DrawGround();                           // 地面平面
    DrawGroundGrid();                       // 网格 + 原点标记
    DrawAxes();                             // 三色坐标轴
    DrawBoundaryBox();                      // 空域边界框

    /* ---- 绘制所有无人机 ---- */
    for (int i = 0; i < N; i++)
        DD(&D[i]);                          // 委托 drone.c 的 DD 函数

    EndMode3D();                            // 退出3D模式
}

/* ================================================================
 *  PickRaycast() - 让一条射线与所有无人机做碰撞检测
 *
 *  遍历所有无人机，检测射线与每架的包围球相交情况，
 *  返回距离最近的那个索引，-1 = 没碰到任何一架。
 * ================================================================ */
static int PickRaycast(Ray r) {
    float b = 3.5f;                         // 记录最近距离
    int   h = -1;                           // 记录最近无人机索引

    /* 遍历所有无人机，检测射线碰撞 */
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;            // 跳过不存在的

        /* 射线与无人机包围球碰撞检测（球半径=DR×4，放大方便点击） */
        RayCollision rc = GetRayCollisionSphere(r,
            (Vector3){D[i].pos.x, D[i].pos.z, D[i].pos.y},   // 数据Z=高度→raylib Y
            DR * 4);

        /* 碰到了且距离比之前更近 → 更新最近记录 */
        if (rc.hit && rc.distance < b) {
            b = rc.distance;
            h = i;
        }
    }

    return h;                               // 返回索引或-1
}

/* ================================================================
 *  Pick() - 鼠标3D拾取
 *
 *  从鼠标位置发出一条射线，检测射线与哪架无人机的包围球相交。
 *  返回最近的被击中的无人机索引，-1 = 没点到。
 *
 *  原理：Imagine 从你的眼睛（相机）穿过鼠标指针，射出一条无限远的线，
 *        检查这条线穿过了哪些3D物体。
 * ================================================================ */
int Pick(void) {
    if (alertActive) return -1;             // 弹窗显示时不拾取（防止点OK误选无人机）

    Vector2 m = GetMousePosition();         // 鼠标屏幕坐标

    /* 鼠标在右侧UI面板上 → 不拾取（防止点UI误选3D物体） */
    if (m.x > GetScreenWidth() - PW)
        return -1;

    /* 只在鼠标左键按下时检测 */
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return -1;

    /* 从屏幕坐标发射射线，交给 PickRaycast 做碰撞检测 */
    Ray r = GetMouseRay(m, Cam);
    return PickRaycast(r);
}

/* ================================================================
 *  MouseGround() - 鼠标射线与水平面求交
 *
 *  从相机穿过鼠标指针发射一条射线，求它与"高度 = y"的水平面的交点。
 *  返回该交点的三维坐标，用于鼠标拖拽移动无人机（在高度不变的平面上）。
 *
 *  原理：射线 = 起点 + 方向 × t，令结果的 Y 分量等于目标高度 y，
 *        反解出 t，再代回得到完整的 (x, y, z)。
 * ================================================================ */
/* RayGroundT() - 求射线与"高度 = y"水平面的交点参数 t
 * 射线方向近乎水平时返回 -1 表示无交点（由调用方兜底）。 */
static float RayGroundT(Ray r, float y) {
    if (fabsf(r.direction.y) < 1e-6f) return -1.0f;    // 与平面平行 → 无交点

    float t = (y - r.position.y) / r.direction.y;      // 反解参数 t
    if (t < 0) t = 0;                                  // 交点应在射线前方
    return t;
}

Pt MouseGround(float y) {
    Ray r = GetMouseRay(GetMousePosition(), Cam);   // 鼠标射线

    /* 射线方向近乎水平（与平面平行）时无交点，返回平面中心避免除零 */
    float t = RayGroundT(r, y);
    if (t < 0)
        return (Pt){GROUND / 2, y, GROUND / 2};

    Vector3 p = {                                    // 代入求交点
        r.position.x + r.direction.x * t,
        y,
        r.position.z + r.direction.z * t
    };
    return (Pt){p.x, p.z, p.y};                  // raylib Y(高度) → 数据 Z
}
