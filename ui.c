/******************************************************************************
 *  ui.c  -  UI面板模块实现
 *
 *  负责绘制所有2D界面元素：
 *    DrawStartScreen() - 初始欢迎界面
 *    DrawUI()          - 右侧面板（Setup/Edit/Show三种模式）
 *
 *  这是项目中最长的函数，但结构清晰——按三种模式分成三个大块。
 ******************************************************************************/
#include "ui.h"         // 自己的头文件
#include "common.h"     // 所有全局变量
#include "utils.h"      // Btn, Txt, Sep, Sld, In
#include "drone.h"      // MakeDrone, DelDrone, Rst, FormCircle/Line/Grid
#include "safety.h"     // RunSafetyCheck, collisions（安全检测UI）
#include "undo.h"       // Undo/Redo（撤销按钮）
#include "json.h"       // SaveShow/LoadShow（保存/加载按钮）
#include "trajectory.h" // PathLen（航点累计长度显示）
#include "stats.h"      // ComputeStats（演出统计面板）
#include "test.h"       // RunSelfTest（自检按钮）

/* ---- UI面板的坐标宏（只在 ui.c 内使用） ---- */
/* PX = 面板左边界X, X = 内容区起始X, W = 内容区宽度 */
#define PX (GetScreenWidth() - PW)
#define X  (PX + 10)
#define W  (PW - 20)

/* 航点剪贴板：复制航点后暂存在这里，供"粘贴"使用 */
static Waypoint clip;           // 复制的航点
static bool     clipSet = false;   // 是否有已复制的航点

/* ================================================================
 *  DrawStartScreen() - 绘制初始欢迎界面
 *
 *  启动时显示，包含标题、说明和快捷键提示。
 *  按任意键进入 Setup 模式。
 * ================================================================ */
void DrawStartScreen(void) {
    int sw = GetScreenWidth();              // 屏幕宽度
    int sh = GetScreenHeight();             // 屏幕高度

    /* 所有文字基于屏幕中心定位（sw/2 是水平中心） */
    DrawText("Drone Light Show",
             sw / 2 - 260, sh / 2 - 80, 48, Ye);          // 主标题（黄色大字）
    DrawText("Drone Formation Light Show Simulator",
             sw / 2 - 300, sh / 2 - 16, 22, Wh);          // 副标题（白色）
    DrawText("Press any key to start",
             sw / 2 - 140, sh / 2 + 40, 20, Gn);          // 操作提示（绿色）
    DrawText("F1=Setup  F2=Edit  F3=Show",
             sw / 2 - 145, sh / 2 + 90, 16, Gr);          // 快捷键（灰色）
}

/* ================================================================
 *  DrawUI() - 绘制右侧UI面板
 *
 *  这是整个程序最长的函数。根据当前模式 M 的值显示不同的内容：
 *    M_SETUP → 创建无人机界面
 *    M_EDIT  → 编辑灯光/路径界面
 *    M_SHOW  → 播放控制界面
 * ================================================================ */
void DrawUI(void) {
    int sh = GetScreenHeight();             // 屏幕高度
    int px = PX;                            // 面板左边界
    int x  = X;                             // 内容区X
    int w  = W;                             // 内容区宽度
    int y  = 8;                             // 当前Y坐标（从上往下画）

    /* ---- 面板背景 ---- */
    DrawRectangle(px, 0, PW, sh, Pn);       // 深色半透明面板
    DrawLine(px, 0, px, sh, Br);            // 左边框线（分隔3D和UI）

    /* ---- 标题 ---- */
    DrawText("Drone Light Show", x, y, 16, Bl);
    y += 22;

    /* ---- 模式切换标签栏 ---- */
    const char* ms[] = {"1.Setup", "2.Edit", "3.Show"};    // 标签文字
    const Mode  mm[] = {M_SETUP, M_EDIT, M_SHOW};          // 对应的模式值
    Color       mc[] = {Bl, Gn, Ye};       // 标签颜色（蓝/绿/黄）
    float       bw   = (w - 10) / 3.0f;    // 每个标签宽度

    for (int i = 0; i < 3; i++) {
        Color bg = (M == mm[i]) ? mc[i] : Bt;   // 当前模式=彩色，其他=暗色
        if (Btn((Rectangle){x + i * (bw + 4), (float)y, bw, 26}, ms[i], bg)) {
            M = mm[i];                      // 切换模式
            if (M == M_SHOW) Rst();         // 进入Show模式自动重置
        }
    }
    y += 34;
    Sep(x, y, w);                           // 分隔线
    y += 8;

    /* ==================== SETUP 模式 ==================== */
    if (M == M_SETUP) {
        DrawText("[ Setup ] Create Drones", x, y, 14, Bl);
        y += 18;

        /* 位置输入 */
        DrawText("Position:", x, y, 12, Gr);
        y += 14;

        DrawText("X", x,         y + 3, 14, Rd);        // X标签（红色)
        Txt((Rectangle){x + 14,  (float)y, 60, 24}, sx, 15, "");
        DrawText("Y", x + 80,    y + 3, 14, Gn);        // Y标签（绿色）
        Txt((Rectangle){x + 94,  (float)y, 60, 24}, sy, 15, "");
        DrawText("Z", x + 160,   y + 3, 14, Bl);        // Z标签（蓝色）
        Txt((Rectangle){x + 174, (float)y, 60, 24}, sz, 15, "");
        y += 30;

        /* 颜色选择（8 色，两行四列） */
        DrawText("Color:", x, y, 12, Gr);
        y += 14;
        for (int i = 0; i < 8; i++) {
            int row = i / 4;                            // 第几行
            int col = i % 4;                            // 第几列
            Rectangle cr = { x + col * 52.0f, (float)(y + row * 22), 48, 20 };
            DrawRectangleRec(cr, LC[i]);                // 颜色块
            if (ic == i)
                DrawRectangleLinesEx(cr, 2.5f, Wh);     // 选中的加粗白边框
            else
                DrawRectangleLinesEx(cr, 1, Br);        // 未选中的普通边框
            DrawText(LCN[i], (int)cr.x + 4, (int)cr.y + 2, 12, Wh);
            if (In(cr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                ic = i;                                 // 点击切换颜色
        }
        y += 2 * 22 + 6;                                // 两行高度 + 间距

        /* 创建按钮 */
        if (Btn((Rectangle){x, (float)y, w, 26}, "+ Create Drone", Gn))
            MakeDrone();
        y += 30;
        Sep(x, y, w);
        y += 6;

        /* 已创建无人机列表 */
        DrawText(TextFormat("Drones: %d", N), x, y, 13, Ye);
        y += 16;

        for (int i = 0; i < N; i++) {
            Drone* d = &D[i];

            /* 小色块 */
            Color lc = LC[d->color];
            DrawRectangle(x + 4, (int)y + 2, 10, 10, lc);
            DrawRectangleLines(x + 4, (int)y + 2, 10, 10, Wh);

            /* 信息文字 */
            DrawText(TextFormat("#%d %s (%.0f,%.0f,%.0f)",
                i + 1, d->name, d->start.x, d->start.y, d->start.z),
                x + 18, y, 12, d->sel ? Wh : Gr);

            /* 点击行→选中并切换到Edit */
            Rectangle row = {x, (float)y, w, 15};
            if (In(row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (S >= 0) D[S].sel = 0;
                S = i;
                d->sel = 1;
                M = M_EDIT;
            }
            y += 15;
        }

        if (N == 0) {
            DrawText("No drones yet.", x, y, 12, Gr);
            y += 15;
        }

        /* ---- 编队变换（把所有无人机排成队形） ---- */
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

        /* 跳转编辑模式按钮 */
        if (N > 0 && Btn((Rectangle){x, (float)y, w, 22}, "-> Continue to Edit", Bl))
            M = M_EDIT;
    }

    /* ==================== EDIT 模式 ==================== */
    else if (M == M_EDIT) {
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

    /* ==================== SHOW 模式 ==================== */
    else if (M == M_SHOW) {
        DrawText("[ Show ] Playback", x, y, 14, Ye);
        y += 18;

        /* 统计有路径点的无人机 */
        int h = 0;
        for (int i = 0; i < N; i++)
            if (D[i].wc > 0) h++;

        DrawText(TextFormat("Ready: %d/%d drones", h, N), x, y, 12, Gr);
        y += 17;

        /* 速度滑块 */
        spd = Sld((Rectangle){x, (float)y, (float)w, 22}, spd, 0.5f, 8, "Speed: %.1fx");
        y += 28;

        /* 轨迹平滑模式选择 */
        DrawText("Path:", x, y, 12, Gr);
        y += 14;
        float pmw = (w - 8) / 3.0f;
        if (Btn((Rectangle){x, (float)y, pmw, 20}, "Linear", pathMode == PM_LINEAR ? Gn : Bt)) pathMode = PM_LINEAR;
        if (Btn((Rectangle){x + pmw + 3, (float)y, pmw, 20}, "Eased", pathMode == PM_EASED ? Gn : Bt)) pathMode = PM_EASED;
        if (Btn((Rectangle){x + 2 * (pmw + 3), (float)y, pmw, 20}, "Spline", pathMode == PM_SPLINE ? Gn : Bt)) pathMode = PM_SPLINE;
        y += 24;

        /* 播放/暂停/停止按钮 */
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

        /* 进度条 */
        DrawRectangle(x, y, w, 10, (Color){40, 40, 55, 255});            // 背景
        DrawRectangle(x, y, (int)(w * prog), 10, Ye);                    // 前景
        DrawText(TextFormat("%.0f%%", prog * 100), x, y + 14, 12, Gr);   // 百分比
        y += 26;

        /* 快捷键提示 */
        DrawText("Space=Play/Pause  Esc=Stop", x, y, 11, Gr);
        y += 15;

        /* ---- 演出统计面板 ---- */
        Sep(x, y, w);
        y += 6;
        DrawText("Statistics:", x, y, 12, Bl);
        y += 14;

        Stats st = ComputeStats();                          // 计算当前场景统计
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

        /* 播放中的已用时间 = 进度 × 预计时长；未播放显示占位符 */
        if (play)
            DrawText(TextFormat("Elapsed: %.1f s", prog * st.duration), x, y, 11, Ye);
        else
            DrawText("Elapsed: --", x, y, 11, Gr);
        y += 13;

        /* 包围盒：整场演出占用的空间范围（最小角 ~ 最大角） */
        DrawText(TextFormat("Bounds X: %.0f..%.0f  Z: %.0f..%.0f",
            st.bmin.x, st.bmax.x, st.bmin.z, st.bmax.z), x, y, 11, Gr);
    }
}

/* ================================================================
 *  DrawAlert() - 绘制实时安全告警弹窗
 *
 *  播放中检测到越界或碰撞时，弹出一个居中的模态对话框，
 *  显示问题详情。点击 OK 关闭弹窗并停止回放。
 * ================================================================ */
void DrawAlert(void) {
    if (!alertActive) return;               // 没有告警就不画

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    /* 半透明遮罩，盖住整个窗口，制造"模态"效果 */
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));

    /* 弹窗矩形（居中） */
    Rectangle box = { sw / 2.0f - 210, sh / 2.0f - 100, 420, 200 };
    DrawRectangleRec(box, (Color){42, 44, 60, 255});
    DrawRectangleLinesEx(box, 2, Rd);       // 红色边框

    /* 标题 */
    DrawText("Safety Warning", (int)box.x + 20, (int)box.y + 16, 20, Rd);

    /* 分隔线 */
    Sep((int)box.x + 20, (int)box.y + 48, (int)box.width - 40);

    /* 详情文字 */
    DrawText(alertMsg, (int)box.x + 20, (int)box.y + 60, 15, Wh);

    /* OK 按钮：关闭弹窗并停止回放 */
    Rectangle ok = { box.x + box.width - 90, box.y + box.height - 40, 70, 26 };
    if (Btn(ok, "OK", Gn)) {
        alertActive = false;                // 关闭弹窗
        Rst();                              // 停止并重置回放
    }
}
