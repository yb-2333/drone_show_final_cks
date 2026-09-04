/******************************************************************************
 *  trajectory.c  -  轨迹数学模块实现
 *
 *  包含：
 *    缓动函数（easing）：让运动先加速后减速
 *    Catmull-Rom 样条：让轨迹变成平滑曲线
 *    DronePosAt：回放与安全检测共用的位置采样函数
 *
 *  【给初学者】
 *    缓动和样条都是"插值"：已知两个端点，求中间某个位置。
 *    缓动是"时间上的插值"（0~1 的比例怎么变化），
 *    样条是"空间上的插值"（如何用曲线连接多个点）。
 ******************************************************************************/
#include "trajectory.h"
#include "common.h"

/* ================================================================
 *  缓动函数
 *
 *  数学思路：输入 u（0~1 的线性进度），输出经过曲线"整形"后的进度。
 *  例如 SmoothStep 用 u*u*(3-2u)，u=0.5 时输出仍约 0.5，但两端变化更缓。
 * ================================================================ */

/* 线性：不做任何平滑，直接返回 */
float EaseLinear(float u) {
    return u;
}

/* 二次缓入：u^2，起步慢、结尾快 */
float EaseInQuad(float u) {
    return u * u;
}

/* 二次缓出：1-(1-u)^2，起步快、结尾慢 */
float EaseOutQuad(float u) {
    return u * (2.0f - u);
}

/* 三次缓入缓出：前半段 u^3*4，后半段镜像，两端都慢、中间快 */
float EaseInOutCubic(float u) {
    if (u < 0.5f)
        return 4.0f * u * u * u;
    float t = 1.0f - u;
    return 1.0f - 4.0f * t * t * t;
}

/* 平滑阶跃（smoothstep）：最常用的缓入缓出曲线，一阶导数两端为0 */
float SmoothStep(float u) {
    return u * u * (3.0f - 2.0f * u);
}

/* 按模式分发：PM_EASED 用 SmoothStep，其余保持线性 */
float Ease(float u, int mode) {
    switch (mode) {
        case PM_EASED:  return SmoothStep(u);
        case PM_LINEAR: return EaseLinear(u);
        case PM_SPLINE: return EaseLinear(u);   // 样条模式下曲线已由 Catmull-Rom 提供
        default:        return EaseLinear(u);
    }
}

/* ================================================================
 *  Catmull-Rom 样条
 *
 *  用四个控制点 p0,p1,p2,p3 定义一段曲线，曲线从 p1 走向 p2，
 *  并且方向与 p0→p2、p1→p3 相关，因此相邻段曲线方向连续、不折角。
 * ================================================================ */

/* 对单个坐标分量做 Catmull-Rom 计算（x/y/z 三个分量公式相同） */
static float CR(float p0, float p1, float p2, float p3, float u) {
    float u2 = u * u;               // u 的平方
    float u3 = u2 * u;              // u 的立方
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * u +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * u2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * u3
    );
}

/* 三个分量分别套用上面的公式，组成三维坐标 */
Pt CatmullRom(Pt p0, Pt p1, Pt p2, Pt p3, float u) {
    return (Pt){
        CR(p0.x, p1.x, p2.x, p3.x, u),
        CR(p0.y, p1.y, p2.y, p3.y, u),
        CR(p0.z, p1.z, p2.z, p3.z, u)
    };
}

/* ================================================================
 *  Dist3() - 三维欧几里得距离
 * ================================================================ */
float Dist3(Pt a, Pt b) {
    float dx = a.x - b.x;               // X 方向差
    float dy = a.y - b.y;               // Y 方向差
    float dz = a.z - b.z;               // Z 方向差
    return sqrtf(dx * dx + dy * dy + dz * dz);  // 三维勾股定理
}

/* ================================================================
 *  PathLen() - 完整路径总长度
 *
 *  路径 = 起点 → 第1个航点 → 第2个航点 → ...，逐段累加。
 * ================================================================ */
float PathLen(const Drone* d) {
    float len = 0;                      // 累计长度
    Pt cur = d->start;                  // 从起点开始

    for (int w = 0; w < d->wc; w++) {
        Pt next = d->wp[w].p;           // 下一航点
        len += Dist3(cur, next);        // 累加这一段
        cur = next;
    }
    return len;
}

/* ================================================================
 *  PathPoint() - 取路径上第 idx 个关键点
 *
 *  把"起点 + 航点序列"统一编号：索引 0 是起点，1 是第一个航点，以此类推。
 *  越界时自动夹取到两端（用于样条取相邻控制点时避免越界）。
 * ================================================================ */
static Pt PathPoint(const Drone* d, int idx) {
    if (idx <= 0) return d->start;                  // 起点及之前都返回起点
    if (idx > d->wc) return d->wp[d->wc - 1].p;     // 末尾之后都返回最后一个航点
    return d->wp[idx - 1].p;                        // 正常：第 idx 个点 = wp[idx-1]
}

/* ================================================================
 *  DronePosAt() - 采样无人机在"已飞行距离 s"处的位置
 *
 *  这是整个项目轨迹的核心函数：回放和安全检测都调用它，
 *  所以两边对"无人机某时刻在哪"的判断永远一致。
 *
 *  流程：
 *    1. 从起点开始逐段推进，找到 s 落在哪一段。
 *    2. 算出段内比例 u（0=段起点，1=段终点）。
 *    3. 按 mode 插值：
 *       PM_LINEAR/PM_EASED -> 直线 + 缓动
 *       PM_SPLINE         -> Catmull-Rom 曲线
 * ================================================================ */
Pt DronePosAt(const Drone* d, float s, int mode) {
    /* 没有航点：无人机哪儿也不去，停在起点 */
    if (d->wc <= 0) return d->start;

    Pt  cur = d->start;                 // 当前段起点
    int seg = 0;                        // 当前段索引（0 = start→wp[0]）

    for (int w = 0; w < d->wc; w++) {
        Pt next = d->wp[w].p;           // 当前段终点
        float L = Dist3(cur, next);     // 这一段长度

        /* s 落在这段上（或已是最后一段）→ 在这里插值 */
        if (s <= L || w == d->wc - 1) {
            float u;
            if (L < 1e-4f) u = 1.0f;    // 零长度段保护（避免除0）
            else {
                u = s / L;              // 段内比例 0~1
                if (u > 1.0f) u = 1.0f;
            }

            /* 样条模式：取四个控制点，用 Catmull-Rom 得到曲线上的点 */
            if (mode == PM_SPLINE)
                return CatmullRom(PathPoint(d, seg - 1),   // 前一个点
                                  cur,                      // 段起点
                                  next,                     // 段终点
                                  PathPoint(d, seg + 2),    // 后一个点
                                  u);

            /* 直线 + 缓动模式：起点 + 方向分量 × 平滑比例 */
            float e = Ease(u, mode);
            return (Pt){ cur.x + (next.x - cur.x) * e,
                         cur.y + (next.y - cur.y) * e,
                         cur.z + (next.z - cur.z) * e };
        }

        s -= L;                         // 减去这段长度，进入下一段
        cur = next;
        seg++;
    }

    return cur;                         // 全部走完，停在最后一个航点
}
