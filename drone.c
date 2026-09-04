/******************************************************************************
 *  drone.c  -  无人机操作 + 回放系统实现
 *
 *  包含：
 *    无人机管理：MakeDrone（创建）、DelDrone（删除）
 *    3D外观：RC（计算渲染颜色）、DD（3D绘制）
 *    回放：Rst（重置）、Upd（更新动画）
 *
 *  【给初学者】
 *   这里操作全局变量 D[]（无人机数组）、N（数量）、S（选中索引）等。
 *   这些变量在 common.c 中定义，在 common.h 中用 extern 声明。
 ******************************************************************************/
#include "drone.h"      // 自己的头文件
#include "common.h"     // 全局变量：D, N, S, M, sx, sy, sz, ic, LC, LCN 等
#include "utils.h"      // Msg() 函数、Hsv2Rgb() 颜色转换
#include "safety.h"     // InAirspace, SetAlert（创建时起点越界弹窗）
#include "trajectory.h" // PathLen, DronePosAt（回放插值）
#include "undo.h"       // UndoPush（变更前记录快照）

/* ================================================================
 *  MakeDrone() - 创建一架新无人机
 *
 *  从全局表单 sx, sy, sz（字符串）读取坐标，用 atof() 转成浮点数，
 *  然后在 D[N] 位置初始化一架新无人机并自动选中。
 * ================================================================ */
void MakeDrone(void) {
    /* 检查数量上限 */
    if (N >= MAX_DRONES) {                  // 已达最大数量
        Msg("Max %d drones!", MAX_DRONES);
        return;
    }

    /* 将字符串坐标转为浮点数（atof = ASCII to Float） */
    float px = (float)atof(sx);             // "3.5" → 3.5
    float py = (float)atof(sy);
    float pz = (float)atof(sz);

    /* 限制高度范围：最低0.5米，最高30米 */
    if (py < 0.5f) py = 0.5f;
    if (py > 30)   py = 30;

    /* 起点越界检查：X/Z 必须在 0~GROUND 内（Y 已在上方钳制），越界则弹窗并放弃创建 */
    Pt sp = (Pt){px, py, pz};               // 起始位置（复合字面量）
    if (!InAirspace(sp)) {
        SetAlert("Start out of range (%.1f, %.1f, %.1f)", px, py, pz);
        return;
    }

    UndoPush();                             // 记录快照（真正变更之前）

    Drone* d = &D[N];                       // 取第N个槽位的指针
    memset(d, 0, sizeof(Drone));            // 全部内存清零（安全初始化）
    d->act   = 1;                           // 激活
    d->light = L_ON;                        // 默认常亮
    d->color = ic;                          // 使用用户选的颜色
    d->bon   = 1;                           // 闪烁初始为亮
    d->h     = py;                          // 保存高度
    d->espeed = 1.0f;                       // 灯光效果速度倍率默认1
    d->ph     = (float)N;                   // 效果相位 = 序号（追逐灯用）

    d->start = sp;                          // 设置起始位置
    d->pos   = d->start;                    // 当前位置=起始位置

    snprintf(d->name, MAX_NAME, "D-%d", N + 1); // 生成名称 "D-1", "D-2"...
    N++;                                    // 总数+1
    S = N - 1;                              // 自动选中新创建的

    Msg("Created %s (%.0f,%.0f,%.0f) [%s]",
        d->name, px, py, pz, LCN[ic]);
}

/* ================================================================
 *  DelDrone() - 删除第i架无人机
 *
 *  用"前移覆盖"法：把后面的所有元素依次往前挪一位。
 * ================================================================ */
void DelDrone(int i) {
    if (i < 0 || i >= N) return;            // 索引越界检查

    UndoPush();                             // 记录快照

    /* 从位置i开始，每个元素用后一个覆盖 */
    for (int j = i; j < N - 1; j++)
        D[j] = D[j + 1];

    N--;                                    // 总数减一

    /* 如果选中的无人机被删了，调整选中索引 */
    if (S >= N) S = N - 1;
}

/* ================================================================
 *  ColorMul() - 把颜色按系数 k（0~1）变暗
 *
 *  用于呼吸/追逐等效果：把基础色按亮度比例缩放。
 * ================================================================ */
static Color ColorMul(Color c, float k) {
    int r = (int)(c.r * k); if (r > 255) r = 255;
    int g = (int)(c.g * k); if (g > 255) g = 255;
    int b = (int)(c.b * k); if (b > 255) b = 255;
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, c.a };
}

/* ================================================================
 *  RC() - 获取无人机的3D渲染颜色（Render Color）
 *
 *  根据灯光模式（灭/亮/闪烁/呼吸/追逐/彩虹）和选中状态决定显示颜色。
 *  选中的无人机会加亮30%。
 * ================================================================ */
Color RC(Drone* d) {
    Color b;                                // 声明返回颜色

    switch (d->light) {                     // 根据灯光模式
        case L_OFF:
            b = (Color){50, 50, 60, 255};   // 灯灭→暗灰色
            break;
        case L_ON:
            b = LC[d->color];               // 常亮→使用设定的颜色
            break;
        case L_BLINK:
            /* 闪烁：bon为true显示灯光色，false显示暗色 */
            b = d->bon ? LC[d->color] : (Color){35, 35, 45, 255};
            break;
        case L_PULSE: {
            /* 呼吸：亮度按 sin 在 0.2~1.0 之间缓慢起伏 */
            float k = 0.5f + 0.5f * sinf(d->ph);        // -1~1 → 0~1
            float t = 0.2f + 0.8f * k;                  // 映射到 0.2~1.0
            b = ColorMul(LC[d->color], t);
            break;
        }
        case L_CHASE: {
            /* 追逐：相位波形超过阈值时点亮，否则熄灭（各机相位不同形成跑马灯） */
            float k = 0.5f + 0.5f * sinf(d->ph);
            b = (k > 0.4f) ? LC[d->color] : (Color){35, 35, 45, 255};
            break;
        }
        case L_RAINBOW: {
            /* 彩虹：色相随相位循环，Hsv2Rgb 把色相转成 RGB 颜色 */
            float h = fmodf(d->ph * 0.5f, 1.0f);        // 相位 → 色相(0~1)
            b = Hsv2Rgb(h, 1.0f, 1.0f);
            break;
        }
        default:
            b = GRAY;                       // 兜底→灰色
    }

    /* 选中的无人机颜色加亮30%（每个分量×1.3，不超过255） */
    if (d->sel) {
        b.r = (unsigned char)(b.r * 1.3f > 255 ? 255 : b.r * 1.3f);
        b.g = (unsigned char)(b.g * 1.3f > 255 ? 255 : b.g * 1.3f);
        b.b = (unsigned char)(b.b * 1.3f > 255 ? 255 : b.b * 1.3f);
    }

    return b;
}

/* ================================================================
 *  DD() - 在3D场景中绘制一架无人机（Draw Drone）
 *
 *  绘制：发光球+两层光晕+暗色核心+选中高亮环+路径点和连线。
 * ================================================================ */
void DD(Drone* d) {
    if (!d->act) return;                    // 不激活的跳过

    Pt    p = d->pos;                       // 当前位置
    Color c = RC(d);                        // 计算后的渲染颜色

    /* 主灯光球（1.3倍基准半径） */
    DrawSphere((Vector3){p.x, p.y, p.z}, DR * 1.3f, c);

    /* 第一层光晕（1.9倍半径，30%透明度） */
    DrawSphere((Vector3){p.x, p.y, p.z}, DR * 1.9f, Fade(c, 0.3f));

    /* 机身核心（0.5倍半径，深灰色） */
    DrawSphere((Vector3){p.x, p.y, p.z}, DR * 0.5f, (Color){28, 28, 36, 255});

    /* 选中高亮环（蓝色圆圈围绕无人机） */
    if (d->sel)
        DrawCircle3D((Vector3){p.x, p.y, p.z}, DR * 2.0f,
                     (Vector3){0, 1, 0}, 0, Bl);

    /* 编辑模式下：绘制黄色路径点和连线 */
    if (M == M_EDIT && d->wc > 0) {
        for (int i = 0; i < d->wc; i++) {
            /* 路径点小球（黄色） */
            DrawSphere((Vector3){d->wp[i].p.x, d->wp[i].p.y, d->wp[i].p.z},
                       DR * 0.7f, Ye);

            /* 上一个点到当前点的连线（第一个路径点的"上一个"是起点） */
            Pt pr = (i == 0) ? d->start : d->wp[i - 1].p;
            DrawLine3D((Vector3){pr.x, pr.y, pr.z},
                       (Vector3){d->wp[i].p.x, d->wp[i].p.y, d->wp[i].p.z},
                       Fade(Ye, 0.5f));
        }

        /* 平滑路径预览：按当前 pathMode 采样画出实际飞行轨迹（蓝色）
         * 直线模式 = 直线，缓动/样条模式 = 平滑曲线，
         * 让用户在编辑时就能看到回放会走的真实路径。 */
        float L = PathLen(d);                   // 总长
        int   n = 48;                           // 采样段数（越多越平滑）
        Pt    prev = d->start;
        for (int k = 1; k <= n; k++) {
            float s = L * k / n;                // 等距采样点
            Pt    p = DronePosAt(d, s, pathMode);   // 用共享函数采样位置
            DrawLine3D((Vector3){prev.x, prev.y, prev.z},
                       (Vector3){p.x, p.y, p.z}, Fade(Bl, 0.6f));
            prev = p;
        }
    }
}

/* ================================================================
 *  Rst() - 重置回放（Reset）
 *
 *  所有无人机回到起点，重置路径进度，停止播放。
 * ================================================================ */
void Rst(void) {
    for (int i = 0; i < N; i++) {           // 遍历所有无人机
        D[i].pos   = D[i].start;            // 位置→起点
        D[i].ci    = 0;                     // 路径索引→0
        D[i].fin   = 0;                     // 标记未完成
        D[i].flown = 0;                     // 已飞距离→0
    }
    play  = false;                          // 停止播放
    pause = false;                          // 取消暂停
    prog  = 0;                              // 进度归零
}

/* ================================================================
 *  Upd() - 更新回放动画（Update playback）
 *
 *  每帧调用。每架无人机沿自己的路径前进一段距离，然后用
 *  DronePosAt() 按当前平滑模式（pathMode）采样位置。
 *
 *  关键：这里不再自己算直线移动，而是把"已飞距离 flown"交给
 *  trajectory.c 的 DronePosAt()，后者统一处理直线/缓动/样条插值。
 *  安全检测（safety.c）也调用同一个函数，保证结果一致。
 *
 *  参数:
 *    dt - 帧时间间隔（秒），保证不同帧率下移动速度一致
 * ================================================================ */
void Upd(float dt) {
    if (!play || pause) return;             // 没在播放或暂停了→不更新

    int done = 0;                           // 已完成飞行的无人机计数

    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];

        /* 跳过不激活或已完成的 */
        if (!d->act || d->fin) { done++; continue; }

        /* 没有航点或总长为0 → 视为已完成 */
        float L = PathLen(d);
        if (L < 1e-4f) { d->fin = 1; done++; continue; }

        /* 沿路径前进一段距离（速度 = spd × 3，与原实现一致） */
        d->flown += spd * 3.0f * dt;

        /* 飞完全程 → 夹到终点并标记完成 */
        if (d->flown >= L) { d->flown = L; d->fin = 1; }

        /* 用共享的位置函数采样当前位置 */
        d->pos = DronePosAt(d, d->flown, pathMode);
    }

    /* 计算播放进度：每架无人机"已飞比例"的平均值（0.0 ~ 1.0） */
    float total = 0, cur = 0;
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        float L = PathLen(&D[i]);
        if (L < 1e-4f) { total += 1; cur += 1; continue; }   // 无路径算已完成
        total += 1.0f;
        cur   += D[i].flown / L;                             // 该机进度比例
    }
    prog = total > 0 ? cur / total : 0;     // 防止除零

    /* 全部完成→结束播放 */
    if (done >= N) {
        play = 0;
        Msg("Show finished!");
    }
}

/* ================================================================
 *  编队变换（Formation）
 *
 *  把"所有激活无人机"的起始位置重新排列成某种队形。
 *  只改 start/pos（无人机停在哪），保留各自航点、颜色和灯光，
 *  因此变换后整体队形改变、各自飞行轨迹保持不变。
 * ================================================================ */

/* Clampf() - 把 v 限制在 [lo, hi] 之间 */
static float Clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* FormReset() - 编队变换的公共前置：记录快照 + 重置飞行状态 */
static void FormReset(const char* name) {
    UndoPush();                             // 记录快照（可撤销）
    for (int i = 0; i < N; i++) {
        D[i].flown = 0;                     // 重置已飞距离
        D[i].fin   = 0;                     // 标记未完成
        D[i].ci    = 0;                     // 路径索引归零
    }
    Msg("Formation: %s (%d drones)", name, N);
}

/* FormCircle() - 圆形编队：所有无人机围绕中心等角分布 */
void FormCircle(void) {
    if (N <= 0) return;
    FormReset("Circle");

    float cx = GROUND / 2;                  // 圆心 X（地面中央）
    float cz = GROUND / 2;                  // 圆心 Z
    float R  = 3.0f + N * 0.6f;             // 半径随数量增大，避免挤在一起
    if (R > GROUND / 2 - 1) R = GROUND / 2 - 1;  // 保证圆不超出地面
    float h  = 5.0f;                        // 统一飞行高度

    for (int i = 0; i < N; i++) {
        float a = (float)i / N * 2.0f * PI; // 等角分布（0~2π）
        Pt p = {
            Clampf(cx + cosf(a) * R, 0.5f, GROUND - 0.5f),
            h,
            Clampf(cz + sinf(a) * R, 0.5f, GROUND - 0.5f)
        };
        D[i].start = p;
        D[i].pos   = p;
        D[i].h     = h;
    }
}

/* FormLine() - 直线编队：沿 X 轴等距排开 */
void FormLine(void) {
    if (N <= 0) return;
    FormReset("Line");

    float spacing = (N <= 1) ? 0 : (GROUND - 2) / (N - 1);   // 间距
    float h = 5.0f;
    float z = GROUND / 2;                   // Z 固定在中央

    for (int i = 0; i < N; i++) {
        Pt p = { Clampf(1.0f + i * spacing, 0.5f, GROUND - 0.5f), h, z };
        D[i].start = p;
        D[i].pos   = p;
        D[i].h     = h;
    }
}

/* FormGrid() - 网格编队：按接近正方形的栅格排列 */
void FormGrid(void) {
    if (N <= 0) return;
    FormReset("Grid");

    int cols = (int)ceilf(sqrtf((float)N));     // 列数 ≈ √N
    int rows = (N + cols - 1) / cols;           // 行数（向上取整）
    float h  = 5.0f;
    float gx = GROUND / (cols + 1);             // X 方向格子间距
    float gz = GROUND / (rows + 1);             // Z 方向格子间距

    for (int i = 0; i < N; i++) {
        int c = i % cols;                       // 第几列
        int r = i / cols;                       // 第几行
        Pt p = { gx * (c + 1), h, gz * (r + 1) };
        D[i].start = p;
        D[i].pos   = p;
        D[i].h     = h;
    }
}

/* ================================================================
 *  DuplicateDrone() - 复制第 i 架无人机（含航点），新机整体偏移
 *
 *  复制出的新机与原机颜色、灯光、航点轨迹完全一致，
 *  但整条路径沿 X 轴偏移 1 米，避免两机完全重叠。
 * ================================================================ */
void DuplicateDrone(int i) {
    if (i < 0 || i >= N || !D[i].act) return;  // 索引无效或未激活
    if (N >= MAX_DRONES) { Msg("Max %d drones!", MAX_DRONES); return; }

    UndoPush();                                 // 记录快照
    if (S >= 0) D[S].sel = 0;                   // 取消旧选中

    Drone* d = &D[N];                           // 新机放在数组末尾
    *d = D[i];                                  // 整架复制（含航点）

    /* 整条路径沿 X 偏移 1 米，避免与原机重叠 */
    d->start.x += 1.0f;
    for (int w = 0; w < d->wc; w++)
        d->wp[w].p.x += 1.0f;

    snprintf(d->name, MAX_NAME, "D-%d", N + 1); // 重新命名
    d->ph = (float)N;                           // 追逐效果相位 = 新序号
    d->sel = 0;

    N++;                                        // 总数 +1
    S = N - 1;                                  // 选中新机
    d->sel = 1;

    Msg("Duplicated -> %s", d->name);
}

/* ================================================================
 *  MirrorPath() - 把第 i 架无人机的路径沿 X 轴中线镜像
 *
 *  镜像公式：x' = 2×中线 - x，中线取地面中央 GROUND/2。
 *  用于快速制作左右对称的编队（只改 X，Y/Z 不变）。
 * ================================================================ */
void MirrorPath(int i) {
    if (i < 0 || i >= N || !D[i].act) return;

    UndoPush();                                 // 记录快照
    Drone* d = &D[i];
    float ax = GROUND / 2;                      // 对称轴（X 中线）

    d->start.x = 2 * ax - d->start.x;           // 镜像起点
    for (int w = 0; w < d->wc; w++)
        d->wp[w].p.x = 2 * ax - d->wp[w].p.x;   // 镜像每个航点

    Msg("Mirrored %s across X", d->name);
}
