/******************************************************************************
 *  safety.c  -  无人机安全检测模块实现
 *
 *  包含两类检测：
 *    1. 越界/高度检测 —— 检查每架无人机的起点和路径点是否在空域内
 *    2. 碰撞检测     —— 时间同步模拟整场飞行，找任意两架的最小间距
 *
 *  【给初学者】
 *   碰撞检测的核心思路：回放时所有无人机以相同速度同时起飞，
 *   所以可以用"飞行距离 s"来统一表示时间——走过相同距离 = 同一时刻。
 *   我们让 s 从 0 增长到最长路径长度，每一步算出每架无人机的位置，
 *   再两两比较距离，记录全程最小距离。
 ******************************************************************************/
#include "safety.h"     // 自己的头文件
#include "common.h"     // 全局变量：D, N, spd, GROUND 等
#include "utils.h"      // Msg() 函数

/* ============================ 安全参数常量 ============================ */
#define SAFE_DIST   0.8f    // 安全间距（米）：两架距离小于它就有碰撞风险
#define AIR_X_MIN   0.0f    // 空域 X 下界（米）
#define AIR_X_MAX   GROUND  // 空域 X 上界（40米，与地面网格一致）
#define AIR_Z_MIN   0.0f    // 空域 Z 下界
#define AIR_Z_MAX   GROUND  // 空域 Z 上界
#define AIR_Y_MIN   0.5f    // 最低飞行高度（米）
#define AIR_Y_MAX   30.0f   // 最高飞行高度（米）
#define SIM_STEP    0.25f   // 碰撞模拟的采样步长（米），越小越精确
#define MAX_STEPS   4000    // 采样步数上限（防止路径过长导致卡顿）

/* ============================ 结果全局变量定义 ============================ */
Collision collisions[MAX_COLLISIONS];   // 碰撞风险列表
int       nCollisions = 0;              // 碰撞风险数量
Violation violations[MAX_VIOLATIONS];   // 越界违规列表
int       nViolations = 0;              // 越界违规数量
bool      safetyChecked = false;        // 是否运行过检测

/* ================================================================
 *  Dist() - 计算两个三维点之间的欧几里得距离
 * ================================================================ */
static float Dist(Pt a, Pt b) {
    float dx = a.x - b.x;                       // X 方向差
    float dy = a.y - b.y;                       // Y 方向差
    float dz = a.z - b.z;                       // Z 方向差
    return sqrtf(dx * dx + dy * dy + dz * dz);  // 三维勾股定理
}

/* ================================================================
 *  PathLen() - 计算第 i 架无人机的总路径长度
 *
 *  路径 = 起点 → 第1个路径点 → 第2个路径点 → ...，逐段累加。
 * ================================================================ */
static float PathLen(int i) {
    Drone* d = &D[i];
    float len = 0;                          // 累计长度
    Pt cur = d->start;                      // 从起点开始

    for (int w = 0; w < d->wc; w++) {
        Pt next = d->wp[w].p;               // 下一个路径点
        len += Dist(cur, next);             // 累加这一段
        cur = next;
    }
    return len;
}

/* ================================================================
 *  PosAt() - 返回第 i 架无人机在"飞行距离 s"处的位置
 *
 *  参数:
 *    s - 从起点算起的飞行距离（米）
 *
 *  原理：沿路径逐段推进，找到 s 落在哪一段上，然后按比例插值。
 *  如果 s 超过总路径长度，说明已飞完，停在最后一个路径点。
 * ================================================================ */
static Pt PosAt(int i, float s) {
    Drone* d = &D[i];
    Pt cur = d->start;                      // 当前段起点

    for (int w = 0; w < d->wc; w++) {
        Pt next = d->wp[w].p;               // 当前段终点
        float seg = Dist(cur, next);        // 这一段长度

        if (s <= seg) {                     // s 落在这段上
            if (seg < 1e-4f) return next;   // 零长度段保护（避免除0）
            float t = s / seg;              // 段内比例 0~1
            /* 线性插值：起点 + 方向分量 × 比例 */
            return (Pt){ cur.x + (next.x - cur.x) * t,
                         cur.y + (next.y - cur.y) * t,
                         cur.z + (next.z - cur.z) * t };
        }
        s -= seg;                           // 减去这段，进入下一段
        cur = next;
    }
    return cur;                             // 全部走完，停在最后
}

/* ================================================================
 *  CheckPt() - 检查一个点是否越界/高度违规，违规则记录
 *
 *  参数:
 *    i  - 无人机索引
 *    wp - 路径点索引（-1 表示起点）
 *    p  - 要检查的三维坐标
 * ================================================================ */
static void CheckPt(int i, int wp, Pt p) {
    if (nViolations >= MAX_VIOLATIONS) return;  // 记录已满

    int kind = -1;                          // 违规类型，-1 = 正常
    float val = 0;                          // 违规时的坐标值

    if (p.x < AIR_X_MIN || p.x > AIR_X_MAX)      { kind = 0; val = p.x; }  // X越界
    else if (p.y < AIR_Y_MIN || p.y > AIR_Y_MAX) { kind = 1; val = p.y; }  // 高度违规
    else if (p.z < AIR_Z_MIN || p.z > AIR_Z_MAX) { kind = 2; val = p.z; }  // Z越界

    if (kind >= 0)                          // 有违规才记录
        violations[nViolations++] = (Violation){ i, wp, kind, val };
}

/* ================================================================
 *  RunSafetyCheck() - 运行完整安全检测
 *
 *  流程：
 *    1. 清空上次的结果
 *    2. 越界/高度检测：遍历每架的起点和所有路径点
 *    3. 碰撞检测：时间同步模拟飞行，两两找最小间距
 *    4. 设置已检测标记，弹出汇总消息
 * ================================================================ */
void RunSafetyCheck(void) {
    nCollisions = 0;                        // 清空碰撞结果
    nViolations = 0;                        // 清空越界结果

    /* ---- 1. 越界/高度检测 ---- */
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;              // 跳过不存在的
        CheckPt(i, -1, d->start);           // 检查起点（wp = -1）
        for (int w = 0; w < d->wc; w++)
            CheckPt(i, w, d->wp[w].p);      // 检查每个路径点
    }

    /* ---- 2. 碰撞检测 ---- */
    /* 找到最长路径长度，作为模拟的总里程 */
    float maxS = 0;
    for (int i = 0; i < N; i++)
        if (D[i].act) {
            float L = PathLen(i);
            if (L > maxS) maxS = L;
        }

    /* 参考速度 = spd × 3（与 drone.c 的 Upd 一致），用于把里程换算成时间 */
    float vref = spd * 3.0f;
    if (vref < 0.01f) vref = 0.01f;         // 防止除0

    /* 对每一对无人机，模拟全程，记录它们的最小间距 */
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        for (int j = i + 1; j < N; j++) {
            if (!D[j].act) continue;

            float minDist = 1e9f;           // 初始化为很大，表示"还没找到"
            float minS = 0;                 // 最小距离发生时走过的里程
            int steps = 0;

            /* s 从 0 增长到 maxS，每步算一次两机距离 */
            for (float s = 0; s <= maxS && steps < MAX_STEPS; s += SIM_STEP, steps++) {
                float d = Dist(PosAt(i, s), PosAt(j, s));
                if (d < minDist) {          // 找到更近的距离
                    minDist = d;
                    minS = s;
                }
            }

            /* 全程最小距离 < 安全间距 → 记录为碰撞风险 */
            if (minDist < SAFE_DIST && nCollisions < MAX_COLLISIONS)
                collisions[nCollisions++] =
                    (Collision){ i, j, minS / vref, minDist };
        }
    }

    safetyChecked = true;                   // 标记已检测

    Msg("Safety: %d collision(s), %d violation(s)", nCollisions, nViolations);
}

/* ================================================================
 *  SafetyWarn() - 判断某架无人机是否被标记为有问题
 *
 *  在碰撞列表或越界列表里出现过的无人机都算"有问题"。
 *  返回 1=有问题, 0=正常。用于 3D 场景红色高亮。
 * ================================================================ */
int SafetyWarn(int i) {
    for (int k = 0; k < nCollisions; k++)
        if (collisions[k].a == i || collisions[k].b == i) return 1;
    for (int k = 0; k < nViolations; k++)
        if (violations[k].drone == i) return 1;
    return 0;
}
