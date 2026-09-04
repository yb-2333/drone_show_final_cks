/******************************************************************************
 *  safety.c  -  无人机安全检测模块实现
 *
 *  包含碰撞检测：时间同步模拟整场飞行，找任意两架的最小间距。
 *  另外提供 InAirspace（空域范围判断）和 SetAlert（告警弹窗），
 *  供创建/编辑时即时校验坐标、播放时实时告警使用。
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
#include "trajectory.h" // Dist3, PathLen, DronePosAt（与回放一致的插值）

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
bool      safetyChecked = false;        // 是否运行过检测

bool      alertActive = false;          // 是否正在显示实时告警弹窗
char      alertMsg[256] = "";           // 实时告警弹窗文字

/* ================================================================
 *  距离 / 路径长度 / 位置采样 已统一搬到 trajectory.c：
 *    Dist3()      —— 三维距离
 *    PathLen()    —— 路径总长度
 *    DronePosAt() —— 位置采样（直线 / 缓动 / 样条）
 *  这里直接引用它们，保证安全检测与回放使用完全相同的插值。
 * ================================================================ */

/* ================================================================
 *  InAirspace() - 判断一个三维点是否在允许的空域内
 *
 *  空域范围：X/Z 在 0~GROUND（40米），Y 在 0.5~30 米。
 *  返回 1=在范围内, 0=越界。供创建/编辑时即时检查坐标用。
 * ================================================================ */
int InAirspace(Pt p) {
    return p.x >= AIR_X_MIN && p.x <= AIR_X_MAX &&   // X 在界内
           p.y >= AIR_Y_MIN && p.y <= AIR_Y_MAX &&   // Y 高度在界内
           p.z >= AIR_Z_MIN && p.z <= AIR_Z_MAX;     // Z 在界内
}

/* ================================================================
 *  SetAlert() - 弹出一个安全告警弹窗
 *
 *  用法同 printf：SetAlert("起点越界 (%.1f)", x)。
 *  设置文字并打开 alertActive，交给 ui.c 的 DrawAlert() 绘制。
 * ================================================================ */
void SetAlert(const char* fmt, ...) {
    va_list a;                                  // 可变参数列表
    va_start(a, fmt);                           // 指向第一个可变参数
    vsnprintf(alertMsg, sizeof(alertMsg), fmt, a);  // 格式化写入 alertMsg
    va_end(a);                                  // 清理
    alertActive = true;                         // 打开弹窗
}

/* ================================================================
 *  RunSafetyCheck() - 运行碰撞安全检测
 *
 *  流程：
 *    1. 清空上次的结果
 *    2. 碰撞检测：时间同步模拟飞行，两两找最小间距
 *    3. 设置已检测标记，弹出汇总消息
 * ================================================================ */
void RunSafetyCheck(void) {
    nCollisions = 0;                        // 清空碰撞结果

    /* ---- 碰撞检测 ---- */
    /* 找到最长路径长度，作为模拟的总里程 */
    float maxS = 0;
    for (int i = 0; i < N; i++)
        if (D[i].act) {
            float L = PathLen(&D[i]);
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

            /* s 从 0 增长到 maxS，每步用 DronePosAt 采样两机位置算距离 */
            for (float s = 0; s <= maxS && steps < MAX_STEPS; s += SIM_STEP, steps++) {
                float d = Dist3(DronePosAt(&D[i], s, pathMode),
                                DronePosAt(&D[j], s, pathMode));
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

    Msg("Safety: %d collision(s)", nCollisions);
}

/* ================================================================
 *  SafetyWarn() - 判断某架无人机是否被标记为有碰撞风险
 *
 *  在碰撞列表里出现过的无人机都算"有风险"。
 *  返回 1=有风险, 0=正常。用于 3D 场景红色高亮。
 * ================================================================ */
int SafetyWarn(int i) {
    for (int k = 0; k < nCollisions; k++)
        if (collisions[k].a == i || collisions[k].b == i) return 1;
    return 0;
}

/* ================================================================
 *  LiveCheck() - 实时安全检测（播放过程中每帧调用）
 *
 *  与 RunSafetyCheck（静态预判）不同，这里检查的是无人机"当前时刻"
 *  的实际位置。发现越界或碰撞就设置告警弹窗并返回 1。
 *
 *  返回 1=检测到问题（弹窗已设置）, 0=一切正常。
 * ================================================================ */
int LiveCheck(void) {
    /* ---- 1. 越界检测（当前实时位置） ---- */
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;

        Pt p = d->pos;                      // 当前实际位置
        if (p.x < AIR_X_MIN || p.x > AIR_X_MAX ||
            p.y < AIR_Y_MIN || p.y > AIR_Y_MAX ||
            p.z < AIR_Z_MIN || p.z > AIR_Z_MAX) {
            snprintf(alertMsg, sizeof(alertMsg),
                     "%s out of bounds (%.1f, %.1f, %.1f)",
                     d->name, p.x, p.y, p.z);
            alertActive = true;
            return 1;
        }
    }

    /* ---- 2. 碰撞检测（当前两两距离） ---- */
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        for (int j = i + 1; j < N; j++) {
            if (!D[j].act) continue;
            float d = Dist3(D[i].pos, D[j].pos);
            if (d < SAFE_DIST) {
                snprintf(alertMsg, sizeof(alertMsg),
                         "%s too close to %s (%.2fm)",
                         D[i].name, D[j].name, d);
                alertActive = true;
                return 1;
            }
        }
    }

    return 0;
}
