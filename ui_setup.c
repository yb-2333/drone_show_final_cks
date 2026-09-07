/******************************************************************************
 *  ui_setup.c  -  Setup 模式面板（创建无人机）
 *
 *  提供创建无人机的完整界面：
 *     - 位置输入（X/Y/Z）
 *     - 8 色颜色选择
 *     - 创建按钮 + 已创建无人机列表（点击选中/显示坐标，×删除）
 *     - 一键编队变换（Circle / Line / Grid）
 *
 *  由 ui.c 的 DrawUI() 在 M_SETUP 模式下调用。
 ******************************************************************************/
#include "ui.h"         // 自己的头文件
#include "common.h"     // 所有全局变量
#include "utils.h"      // Btn, Txt, Sep, In
#include "drone.h"      // MakeDrone, FormCircle, FormLine, FormGrid, MakeDemo

/* ================================================================
 *  DrawSetupPanel() - 绘制 Setup 模式面板
 *
 *  参数 x/w/y 由 DrawUI 传入（面板内容区坐标）。
 * ================================================================ */
void DrawSetupPanel(int x, int w, int y) {
    DrawText("[ Setup ] Create Drones", x, y, 14, Bl);
    y += 18;

    /* 快速开始：一键加载示例表演（新手点这里就能看到完整效果） */
    if (Btn((Rectangle){x, (float)y, w, 24}, "Load Demo Show", Ye))
        MakeDemo();
    y += 28;

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
        if (In(cr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            ic = i;                                 // 记住颜色（新建无人机用）
            if (S >= 0 && S < N && D[S].act) {
                D[S].color = i;                     // 同时改选中无人机颜色
                PrintDrone(&D[S]);                  // 终端实时显示颜色变化
            }
        }
    }
    y += 2 * 22 + 6;                                // 两行高度 + 间距

    /* 创建按钮 */
    if (Btn((Rectangle){x, (float)y, w, 26}, "+ Create Drone", Gn))
        MakeDrone();
    y += 30;
    Sep(x, y, w);
    y += 6;

    /* 已创建无人机列表（点击行→选中并显示坐标，行尾×→删除） */
    DrawText(TextFormat("Drones: %d", N), x, y, 13, Ye);
    y += 16;

    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];

        /* 小色块 */
        Color lc = LC[d->color];
        DrawRectangle(x + 4, (int)y + 4, 10, 10, lc);
        DrawRectangleLines(x + 4, (int)y + 4, 10, 10, Wh);

        /* 信息文字 */
        DrawText(TextFormat("#%d %s (%.0f,%.0f,%.0f)",
            i + 1, d->name, d->start.x, d->start.y, d->start.z),
            x + 18, y + 3, 12, d->sel ? Wh : Gr);

        /* 点击行→选中 + 坐标输入框显示该机坐标（不自动切 Edit） */
        Rectangle row = {x, (float)y, w - 26, 20};
        if (In(row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (S >= 0) D[S].sel = 0;
            S = i;
            d->sel = 1;
            ic = d->color;          // 颜色选择器跳到该机当前颜色
            FillCoords(d);          // 坐标框实时显示该机坐标
            PrintDrone(d);          // 终端实时显示该机状态
        }

        /* 行尾叉号：删除这架无人机 */
        if (Btn((Rectangle){x + w - 20, (float)y, 20, 20}, "X", Rd)) {
            DelDrone(i);
            break;                  // 数组已前移，跳出循环
        }

        y += 20;
    }

    if (N == 0) {
        DrawText("No drones yet.", x, y, 12, Gr);
        y += 20;
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
    if (N > 0 && Btn((Rectangle){x, (float)y, w, 22}, "-> Edit Track (F2)", Bl))
        M = M_EDIT;
}
