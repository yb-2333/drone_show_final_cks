/******************************************************************************
 *  utils.h  -  工具函数模块声明
 *
 *  提供UI交互的基础组件：按钮、文字输入框、滑块等。
 *  这些函数不直接操作无人机数据，只负责"画"和"交互检测"。
 ******************************************************************************/
#ifndef UTILS_H
#define UTILS_H

#include "common.h"     // 需要 raylib 类型（Color, Rectangle, Vector2）

/* ---- 消息 ---- */
/* 在屏幕顶部显示一条状态消息，约2.5秒后自动消失。用法和 printf 一样 */
void Msg(const char* f, ...);

/* ---- 鼠标检测 ---- */
/* 判断鼠标光标是否在指定矩形内，返回1=在里面, 0=不在 */
int In(Rectangle r);

/* ---- 按钮 ---- */
/* 画一个按钮（背景+边框+居中文字），并检测是否被点击 */
int Btn(Rectangle r, const char* t, Color c);

/* ---- 文字输入框 ---- */
/* 绘制带标签的输入框，支持键盘打字、退格、负号、小数点、闪烁光标 */
int Txt(Rectangle r, char* buf, int max, const char* label);

/* ---- 分隔线 ---- */
/* 在指定位置画一条水平分隔线 */
void Sep(int x, int y, int w);

/* ---- 滑块 ---- */
/* 绘制滑块控件，用户拖动滑块可调整数值 */
float Sld(Rectangle r, float v, float lo, float hi, const char* f);

/* ---- 颜色工具 ---- */
/* HSV 色相转 RGB 颜色（h/s/v 都在 0~1 范围），用于彩虹效果 */
Color Hsv2Rgb(float h, float s, float v);

#endif  /* UTILS_H */
