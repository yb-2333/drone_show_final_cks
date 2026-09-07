/******************************************************************************
 *  ui_edit.c  -  Edit 模式面板（灯光 + 轨迹编辑）
 *
 *  提供编辑选中无人机的完整界面：
 *     - 灯光模式（6 种）（颜色在 Setup 界面改）
 *     - 轨迹平滑模式（缓动 / 样条，每架独立）
 *     - 航点添加 / 列表编辑（上移/下移/复制/粘贴/删除）
 *     - 复制整架无人机
 *
 *  由 ui.c 的 DrawUI() 在 M_EDIT 模式下调用。
 ******************************************************************************/
#include "ui.h"         // 自己的头文件
#include "common.h"     // 所有全局变量
#include "utils.h"      // Btn, Txt, Sep, Msg, In
#include "drone.h"      // DuplicateDrone（复制整架无人机）
#include "safety.h"     // InAirspace, SetAlert, CheckOverlap
#include "trajectory.h" // PathLen（航点累计长度显示）

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
                d->light = lvals[k];    // 切换灯光模式
                PrintDrone(d);          // 终端实时显示灯光变化
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
                Pt wp = (Pt){px, py, pz};
                /* 路径点越界检查：越界则弹窗提示，不加入 */
                if (!InAirspace(wp)) {
                    SetAlert("Waypoint out of range (%.0f, %.0f, %.0f)", px, py, pz);
                } else {
                    d->wp[d->wc].p = wp;
                    d->wc++;
                    /* 与其它机重合：CheckOverlap 已弹窗提示，这里回滚刚加入的航点，
                     * 避免留下一条会相撞的轨迹（否则关掉弹窗后它还在）。 */
                    if (CheckOverlap(S))
                        d->wc--;
                }
            } else {
                Msg("Max waypoints!");
            }
        }
        y += 26;

        /* ---- 路径点列表编辑器 ---- */
        DrawText(TextFormat("Waypoints: %d  Len: %.1fm", d->wc, PathLen(d)), x, y, 12, Gr);
        y += 15;

        /* 顶部工具：复制 + 粘贴（并排放在一起）
         * "Copy" 复制最后一个航点，"Paste" 把它追加到末尾。 */
        if (Btn((Rectangle){x, (float)y, w / 2 - 3, 20},
                "Copy", d->wc > 0 ? Bl : Bt)) {
            if (d->wc > 0) { clip = d->wp[d->wc - 1]; clipSet = true; Msg("Copied last waypoint"); }
        }
        if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 20},
                "Paste", clipSet ? Gn : Bt)) {
            if (clipSet && d->wc < MAX_WP) { d->wp[d->wc] = clip; d->wc++; }
        }
        y += 24;

        /* 每个航点一行：坐标 + 上移/下移/删除 */
        for (int i = 0; i < d->wc && i < 6; i++) {
            DrawText(TextFormat("#%d %.0f,%.0f,%.0f", i + 1,
                d->wp[i].p.x, d->wp[i].p.y, d->wp[i].p.z),
                x + 2, y + 2, 11, Wh);

            float bx = x + w - 58;                  // 右侧按钮区起点（3个按钮）
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
                break;                              // 数组已前移，跳出循环
            }
            y += 20;
        }

        if (d->wc > 6)
            DrawText("... more ...", x + 4, y, 11, Gr);

        /* ---- 轨迹平滑模式（只影响选中这架无人机，每架独立） ----
         * Eased=直线+缓动（加速→减速）  Spline=平滑曲线 */
        DrawText("Path style:", x, y, 12, Gr);
        y += 14;
        float pmw = (w - 8) / 2.0f;
        if (Btn((Rectangle){x, (float)y, pmw, 20}, "Eased", d->pm == PM_EASED ? Gn : Bt)) d->pm = PM_EASED;
        if (Btn((Rectangle){x + pmw + 3, (float)y, pmw, 20}, "Spline", d->pm == PM_SPLINE ? Gn : Bt)) d->pm = PM_SPLINE;
        y += 24;

        Sep(x, y, w);
        y += 6;

        /* 复制整架无人机 + 返回 Setup */
        if (Btn((Rectangle){x, (float)y, w / 2 - 3, 22}, "Duplicate", Bl))
            DuplicateDrone(S);
        if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 22}, "<- Setup", Bt))
            M = M_SETUP;
        y += 26;

    } else {
        /* 无选中无人机时 */
        DrawText("No drone selected.", x, y, 12, Gr);
        y += 14;
        DrawText("Click in 3D or go Setup.", x, y, 12, Gr);
        y += 14;
        if (Btn((Rectangle){x, (float)y, w, 22}, "<- Back to Setup", Bt))
            M = M_SETUP;
        y += 26;
    }
}
