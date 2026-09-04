/******************************************************************************
 *  stats.h  -  演出统计模块声明
 *
 *  计算整场表演的一些统计信息：无人机数量、航点总数、路径总长、
 *  最长/最短/平均路径、预计演出时长、以及所有无人机的包围盒。
 *  结果供 UI 面板显示。
 ******************************************************************************/
#ifndef STATS_H
#define STATS_H

#include "common.h"

/* 统计结果结构 */
typedef struct {
    int   drones;        // 激活无人机数量
    int   waypoints;     // 航点总数
    float totalLen;      // 所有无人机路径总长度（米）
    float maxLen;        // 最长单机路径
    float minLen;        // 最短单机路径（仅统计有航点的）
    float avgLen;        // 平均路径长度
    float duration;      // 预计演出时长（秒，按当前速度）
    Pt    bmin;          // 包围盒最小角
    Pt    bmax;          // 包围盒最大角
} Stats;

/* 计算并返回当前场景的统计信息 */
Stats ComputeStats(void);

#endif  /* STATS_H */
