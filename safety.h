/******************************************************************************
 *  safety.h  -  无人机安全检测模块声明
 *
 *  在表演开始前，自动分析所有无人机的飞行路径，检测两类危险：
 *    1. 碰撞风险 —— 两架无人机在飞行过程中会靠得太近
 *    2. 越界/高度违规 —— 起点或路径点飞出允许的空域
 *
 *  检测结果保存在全局数组里，供 UI 面板显示和 3D 渲染高亮使用。
 ******************************************************************************/
#ifndef SAFETY_H
#define SAFETY_H

#include "common.h"     // 需要 Drone、Pt、bool 等类型定义

/* ============================ 结果结构 ============================ */

/* 碰撞风险记录：两架无人机（a、b）全程最近距离为 dist，发生在时刻 t */
typedef struct {
    int   a;        // 无人机 A 的索引
    int   b;        // 无人机 B 的索引
    float t;        // 预计发生最近距离的时刻（秒）
    float dist;     // 全程最小距离（米）
} Collision;

/* 越界/高度违规记录：某架无人机的起点或某个路径点飞出空域 */
typedef struct {
    int   drone;    // 无人机索引
    int   wp;       // 路径点索引，-1 表示起点
    int   kind;     // 违规类型：0=X越界, 1=Y高度违规, 2=Z越界
    float val;      // 违规时的坐标值
} Violation;

/* ============================ 结果存储上限 ============================ */
#define MAX_COLLISIONS 64       // 最多记录 64 条碰撞风险
#define MAX_VIOLATIONS 128      // 最多记录 128 条越界违规

/* ============================ 结果全局变量（extern 声明） ============================ */
extern Collision collisions[MAX_COLLISIONS];   // 碰撞风险列表
extern int       nCollisions;                  // 碰撞风险数量
extern Violation violations[MAX_VIOLATIONS];   // 越界违规列表
extern int       nViolations;                  // 越界违规数量
extern bool      safetyChecked;                // 是否至少运行过一次检测

/* ============================ 函数声明 ============================ */

/* 运行完整安全检测（越界 + 碰撞），结果写入全局数组 */
void RunSafetyCheck(void);

/* 判断第 i 架无人机是否被检测标记为有问题，返回 1=有问题, 0=正常 */
int SafetyWarn(int i);

/* 判断一个三维点是否在允许的空域内，返回 1=在范围内, 0=越界 */
int InAirspace(Pt p);

/* ============================ 实时告警（弹窗） ============================ */

/* 是否正在显示实时告警弹窗 */
extern bool alertActive;

/* 实时告警内容（弹窗中显示的文字） */
extern char alertMsg[256];

/* 弹出一个安全告警弹窗（用法同 printf），供创建/编辑时即时提示越界 */
void SetAlert(const char* fmt, ...);

/* 实时检测当前所有无人机的位置：越界或碰撞则设置告警并返回1，否则返回0 */
int LiveCheck(void);

#endif  /* SAFETY_H */
