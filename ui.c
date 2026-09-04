/******************************************************************************
 *  ui.c  -  UI 面板框架（欢迎界面 + 面板骨架 + 告警弹窗）
 *
 *  这里只负责 UI 的"框架"：
 *    DrawStartScreen() - 初始欢迎界面
 *    DrawUI()          - 右侧面板骨架（背景、标题、模式标签栏），
 *                        再按当前模式分派到具体面板
 *    DrawAlert()       - 实时安全告警弹窗
 *
 *  三种模式的面板内容分别在：
 *    ui_setup.c（Setup）、ui_edit.c（Edit）、ui_show.c（Show）。
 ******************************************************************************/
#include "ui.h"         // 自己的头文件
#include "common.h"     // 所有全局变量
#include "utils.h"      // Btn, Sep
#include "drone.h"      // Rst（切到 Show / 关弹窗时重置回放）
#include "safety.h"     // alertActive, alertMsg（告警弹窗数据）

/* ---- UI面板的坐标宏（只在 ui.c 内使用） ---- */
/* PX = 面板左边界X, X = 内容区起始X, W = 内容区宽度 */
#define PX (GetScreenWidth() - PW)
#define X  (PX + 10)
#define W  (PW - 20)

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
 *  先画面板骨架（背景、标题、模式标签栏），再根据当前模式 M，
 *  把内容区的坐标 (x, w, y) 交给对应的面板函数去画具体内容。
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

    /* ---- 按当前模式分派到对应的面板绘制函数 ---- */
    if (M == M_SETUP)         DrawSetupPanel(x, w, y);
    else if (M == M_EDIT)     DrawEditPanel(x, w, y);
    else if (M == M_SHOW)     DrawShowPanel(x, w, y);
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
