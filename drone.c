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
#include "utils.h"      // Msg() 函数

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

    Drone* d = &D[N];                       // 取第N个槽位的指针
    memset(d, 0, sizeof(Drone));            // 全部内存清零（安全初始化）
    d->act   = 1;                           // 激活
    d->light = L_ON;                        // 默认常亮
    d->color = ic;                          // 使用用户选的颜色
    d->bon   = 1;                           // 闪烁初始为亮
    d->h     = py;                          // 保存高度

    d->start = (Pt){px, py, pz};            // 设置起始位置（复合字面量）
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

    /* 从位置i开始，每个元素用后一个覆盖 */
    for (int j = i; j < N - 1; j++)
        D[j] = D[j + 1];

    N--;                                    // 总数减一

    /* 如果选中的无人机被删了，调整选中索引 */
    if (S >= N) S = N - 1;
}

/* ================================================================
 *  RC() - 获取无人机的3D渲染颜色（Render Color）
 *
 *  根据灯光模式（灭/亮/闪烁）和选中状态决定显示颜色。
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
    }
}

/* ================================================================
 *  Rst() - 重置回放（Reset）
 *
 *  所有无人机回到起点，重置路径进度，停止播放。
 * ================================================================ */
void Rst(void) {
    for (int i = 0; i < N; i++) {           // 遍历所有无人机
        D[i].pos = D[i].start;              // 位置→起点
        D[i].ci  = 0;                       // 路径索引→0
        D[i].fin = 0;                       // 标记未完成
    }
    play  = false;                          // 停止播放
    pause = false;                          // 取消暂停
    prog  = 0;                              // 进度归零
}

/* ================================================================
 *  Upd() - 更新回放动画（Update playback）
 *
 *  每帧调用。让每架无人机向它的下一个路径点移动。
 *  移动速度 = spd × dt × 3。
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

        /* 路径点全部完成→标记完成 */
        if (d->ci >= d->wc) { d->fin = 1; done++; continue; }

        /* 目标路径点坐标 */
        Pt t = d->wp[d->ci].p;

        /* 计算当前位置到目标的向量差 */
        float dx = t.x - d->pos.x;
        float dy = t.y - d->pos.y;
        float dz = t.z - d->pos.z;

        /* 三维欧几里得距离 */
        float ds = sqrtf(dx * dx + dy * dy + dz * dz);

        /* 足够近（<0.15米）→ 视为到达 */
        if (ds < 0.15f) {
            d->ci++;                        // 切到下一个路径点
            if (d->ci >= d->wc) d->fin = 1; // 最后一个→完成
        } else {
            /* 还没到：沿方向向量移动一步 */
            float step = spd * dt * 3;      // 本帧移动距离
            if (step > ds) step = ds;       // 防止飞过头

            /* dx/ds = X方向的单位分量，×step = 实际位移 */
            d->pos.x += dx / ds * step;
            d->pos.y += dy / ds * step;
            d->pos.z += dz / ds * step;
        }
    }

    /* 计算播放进度（0.0 ~ 1.0） */
    int total = 0, cur = 0;
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        total += D[i].wc > 0 ? D[i].wc : 1;     // 至少算1个单位
        if (D[i].fin)
            cur += D[i].wc > 0 ? D[i].wc : 1;
        else
            cur += D[i].ci;
    }
    prog = total > 0 ? (float)cur / total : 0;  // 防止除零

    /* 全部完成→结束播放 */
    if (done >= N) {
        play = 0;
        Msg("Show finished!");
    }
}
