/******************************************************************************
 *  ui_show.c  -  Show 模式面板（播放控制）
 *
 *  提供回放的完整控制界面：
 *     - 速度滑块 / 轨迹平滑模式切换
 *     - 播放 / 暂停 / 停止 + 进度条
 *     - 演出统计面板（数量、路径长、时长、包围盒）
 *
 *  由 ui.c 的 DrawUI() 在 M_SHOW 模式下调用。
 ******************************************************************************/
#include "ui.h"         // 自己的头文件
#include "common.h"     // 所有全局变量
#include "utils.h"      // Btn, Sld, Sep
#include "drone.h"      // Rst（重置/停止回放）
#include "stats.h"      // ComputeStats（演出统计）

/* ================================================================
 *  DrawShowPanel() - 绘制 Show 模式面板
 *
 *  参数 x/w/y 由 DrawUI 传入（面板内容区坐标）。
 * ================================================================ */
void DrawShowPanel(int x, int w, int y) {
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
