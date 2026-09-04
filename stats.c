/******************************************************************************
 *  stats.c  -  演出统计模块实现
 *
 *  计算整场表演的统计信息：无人机数量、航点总数、路径总长、
 *  最长/最短/平均路径、预计演出时长、以及所有无人机的包围盒。
 *
 *  【给初学者】
 *  统计函数不修改任何数据，只"读"全局无人机数组并计算汇总结果。
 *  包围盒（bounding box）是能装下所有无人机的最小长方体，常用于
 *  判断演出占用的空间范围。
 ******************************************************************************/
#include "stats.h"      // 自己的头文件
#include "common.h"     // 全局变量：D, N, spd
#include "trajectory.h" // PathLen（路径长度）

/* ================================================================
 *  BBoxAdd() - 用点 p 扩张包围盒
 *
 *  bmin/bmax 分别是最小角和最大角，调用后把 p 纳入其中。
 * ================================================================ */
static void BBoxAdd(Pt* bmin, Pt* bmax, Pt p) {
    if (p.x < bmin->x) bmin->x = p.x;   // 更新最小 X
    if (p.y < bmin->y) bmin->y = p.y;   // 更新最小 Y
    if (p.z < bmin->z) bmin->z = p.z;   // 更新最小 Z
    if (p.x > bmax->x) bmax->x = p.x;   // 更新最大 X
    if (p.y > bmax->y) bmax->y = p.y;   // 更新最大 Y
    if (p.z > bmax->z) bmax->z = p.z;   // 更新最大 Z
}

/* ================================================================
 *  ComputeStats() - 计算当前场景的统计信息
 *
 *  遍历所有激活无人机，累加各项指标。返回一个填好的 Stats。
 * ================================================================ */
Stats ComputeStats(void) {
    Stats s;
    memset(&s, 0, sizeof(s));           // 结构体清零

    s.minLen = 1e9f;                    // 最短路径初始化为极大值
    s.bmin   = (Pt){1e9f, 1e9f, 1e9f};  // 包围盒最小角初始化为极大值
    s.bmax   = (Pt){-1e9f, -1e9f, -1e9f}; // 包围盒最大角初始化为极小值

    /* 逐架统计 */
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;          // 跳过不存在的无人机

        s.drones++;                     // 计数
        s.waypoints += d->wc;           // 累加航点数

        float L = PathLen(d);           // 该机路径总长
        s.totalLen += L;                // 累加总长
        if (d->wc > 0) {                // 只有带航点的才参与最值
            if (L > s.maxLen) s.maxLen = L;
            if (L < s.minLen) s.minLen = L;
        }

        /* 包围盒：纳入起点和所有航点 */
        BBoxAdd(&s.bmin, &s.bmax, d->start);
        for (int w = 0; w < d->wc; w++)
            BBoxAdd(&s.bmin, &s.bmax, d->wp[w].p);
    }

    /* 平均路径长度（有无人机才计算） */
    if (s.drones > 0)
        s.avgLen = s.totalLen / s.drones;
    else
        s.minLen = 0;                   // 没无人机时避免显示 1e9

    /* 预计演出时长 = 最长路径 / 当前播放速度（速度 = spd × 3 米/秒） */
    float vref = spd * 3.0f;            // 当前回放线速度
    if (vref < 0.01f) vref = 0.01f;     // 防止除零
    s.duration = s.maxLen / vref;

    return s;
}
