/******************************************************************************
 *  ui.h  -  UI 面板模块声明
 *
 *  负责绘制所有 2D 用户界面。整体分为两层：
 *    1. 框架层（ui.c）：欢迎界面 DrawStartScreen、右侧面板骨架 DrawUI、
 *       安全告警弹窗 DrawAlert。
 *    2. 面板层：Setup / Edit / Show 三种模式的具体内容，
 *       分别由 ui_setup.c / ui_edit.c / ui_show.c 实现。
 ******************************************************************************/
#ifndef UI_H
#define UI_H

#include "common.h"

/* ==================== 框架层 ==================== */

/* 绘制右侧UI面板骨架（背景/标题/模式标签），并按当前模式分派到具体面板 */
void DrawUI(void);

/* 绘制初始欢迎界面（按任意键开始） */
void DrawStartScreen(void);

/* 绘制实时安全告警弹窗（有告警时才显示） */
void DrawAlert(void);

/* ==================== 面板层 ==================== */
/* 三个函数都由 DrawUI() 调用：x/w 是面板内容区的起点X和宽度，
 * y 是面板内容区顶部的 Y 坐标（标签栏之后）。 */

/* Setup 模式：创建无人机 / 颜色选择 / 编队变换 */
void DrawSetupPanel(int x, int w, int y);

/* Edit 模式：灯光 / 航点编辑 / 安全检测 / 撤销 / 存取 */
void DrawEditPanel(int x, int w, int y);

/* Show 模式：播放控制 / 轨迹平滑 / 演出统计 */
void DrawShowPanel(int x, int w, int y);

#endif  /* UI_H */
