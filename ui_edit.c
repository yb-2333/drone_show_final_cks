/******************************************************************************
 *  ui_edit.c  -  Edit 模式面板（灯光 + 轨迹编辑）
 *
 *  提供编辑选中无人机的完整界面：
 *     - 灯光模式（6 种）
 *     - 航点添加 / 列表编辑（上移/下移/复制/删除/粘贴/清空）
 *     - 删除 / 复制 / 镜像无人机
 *     - 安全检测、撤销/重做、保存/加载、自检
 *
 *  由 ui.c 的 DrawUI() 在 M_EDIT 模式下调用。
 ******************************************************************************/
#include "ui.h"         // 自己的头文件
#include "common.h"     // 所有全局变量
#include "utils.h"      // Btn, Txt, Sep, Msg
#include "drone.h"      // DelDrone, DuplicateDrone, MirrorPath, Rst
#include "safety.h"     // RunSafetyCheck, InAirspace, SetAlert, collisions
#include "undo.h"       // UndoPush, Undo, Redo, UndoCan, RedoCan
#include "json.h"       // SaveShow, LoadShow
#include "trajectory.h" // PathLen（航点累计长度显示）
#include "test.h"       // RunSelfTest（自检按钮）

/* 航点剪贴板：复制航点后暂存在这里，供"粘贴"使用 */
static Waypoint clip;           // 复制的航点
static bool     clipSet = false;   // 是否有已复制的航点

/* ================================================================
 *  DrawEditPanel() - 绘制 Edit 模式面板
 *
 *  参数 x/w/y 由 DrawUI 传入（面板内容区坐标）。
 * ================================================================ */
void DrawEditPanel(int x, int w, int y) {
    DrawText("[ Edit ] Light & Trajectory", x, y, 14, Gn);
    y += 18;

    if (S >= 0 && S < N && D[S].act) {
        Drone* d = &D[S];               // 选中无人机指针

        DrawText(TextFormat("Selected: %s", d->name), x, y, 13, Wh);
        y += 17;

        /* ---- 灯光模式（6 种，两行三列） ---- */
        float lw = (w - 8) / 3.0f;
        const char* lnames[6] = {"OFF", "ON", "Blink", "Pulse", "Chase", "Rainbow"};
        Light  lvals[6] = {L_OFF, L_ON, L_BLINK, L_PULSE, L_CHASE, L_RAINBOW};
        Color  lcols[6] = {(Color){100,100,110,255}, Gn, Ye, Bl, Rd,
                           (Color){200, 80, 220, 255}};

        for (int k = 0; k < 6; k++) {
            int r = k / 3, c = k % 3;
            Rectangle br = { x + c * (lw + 3), (float)(y + r * 26), lw, 22 };
            if (Btn(br, lnames[k], d->light == lvals[k] ? lcols[k] : Bt)) {
                UndoPush();                 // 记录快照
                d->light = lvals[k];
            }
        }
        y += 2 * 26 + 4;
        Sep(x, y, w);
        y += 6;

        /* ---- 添加路径点 ---- */
        DrawText("Add Waypoint:", x, y, 12, Gr);
        y += 14;

        DrawText("X", x,         y + 3, 14, Rd);
        Txt((Rectangle){x + 14,  (float)y, 60, 24}, wx, 15, "");
        DrawText("Y", x + 80,    y + 3, 14, Gn);
        Txt((Rectangle){x + 94,  (float)y, 60, 24}, wy, 15, "");
        DrawText("Z", x + 160,   y + 3, 14, Bl);
        Txt((Rectangle){x + 174, (float)y, 60, 24}, wz, 15, "");
        y += 30;

        if (Btn((Rectangle){x, (float)y, w, 24}, "+ Add Waypoint", Bl)) {
            if (d->wc < MAX_WP) {
                float px = (float)atof(wx);
                float py = (float)atof(wy);
                float pz = (float)atof(wz);
                if (py < 0.5f) py = 0.5f;
                if (py > 30)   py = 30;
                Pt wp = (Pt){px, py, pz};
                /* 路径点越界检查：越界则弹窗提示，不加入 */
                if (!InAirspace(wp)) {
                    SetAlert("Waypoint out of range (%.1f, %.1f, %.1f)", px, py, pz);
                } else {
                    UndoPush();             // 记录快照
                    d->wp[d->wc].p = wp;
                    d->wc++;
                }
            } else {
                Msg("Max waypoints!");
            }
        }
        y += 26;

        /* ---- 路径点列表编辑器 ---- */
        DrawText(TextFormat("Waypoints: %d  Len: %.1fm", d->wc, PathLen(d)), x, y, 12, Gr);
        y += 15;

        /* 顶部工具：粘贴 + 清空 */
        if (Btn((Rectangle){x, (float)y, w / 2 - 3, 20},
                clipSet ? "Paste" : "(No copy)", clipSet ? Gn : Bt)) {
            if (clipSet && d->wc < MAX_WP) { UndoPush(); d->wp[d->wc] = clip; d->wc++; }
        }
        if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 20}, "Clear All", Rd)) {
            if (d->wc > 0) { UndoPush(); d->wc = 0; }
        }
        y += 24;

        /* 每个航点一行：坐标 + 上移/下移/复制/删除 */
        for (int i = 0; i < d->wc && i < 6; i++) {
            DrawText(TextFormat("#%d %.0f,%.0f,%.0f", i + 1,
                d->wp[i].p.x, d->wp[i].p.y, d->wp[i].p.z),
                x + 2, y + 2, 11, Wh);

            float bx = x + w - 78;                  // 右侧按钮区起点
            if (Btn((Rectangle){bx,      (float)y, 18, 18}, "^", i > 0 ? Gn : Bt)) {
                if (i > 0) { UndoPush(); Waypoint t = d->wp[i]; d->wp[i] = d->wp[i - 1]; d->wp[i - 1] = t; }
            }
            if (Btn((Rectangle){bx + 20, (float)y, 18, 18}, "v", i < d->wc - 1 ? Gn : Bt)) {
                if (i < d->wc - 1) { UndoPush(); Waypoint t = d->wp[i]; d->wp[i] = d->wp[i + 1]; d->wp[i + 1] = t; }
            }
            if (Btn((Rectangle){bx + 40, (float)y, 18, 18}, "C", Bl)) {
                clip = d->wp[i]; clipSet = true;    // 复制到剪贴板
            }
            if (Btn((Rectangle){bx + 60, (float)y, 18, 18}, "X", Rd)) {
                UndoPush();
                for (int j = i; j < d->wc - 1; j++)
                    d->wp[j] = d->wp[j + 1];
                d->wc--;
                break;                              // 数组已前移，跳出循环
            }
            y += 20;
        }

        if (d->wc > 6)
            DrawText("... more ...", x + 4, y, 11, Gr);

        Sep(x, y, w);
        y += 6;

        /* 底部按钮 */
        if (Btn((Rectangle){x, (float)y, w / 2 - 3, 22}, "Delete Drone", Rd))
            DelDrone(S);

        if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 22}, "<- Setup", Bt))
            M = M_SETUP;
        y += 26;

        /* 复制 / 镜像：复制整架无人机，或沿 X 中线镜像路径 */
        if (Btn((Rectangle){x, (float)y, w / 2 - 3, 20}, "Duplicate", Bl))
            DuplicateDrone(S);
        if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 20}, "Mirror X", Bl))
            MirrorPath(S);

    } else {
        /* 无选中无人机时 */
        DrawText("No drone selected.", x, y, 12, Gr);
        y += 14;
        DrawText("Click in 3D or go Setup.", x, y, 12, Gr);
        y += 14;
        if (Btn((Rectangle){x, (float)y, w, 22}, "<- Back to Setup", Bt))
            M = M_SETUP;
    }

    /* ---- 安全检测（针对所有无人机，与是否选中无关） ---- */
    y += 28;                            // 让出上一步按钮（高22）的高度 + 间距
    Sep(x, y, w);
    y += 6;

    if (Btn((Rectangle){x, (float)y, w, 22}, "Safety Check", Gn))
        RunSafetyCheck();               // 点击运行检测
    y += 26;

    if (safetyChecked) {                // 至少运行过一次才显示结果
        if (nCollisions == 0) {
            DrawText("All safe - no collisions", x, y, 12, Gn);
            y += 14;
        } else {
            DrawText(TextFormat("Collision risk: %d", nCollisions), x, y, 12, Rd);
            y += 15;

            int shown = 0;
            /* 碰撞风险列表（最多显示4条） */
            for (int i = 0; i < nCollisions && shown < 4; i++) {
                Collision* c = &collisions[i];
                if (c->a >= N || c->b >= N) continue;   // 无人机已删除，跳过
                DrawText(TextFormat("%s x %s @%.1fs (%.2fm)",
                        D[c->a].name, D[c->b].name, c->t, c->dist),
                        x + 4, y, 11, Rd);
                y += 13;
                shown++;
            }
            if (nCollisions > shown)
                DrawText("... more ...", x + 4, y, 11, Gr);
        }
    }

    /* ---- 工具条：撤销 / 重做 / 保存 / 加载 ---- */
    y += 6;
    Sep(x, y, w);
    y += 6;

    if (Btn((Rectangle){x, (float)y, w / 2 - 3, 20}, "Undo", UndoCan() ? Gn : Bt)) Undo();
    if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 20}, "Redo", RedoCan() ? Gn : Bt)) Redo();
    y += 24;

    if (Btn((Rectangle){x, (float)y, w / 2 - 3, 20}, "Save", Bl)) {
        if (SaveShow("show.json")) Msg("Saved to show.json");
        else                       Msg("Save failed!");
    }
    if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 20}, "Load", Bl)) {
        if (LoadShow("show.json")) { Rst(); Msg("Loaded from show.json"); }
        else                        Msg("Load failed!");
    }
    y += 24;

    /* 自检按钮：跑一遍核心算法断言，验证数学函数没被改坏 */
    if (Btn((Rectangle){x, (float)y, w, 20}, "Self-Test", Bl))
        RunSelfTest();
    y += 24;
}
