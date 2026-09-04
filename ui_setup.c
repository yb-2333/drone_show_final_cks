/******************************************************************************
 *  ui_setup.c  -  Setup 模式面板（创建无人机）
 *
 *  提供创建无人机的完整界面：
 *     - 位置输入（X/Y/Z）
 *     - 8 色颜色选择
 *     - 创建按钮 + 已创建无人机列表（点击行进入编辑）
 *     - 一键编队变换（Circle / Line / Grid）
 *
 *  由 ui.c 的 DrawUI() 在 M_SETUP 模式下调用。
 ******************************************************************************/
#include "ui.h"         // 自己的头文件
#include "common.h"     // 所有全局变量
#include "utils.h"      // Btn, Txt, Sep, In
#include "drone.h"      // MakeDrone, FormCircle, FormLine, FormGrid

/* ================================================================
 *  DrawSetupPanel() - 绘制 Setup 模式面板
 *
 *  参数 x/w/y 由 DrawUI 传入（面板内容区坐标）。
 * ================================================================ */
void DrawSetupPanel(int x, int w, int y) {
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
