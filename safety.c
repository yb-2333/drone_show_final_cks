/******************************************************************************
 *  safety.c  -  无人机安全检测模块实现
 *
 *  提供两类安全功能：
 *    1. 静态检查（编辑时）：
 *       - InAirspace()   判断坐标是否越界
 *       - CheckOverlap() 判断不同无人机起点/终点是否重合（会相撞）
 *    2. 实时告警（播放时）：
 *       - LiveCheck()    每帧检查当前位置是否越界/相撞
 *       - SetAlert()     弹出告警弹窗
 *
 *  【给初学者】
 *   安全间距 SAFE_DIST = 0.8 米：两架无人机距离小于它，就认为会相撞。
 ******************************************************************************/
#include "safety.h"     // 自己的头文件
#include "common.h"     // 全局变量：D, N 等
#include "trajectory.h" // Dist3（三维距离）

/* ============================ 安全参数常量 ============================ */
#define SAFE_DIST   0.8f    // 安全间距（米）：两架距离小于它就有碰撞风险
#define AIR_X_MIN   0.0f    // 空域 X 下界（米）
#define AIR_X_MAX   GROUND  // 空域 X 上界（40米，与地面网格一致）
#define AIR_Y_MIN   0.0f    // 空域 Y 下界（水平前后）
#define AIR_Y_MAX   GROUND  // 空域 Y 上界
#define AIR_Z_MIN   0.0f    // 最低飞行高度（米）——Z 轴向上
#define AIR_Z_MAX   GROUND  // 最高飞行高度（米），40×40×40 立方空域

/* ============================ 实时告警全局变量 ============================ */
bool alertActive = false;           // 是否正在显示告警弹窗
char alertMsg[256] = "";            // 告警弹窗文字

/* ================================================================
 *  InAirspace() - 判断一个三维点是否在允许的空域内
 *
 *  空域范围：X/Y 在 0~GROUND（40米，水平面），Z 在 0.5~30 米（高度）。
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
 *  DroneEnd() - 取一架无人机的终点坐标
 *
 *  终点 = 最后一个航点；如果没有航点，终点就等于起点。
 * ================================================================ */
static Pt DroneEnd(const Drone* d) {
    if (d->wc > 0) return d->wp[d->wc - 1].p;   // 有航点→最后一个航点
    return d->start;                            // 无航点→起点
}

/* ================================================================
 *  CheckOverlap() - 检查第 i 架无人机的起点/终点是否与其他机重合
 *
 *  课程要求"不同无人机起点和终点不能重复，不然就相撞"。
 *  这里把某架无人机的起点、终点分别与其他每架比较，距离小于
 *  SAFE_DIST 就弹窗提示（终点=最后一个航点，没有航点则等于起点）。
 *
 *  返回 1=发现重合（已弹窗）, 0=正常。
 * ================================================================ */
int CheckOverlap(int i) {
    if (i < 0 || i >= N || !D[i].act) return 0;

    Pt s1 = D[i].start;                                 // 本机起点
    Pt e1 = DroneEnd(&D[i]);                            // 本机终点

    for (int j = 0; j < N; j++) {
        if (j == i || !D[j].act) continue;              // 跳过自己和不存在

        Pt s2 = D[j].start;
        Pt e2 = DroneEnd(&D[j]);

        if (Dist3(s1, s2) < SAFE_DIST) {                // 起点重合
            SetAlert("%s and %s start too close (%.2f m) - will collide!",
                     D[i].name, D[j].name, Dist3(s1, s2));
            return 1;
        }
        if (Dist3(e1, e2) < SAFE_DIST) {                // 终点重合
            SetAlert("%s and %s end too close (%.2f m) - will collide!",
                     D[i].name, D[j].name, Dist3(e1, e2));
            return 1;
        }
    }
    return 0;
}

/* ================================================================
 *  LiveCheck() - 实时安全检测（播放过程中每帧调用）
 *
 *  检查无人机"当前时刻"的实际位置。发现越界或碰撞就设置告警弹窗
 *  并返回 1。返回 0 = 一切正常。
 * ================================================================ */
/* LiveCheckBounds() - 检查所有无人机当前位置是否越界。
 * 发现越界就填写告警并返回 1，否则返回 0。 */
static int LiveCheckBounds(void) {
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;

        Pt p = d->pos;                      // 当前实际位置
        if (p.x < AIR_X_MIN || p.x > AIR_X_MAX ||
            p.y < AIR_Y_MIN || p.y > AIR_Y_MAX ||
            p.z < AIR_Z_MIN || p.z > AIR_Z_MAX) {
            snprintf(alertMsg, sizeof(alertMsg),
                     "%s out of bounds (%.0f, %.0f, %.0f)",
                     d->name, p.x, p.y, p.z);
            alertActive = true;
            return 1;
        }
    }
    return 0;
}

/* LiveCheckCollide() - 检查两两无人机是否过近（会相撞）。
 * 发现过近就填写告警并返回 1，否则返回 0。 */
static int LiveCheckCollide(void) {
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        for (int j = i + 1; j < N; j++) {
            if (!D[j].act) continue;
            float d = Dist3(D[i].pos, D[j].pos);
            if (d < SAFE_DIST) {
                snprintf(alertMsg, sizeof(alertMsg),
                         "%s too close to %s (%.2f m)",
                         D[i].name, D[j].name, d);
                alertActive = true;
                return 1;
            }
        }
    }
    return 0;
}

/* LiveCheck() - 实时安全检测：先查越界，再查碰撞，任一异常即告警 */
int LiveCheck(void) {
    if (LiveCheckBounds()) return 1;    // 越界 → 告警
    return LiveCheckCollide();          // 碰撞 → 告警
}
