/******************************************************************************
 *  render.c  -  3D渲染模块实现
 *
 *  负责3D场景的绘制（地面、网格、坐标轴、无人机）和鼠标3D拾取。
 ******************************************************************************/
#include "render.h"     // 自己的头文件
#include "common.h"     // 全局变量：Cam, GROUND, N, D, PW, Gr, Rd, Gn, Bl
#include "drone.h"      // DD() 绘制无人机函数

/* ================================================================
 *  Draw3D() - 绘制整个3D场景
 *
 *  包括：地面平面、网格线、原点标记、三色坐标轴、所有无人机。
 *  只在第一象限绘制（X≥0, Z≥0），地面大小40×40米。
 * ================================================================ */
void Draw3D(void) {
    BeginMode3D(Cam);                       // 进入3D渲染模式（使用全局相机）

    /* ---- 地面 ---- */
    float S = GROUND;                       // S = 40米
    DrawPlane((Vector3){S / 2, -0.01f, S / 2},  // 平面中心在第一象限中央
              (Vector2){S, S},                   // 40×40米
              (Color){35, 40, 50, 90});          // 半透明深色

    /* ---- 地面网格（20×20格子） ---- */
    for (int i = 0; i <= 20; i++) {
        float v = i * S / 20;               // 网格线位置（0, 2, 4, ..., 40）

        /* X方向网格线 */
        DrawLine3D((Vector3){v, 0, 0}, (Vector3){v, 0, S}, Fade(Gr, 0.25f));
        /* Z方向网格线 */
        DrawLine3D((Vector3){0, 0, v}, (Vector3){S, 0, v}, Fade(Gr, 0.25f));
    }

    /* ---- 原点标记 ---- */
    DrawSphere((Vector3){0, 0.02f, 0}, 0.25f, Rd);    // 红色小圆球

    /* ---- 三色坐标轴 ---- */
    /* X、Z 轴平行于地面网格；Y 轴垂直于网格（向上，即高度方向） */
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){S, 0, 0}, Rd);  // X轴=红色（平行网格）
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, S, 0}, Bl);  // Y轴=蓝色（垂直网格）
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, 0, S}, Gn);  // Z轴=绿色（平行网格）

    /* ---- 绘制所有无人机 ---- */
    for (int i = 0; i < N; i++)
        DD(&D[i]);                          // 委托 drone.c 的 DD 函数

    EndMode3D();                            // 退出3D模式

    /* ---- 坐标轴标签（大写 X / Y / Z） ---- */
    /* 用 GetWorldToScreen 把3D轴端点投影到2D屏幕坐标，再在屏幕上方画文字标签 */
    float L = S + 2.0f;                     // 标签位置：略超出轴末端（避免与线重叠）
    Vector2 ptx = GetWorldToScreen((Vector3){L, 0, 0}, Cam);   // X轴末端的屏幕位置
    Vector2 pty = GetWorldToScreen((Vector3){0, L, 0}, Cam);   // Y轴末端的屏幕位置
    Vector2 ptz = GetWorldToScreen((Vector3){0, 0, L}, Cam);   // Z轴末端的屏幕位置
    DrawText("X", (int)ptx.x, (int)ptx.y, 16, Rd);   // X标签（红色，与X轴同色）
    DrawText("Y", (int)pty.x, (int)pty.y, 16, Bl);   // Y标签（蓝色，与Y轴同色）
    DrawText("Z", (int)ptz.x, (int)ptz.y, 16, Gn);   // Z标签（绿色，与Z轴同色）
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
    Vector2 m = GetMousePosition();         // 鼠标屏幕坐标

    /* 鼠标在右侧UI面板上 → 不拾取（防止点UI误选3D物体） */
    if (m.x > GetScreenWidth() - PW)
        return -1;

    /* 只在鼠标左键按下时检测 */
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return -1;

    /* 从屏幕坐标发射射线 */
    Ray r = GetMouseRay(m, Cam);

    float b = 3.5f;                         // 记录最近距离
    int   h = -1;                           // 记录最近无人机索引

    /* 遍历所有无人机，检测射线碰撞 */
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;            // 跳过不存在的

        /* 射线与无人机包围球碰撞检测（球半径=DR×4，放大方便点击） */
        RayCollision rc = GetRayCollisionSphere(r,
            (Vector3){D[i].pos.x, D[i].pos.y, D[i].pos.z},
            DR * 4);

        /* 碰到了且距离比之前更近 → 更新最近记录 */
        if (rc.hit && rc.distance < b) {
            b = rc.distance;
            h = i;
        }
    }

    return h;                               // 返回索引或-1
}
