/******************************************************************************
 *  trajectory.h  -  轨迹数学模块声明
 *
 *  提供轨迹插值的公共函数：缓动、Catmull-Rom 样条、路径长度、位置采样。
 *  回放（drone.c 的 Upd）和安全检测（safety.c 的 RunSafetyCheck）都调用
 *  这里的 DronePosAt()，保证两边用完全相同的插值，避免结果不一致。
 ******************************************************************************/
#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include "common.h"     // 需要 Drone、Pt 等类型定义

/* ==================== 缓动函数 ==================== */
/*
 * 缓动（easing）：把 0~1 的线性比例 u，重新映射成 0~1 的"平滑比例"。
 * 无人机在每段起点加速、段终点减速，运动看起来更自然。
 * 所有函数满足：输入 0 返回 0，输入 1 返回 1。
 */

float EaseLinear(float u);      // 线性：不做平滑（u 原样返回）
float EaseInQuad(float u);      // 二次缓入：先慢后快
float EaseOutQuad(float u);     // 二次缓出：先快后慢
float EaseInOutCubic(float u);  // 三次缓入缓出：两端慢、中间快
float SmoothStep(float u);      // 平滑阶跃：最常用的缓入缓出曲线

/* 按 PathMode 分发的缓动入口（PM_EASED 用 SmoothStep，其余线性） */
float Ease(float u, int mode);

/* ==================== Catmull-Rom 样条 ==================== */

/* Catmull-Rom 样条插值：p0~p3 四个控制点，u 是 p1→p2 段内的比例(0~1) */
Pt CatmullRom(Pt p0, Pt p1, Pt p2, Pt p3, float u);

/* ==================== 路径长度 ==================== */

/* 两个三维点之间的欧几里得距离 */
float Dist3(Pt a, Pt b);

/* 计算某架无人机完整路径（起点→各航点）的总长度（米） */
float PathLen(const Drone* d);

/* ==================== 位置采样（核心） ==================== */

/*
 * DronePosAt() - 返回无人机在"已飞行距离 s"处的位置。
 *
 * 参数:
 *   d    - 无人机指针
 *   s    - 从起点算起的已飞行距离（米）
 *   mode - 平滑模式（PM_LINEAR / PM_EASED / PM_SPLINE）
 *
 * 原理：沿路径逐段推进，找到 s 落在哪一段，再按 mode 在该段内插值：
 *   PM_LINEAR -> 直线匀速
 *   PM_EASED  -> 直线 + 缓动（加速→减速）
 *   PM_SPLINE -> Catmull-Rom 曲线（穿过航点且方向连续）
 */
Pt DronePosAt(const Drone* d, float s, int mode);

#endif  /* TRAJECTORY_H */
