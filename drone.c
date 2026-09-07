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

/* ================================================================
 *  LightName() - 把灯光模式转成可读名字（终端打印用）
 * ================================================================ */
const char* LightName(Light l) {
    switch (l) {
        case L_OFF:     return "Off";
        case L_ON:      return "On";
        case L_BLINK:   return "Blink";
        case L_PULSE:   return "Pulse";
        case L_CHASE:   return "Chase";
        case L_RAINBOW: return "Rainbow";
        default:        return "?";
    }
}

/* ================================================================
 *  PrintDrone() - 在终端打印一架无人机的实时状态
 *
 *  对应课程要求"实时模拟：在终端动态显示位置和灯光状态"。
 *  选中 / 改灯光 / 改颜色 / 改坐标时都会调用，打印一行状态。
 * ================================================================ */
void PrintDrone(const Drone* d) {
    printf("[%s] pos=(%.0f, %.0f, %.0f)  color=%s  light=%s\n",
           d->name, d->pos.x, d->pos.y, d->pos.z,
           LCN[d->color], LightName(d->light));
    fflush(stdout);                 // 立即刷新，保证终端实时看到
}

/* ================================================================
 *  FillCoords() - 把某架无人机的起点坐标写入 sx/sy/sz 输入框
 *
 *  供 Setup 界面"点列表行 → 坐标输入框显示该机坐标"使用。
 * ================================================================ */
void FillCoords(const Drone* d) {
    snprintf(sx, sizeof(sx), "%.0f", d->start.x);
    snprintf(sy, sizeof(sy), "%.0f", d->start.y);
    snprintf(sz, sizeof(sz), "%.0f", d->start.z);
}

/* ================================================================
 *  ApplySetupCoords() - 把 Setup 坐标输入框的值应用到选中无人机
 *
 *  用户在 Setup 选中一架无人机、改坐标后按回车，就把它移动到新位置。
 * ================================================================ */
void ApplySetupCoords(void) {
    if (S < 0 || S >= N || !D[S].act) return;

    float px = (float)atof(sx);
    float py = (float)atof(sy);
    float pz = (float)atof(sz);

    Pt p = (Pt){px, py, pz};
    if (!InAirspace(p)) {           // 越界则弹窗提示
        SetAlert("Position out of range (%.0f, %.0f, %.0f)", px, py, pz);
        return;
    }

    D[S].start = p;                 // 移动到新位置
    D[S].pos   = p;
    D[S].h     = pz;
    CheckOverlap(S);                // 检查是否与其他机起点/终点重合
    Msg("%s moved to (%.0f, %.0f, %.0f)", D[S].name, px, py, pz);
    PrintDrone(&D[S]);              // 终端实时显示新坐标
}

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

    /* 起点越界检查：X/Y/Z 必须在 0~40 内（40×40×40 空域），越界则弹窗并放弃创建 */
    Pt sp = (Pt){px, py, pz};               // 起始位置（复合字面量）
    if (!InAirspace(sp)) {
        SetAlert("Start out of range (%.0f, %.0f, %.0f)", px, py, pz);
        return;
    }

    Drone* d = &D[N];                       // 取第N个槽位的指针
    memset(d, 0, sizeof(Drone));            // 全部内存清零（安全初始化）
    d->act   = 1;                           // 激活
    d->light = L_ON;                        // 默认常亮
    d->color = ic;                          // 使用用户选的颜色
    d->bon   = 1;                           // 闪烁初始为亮
    d->h     = pz;                          // 保存高度
    d->espeed = 1.0f;                       // 灯光效果速度倍率默认1
    d->pm     = PM_EASED;                   // 轨迹平滑模式默认缓动
    d->ph     = (float)N;                   // 效果相位 = 序号（追逐灯用）

    d->start = sp;                          // 设置起始位置
    d->pos   = d->start;                    // 当前位置=起始位置

    snprintf(d->name, MAX_NAME, "D-%d", N + 1); // 生成名称 "D-1", "D-2"...
    N++;                                    // 总数+1
    S = N - 1;                              // 自动选中新创建的

    Msg("Created %s (%.0f,%.0f,%.0f) [%s]",
        d->name, px, py, pz, LCN[ic]);

    CheckOverlap(N - 1);                    // 检查新机是否与其他机起点/终点重合
    PrintDrone(d);                          // 终端实时显示新机状态
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

    /* 主灯光球（1.3倍基准半径）
     * 注意：数据用 Z 表示高度，raylib 用 Y 表示向上，绘制时交换 Y/Z。 */
    DrawSphere((Vector3){p.x, p.z, p.y}, DR * 1.3f, c);

    /* 第一层光晕（1.9倍半径，30%透明度） */
    DrawSphere((Vector3){p.x, p.z, p.y}, DR * 1.9f, Fade(c, 0.3f));

    /* 机身核心（0.5倍半径，深灰色） */
    DrawSphere((Vector3){p.x, p.z, p.y}, DR * 0.5f, (Color){28, 28, 36, 255});

    /* 选中高亮环（蓝色圆圈围绕无人机） */
    if (d->sel)
        DrawCircle3D((Vector3){p.x, p.z, p.y}, DR * 2.0f,
                     (Vector3){0, 1, 0}, 0, Bl);

    /* 编辑模式下：绘制黄色路径点和连线 */
    if (M == M_EDIT && d->wc > 0) {
        for (int i = 0; i < d->wc; i++) {
            /* 路径点小球（黄色） */
            DrawSphere((Vector3){d->wp[i].p.x, d->wp[i].p.z, d->wp[i].p.y},
                       DR * 0.7f, Ye);

            /* 上一个点到当前点的连线（第一个路径点的"上一个"是起点） */
            Pt pr = (i == 0) ? d->start : d->wp[i - 1].p;
            DrawLine3D((Vector3){pr.x, pr.z, pr.y},
                       (Vector3){d->wp[i].p.x, d->wp[i].p.z, d->wp[i].p.y},
                       Fade(Ye, 0.5f));
        }

        /* 平滑路径预览：按这架无人机自己的 pm 采样画出实际飞行轨迹（蓝色）
         * 缓动 = 直线+缓动，样条 = 平滑曲线，
         * 让用户在编辑时就能看到回放会走的真实路径。 */
        float L = PathLen(d);                   // 总长
        int   n = 48;                           // 采样段数（越多越平滑）
        Pt    prev = d->start;
        for (int k = 1; k <= n; k++) {
            float s = L * k / n;                // 等距采样点
            Pt    p = DronePosAt(d, s);         // 用共享函数采样位置
            DrawLine3D((Vector3){prev.x, prev.z, prev.y},
                       (Vector3){p.x, p.z, p.y}, Fade(Bl, 0.6f));
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
 *  DronePosAt() 按每架无人机自己的平滑模式（pm）采样位置。
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
        d->pos = DronePosAt(d, d->flown);
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
 *  两种用法：
 *    1. FormCircle / FormLine / FormGrid —— 把无人机"瞬间"排成队形
 *       （直接改 start/pos，无人机停在哪），Setup 界面用。
 *    2. FormTransition —— "一键队形变换动画"：给每架无人机追加一个
 *       飞到目标队形位置的航点，Show 播放时它们会一起飞过去，Edit 界面用。
 * ================================================================ */

/* Clampf() - 把 v 限制在 [lo, hi] 之间 */
static float Clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* FormTarget() - 计算所有无人机在某种队形下的目标位置（只算不改）
 *   type: 0=圆形 1=直线 2=网格
 *   out : 输出数组，out[i] = 第 i 架无人机的目标坐标
 */
static void FormTarget(int type, Pt out[MAX_DRONES]) {
    if (type == 0) {                        // 圆形：围绕地面中心等角分布
        float cx = GROUND / 2;              // 圆心 X（水平）
        float cy = GROUND / 2;              // 圆心 Y（水平）
        float R  = 3.0f + N * 0.6f;         // 半径随数量增大，避免挤在一起
        if (R > GROUND / 2 - 1) R = GROUND / 2 - 1;
        float h  = 5.0f;                    // 飞行高度（Z 轴）
        for (int i = 0; i < N; i++) {
            float a = (float)i / N * 2.0f * PI;   // 等角分布（0~2π）
            out[i] = (Pt){
                Clampf(cx + cosf(a) * R, 0.5f, GROUND - 0.5f),   // X
                Clampf(cy + sinf(a) * R, 0.5f, GROUND - 0.5f),   // Y（水平）
                h                                                 // Z（高度）
            };
        }
    } else if (type == 1) {                 // 直线：沿 X 轴等距排开
        float spacing = (N <= 1) ? 0 : (GROUND - 2) / (N - 1);
        float h = 5.0f, y = GROUND / 2;     // 高度统一、Y 固定在中央
        for (int i = 0; i < N; i++)
            out[i] = (Pt){ Clampf(1.0f + i * spacing, 0.5f, GROUND - 0.5f), y, h };
    } else {                                // 网格：接近正方形的栅格
        int cols = (int)ceilf(sqrtf((float)N));
        int rows = (N + cols - 1) / cols;
        float h  = 5.0f;                    // 飞行高度（Z 轴）
        float gx = GROUND / (cols + 1);     // X 方向格子间距
        float gy = GROUND / (rows + 1);     // Y 方向格子间距
        for (int i = 0; i < N; i++) {
            int c = i % cols, r = i / cols;
            out[i] = (Pt){ gx * (c + 1), gy * (r + 1), h };
        }
    }
}

/* FormReset() - 编队变换的公共前置：重置飞行状态 */
static void FormReset(const char* name) {
    for (int i = 0; i < N; i++) {
        D[i].flown = 0;                     // 重置已飞距离
        D[i].fin   = 0;                     // 标记未完成
        D[i].ci    = 0;                     // 路径索引归零
    }
    Msg("Formation: %s (%d drones)", name, N);
}

/* FormCircle() - 圆形编队：所有无人机瞬间排成圆形 */
void FormCircle(void) {
    if (N <= 0) return;
    FormReset("Circle");
    Pt t[MAX_DRONES];
    FormTarget(0, t);
    for (int i = 0; i < N; i++) {
        D[i].start = t[i];
        D[i].pos   = t[i];
        D[i].h     = 5.0f;
    }
}

/* FormLine() - 直线编队：所有无人机瞬间排成直线 */
void FormLine(void) {
    if (N <= 0) return;
    FormReset("Line");
    Pt t[MAX_DRONES];
    FormTarget(1, t);
    for (int i = 0; i < N; i++) {
        D[i].start = t[i];
        D[i].pos   = t[i];
        D[i].h     = 5.0f;
    }
}

/* FormGrid() - 网格编队：所有无人机瞬间排成网格 */
void FormGrid(void) {
    if (N <= 0) return;
    FormReset("Grid");
    Pt t[MAX_DRONES];
    FormTarget(2, t);
    for (int i = 0; i < N; i++) {
        D[i].start = t[i];
        D[i].pos   = t[i];
        D[i].h     = 5.0f;
    }
}

/* ================================================================
 *  FormTransition() - 一键队形变换动画
 *
 *  与上面三个"瞬间排好"不同，这里不直接移动无人机，而是给每架
 *  无人机追加一个"飞到目标队形位置"的航点。进入 Show 播放时，
 *  所有无人机就会从当前位置一起飞到目标队形，形成变换动画。
 *
 *  type: 0=圆形 1=直线 2=网格
 * ================================================================ */
void FormTransition(int type) {
    if (N <= 0) return;
    Pt t[MAX_DRONES];
    FormTarget(type, t);

    int added = 0;
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;
        if (D[i].wc >= MAX_WP) continue;     // 航点满了跳过
        D[i].wp[D[i].wc].p = t[i];           // 追加一个目标航点
        D[i].wc++;
        added++;
    }

    const char* name = type == 0 ? "Circle" : type == 1 ? "Line" : "Grid";
    Msg("Fly to %s: %d waypoints added", name, added);
}

/* ================================================================
 *  MakeDemo() - 一键生成示例表演
 *
 *  生成 12 架无人机：起点排成圆形，颜色轮流、灯光效果各异，
 *  再自动追加"飞到直线 → 网格"的航点，形成完整的队形变换动画。
 *  新手打开软件点一下"Load Demo Show"，就能直接进 Show 播放看效果。
 * ================================================================ */
void MakeDemo(void) {
    N = 0;                              // 清空已有无人机
    S = -1;

    int count = 12;                     // 示例无人机数量
    for (int i = 0; i < count; i++) {
        snprintf(sx, sizeof(sx), "%.0f", 5.0f + i);   // 临时起点（稍后重排）
        snprintf(sy, sizeof(sy), "20");               // Y（水平）
        snprintf(sz, sizeof(sz), "5");                // Z（高度）
        ic = i % 8;                     // 颜色轮流
        MakeDrone();
    }

    FormCircle();                       // 起点排成圆形编队

    Light lights[] = {L_ON, L_BLINK, L_PULSE, L_CHASE, L_RAINBOW};
    for (int i = 0; i < N; i++)
        D[i].light = lights[i % 5];     // 灯光效果轮流

    FormTransition(1);                  // 追加航点：飞到直线
    FormTransition(2);                  // 追加航点：飞到网格
    Rst();                              // 回到起点，准备播放

    Msg("Demo loaded: %d drones - go to Show and Play", N);
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
    PrintDrone(d);                          // 终端显示复制出的新机
}

