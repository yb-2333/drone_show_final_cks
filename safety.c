/******************************************************************************
 *  safety.c  -  无人机安全检测模块实现
 *
 *  包含两类检测：
 *    1. 越界/高度检测 —— 检查每架无人机的起点和路径点是否在空域内
 *    2. 碰撞检测     —— 检查两架无人机是否"终点重合"（飞往同一终点）
 *
 *  【给初学者】
 *   碰撞检测的思路很简单：只要两架无人机最终的"终点"（最后一个路径点）
 *   是同一个位置，就说明它们会撞在一起。起点重合（从同一处起飞）和
 *   途中路径交叉都不算危险，因此不用复杂的飞行模拟。
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

/* ============================ 结果全局变量定义 ============================ */
Collision collisions[MAX_COLLISIONS];   // 碰撞风险列表
int       nCollisions = 0;              // 碰撞风险数量
Violation violations[MAX_VIOLATIONS];   // 越界违规列表
int       nViolations = 0;              // 越界违规数量
bool      safetyChecked = false;        // 是否运行过检测

bool      alertActive = false;          // 是否正在显示实时告警弹窗
char      alertMsg[256] = "";           // 实时告警弹窗文字

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
 *  FinalPos() - 获取第 i 架无人机的终点（最后一个路径点）
 *
 *  只有"飞出去"（wc>0）的无人机才有终点；没有路径点的无人机只是
 *  停在起点，不算有终点。用 out 指针带回终点坐标。
 *  返回 1=有终点, 0=没有终点（无路径）。
 * ================================================================ */
static int FinalPos(int i, Pt* out) {
    Drone* d = &D[i];
    if (d->wc <= 0) return 0;               // 没有路径点 → 没有终点
    *out = d->wp[d->wc - 1].p;              // 最后一个路径点 = 终点
    return 1;
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
 *  RunSafetyCheck() - 运行完整安全检测
 *
 *  流程：
 *    1. 清空上次的结果
 *    2. 越界/高度检测：遍历每架的起点和所有路径点
 *    3. 碰撞检测：检查两两终点是否重合
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

    /* ---- 2. 碰撞检测（终点重合） ---- */
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        Pt fi;
        if (!FinalPos(i, &fi)) continue;    // 没有终点（无路径）→跳过

        for (int j = i + 1; j < N; j++) {
            if (!D[j].act) continue;
            Pt fj;
            if (!FinalPos(j, &fj)) continue;

            /* 两个终点距离 < 安全间距 → 记为碰撞风险 */
            float d = Dist(fi, fj);
            if (d < SAFE_DIST && nCollisions < MAX_COLLISIONS)
                collisions[nCollisions++] = (Collision){ i, j, 0, d };
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

    /* ---- 2. 碰撞检测（终点重合） ---- */
    /* 只有两架都飞到终点（fin）且都有路径（wc>0）时才检查：
     * 起点重合、途中交叉都不算危险，只有"终点一样"才告警。 */
    for (int i = 0; i < N; i++) {
        if (!D[i].act || !D[i].fin || D[i].wc <= 0) continue;
        for (int j = i + 1; j < N; j++) {
            if (!D[j].act || !D[j].fin || D[j].wc <= 0) continue;
            float d = Dist(D[i].pos, D[j].pos);
            if (d < SAFE_DIST) {
                snprintf(alertMsg, sizeof(alertMsg),
                         "%s and %s share same destination (%.2fm)",
                         D[i].name, D[j].name, d);
                alertActive = true;
                return 1;
            }
        }
    }

    return 0;
}
