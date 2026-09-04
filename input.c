/******************************************************************************
 *  input.c  -  键盘输入 + 更新逻辑模块实现
 *
 *  包含：
 *    Keys()   - 处理所有键盘快捷键
 *    Update() - 每帧更新逻辑（闪烁计时、消息计时、回放更新）
 *
 *  【给初学者】
 *   这里用 IsKeyPressed（按键刚按下时触发一次）和 IsKeyDown（按住时持续触发）
 *   来区分"单次操作"和"连续操作"。
 ******************************************************************************/
#include "input.h"      // 自己的头文件
#include "common.h"     // 全局变量：N, S, M, D, txtFocus, play, pause, Cam 等
#include "drone.h"      // MakeDrone, DelDrone, Rst, Upd
#include "safety.h"     // LiveCheck（播放时实时安全检测）
#include "fileio.h"     // SaveTraj / LoadTraj（轨迹保存/加载）

/* ================================================================
 *  Keys() - 处理键盘快捷键
 *
 *  每帧调用一次，检查各种按键并执行对应操作。
 *  快捷键一览：
 *    Tab      → 切换选中下一架无人机
 *    A        → Setup模式快速创建无人机
 *    Delete   → 删除选中的无人机
 *    1/2/3    → 切换灯光模式（灭/亮/闪烁）
 *    方向键    → 微调无人机位置
 *    Shift+方向键 → 快速移动（2倍速）
 *    Space    → Show模式播放/暂停
 *    Esc      → Show模式停止
 *    F        → 相机聚焦选中无人机
 *    F1/F2/F3 → 快速切换模式
 *    Ctrl+S   → 保存轨迹到文件
 *    Ctrl+L   → 从文件加载轨迹
 * ================================================================ */
void Keys(void) {
    /* Tab：切换选中 */
    if (IsKeyPressed(KEY_TAB) && N > 0) {
        if (S >= 0) D[S].sel = 0;           // 取消旧选中
        S = (S + 1) % N;                    // 索引+1，%取余实现循环
        D[S].sel = 1;                       // 标记新选中
    }

    /* A键：Setup模式下快速创建（文字输入框激活时不响应） */
    if (IsKeyPressed(KEY_A) && M == M_SETUP && !txtFocus)
        MakeDrone();

    /* Delete键：删除选中无人机 */
    if (IsKeyPressed(KEY_DELETE) && S >= 0)
        DelDrone(S);

    /* 数字键：切换选中无人机的灯光模式 */
    if (S >= 0 && S < N && !txtFocus) {     // 有选中 且 无文字输入框激活
        Drone* d = &D[S];

        if (IsKeyPressed(KEY_ONE))   d->light = L_OFF;     // 1→关灯
        if (IsKeyPressed(KEY_TWO))   d->light = L_ON;      // 2→常亮
        if (IsKeyPressed(KEY_THREE)) d->light = L_BLINK;   // 3→闪烁

        /* 方向键：微调位置（Shift加速） */
        float st = IsKeyDown(KEY_LEFT_SHIFT) ? 2 : 0.5f;   // 加速/正常步长
        float ft = GetFrameTime() * 20;                     // 帧时间系数

        if (IsKeyDown(KEY_UP))    d->pos.z -= st * ft;      // 上→Z负（前）
        if (IsKeyDown(KEY_DOWN))  d->pos.z += st * ft;      // 下→Z正（后）
        if (IsKeyDown(KEY_LEFT))  d->pos.x -= st * ft;      // 左→X负
        if (IsKeyDown(KEY_RIGHT)) d->pos.x += st * ft;      // 右→X正
    }

    /* 初始界面：按任意键进入Setup */
    if (M == M_INTRO) {
        for (int k = 0; k < 512; k++) {     // 遍历所有按键码
            if (IsKeyPressed(k)) {
                M = M_SETUP;
                break;
            }
        }
    }

    /* Show模式：空格→播放/暂停, Esc→停止 */
    if (M == M_SHOW) {
        if (IsKeyPressed(KEY_SPACE)) {
            if (!play) { Rst(); play = 1; }
            else pause = !pause;
        }
        if (IsKeyPressed(KEY_ESCAPE))
            Rst();
    }

    /* F键：相机聚焦选中无人机 */
    if (IsKeyPressed(KEY_F) && S >= 0)
        Cam.target = (Vector3){D[S].pos.x, D[S].pos.y, D[S].pos.z};

    /* F1/F2/F3：快速切换模式 */
    if (IsKeyPressed(KEY_F1)) M = M_SETUP;
    if (IsKeyPressed(KEY_F2)) M = M_EDIT;
    if (IsKeyPressed(KEY_F3)) { M = M_SHOW; Rst(); }

    /* Ctrl+S：保存轨迹 / Ctrl+L：加载轨迹（文字输入框激活时不响应） */
    if (M != M_INTRO && !txtFocus) {
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
            SaveTraj(TRAJ_FILENAME);
        if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_L))
            LoadTraj(TRAJ_FILENAME);
    }
}

/* ================================================================
 *  ShowStatus() - 终端实时状态输出（回放时）
 *
 *  这是"实时模拟"的文字版：在 Show 模式播放时，把每架无人机的
 *  当前位置和灯光模式打印到终端（控制台），不依赖3D窗口也能看到状态。
 *
 *  用静态变量 printT 做节流（每0.5秒打印一次），避免刷屏；
 *  fflush(stdout) 保证文字立刻显示（否则可能被输出缓冲卡住）。
 * ================================================================ */
static float printT = 0;                        // 打印计时器（秒）

static void ShowStatus(float dt) {
    printT += dt;                               // 累计经过的时间
    if (printT < 0.5f) return;                  // 还没到间隔→本帧不打印
    printT = 0;                                 // 重置计时器

    printf("\n--- Live Show: %.0f%% ---\n", prog * 100);   // 进度标题

    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;                  // 跳过已删除的

        /* 灯光模式 → 可读文字 */
        const char* lm =
            d->light == L_OFF   ? "OFF"   :
            d->light == L_BLINK ? "BLINK" : "ON";

        /* 每行：名字 + 位置 + 灯光 */
        printf("%-6s pos=(%6.1f, %5.1f, %6.1f)  light=%-5s\n",
               d->name, d->pos.x, d->pos.y, d->pos.z, lm);
    }
    fflush(stdout);                             // 立即输出，不等待缓冲
}

/* ================================================================
 *  Update() - 每帧更新逻辑
 *
 *  调用顺序：在 main() 主循环中先调 Keys()，再调 Update()。
 *
 *  负责：
 *    - 重置文字输入焦点标记
 *    - 递减消息显示计时器
 *    - 更新闪烁无人机的亮/灭状态
 *    - Show模式下驱动回放动画
 *
 *  参数:
 *    dt - 帧间隔时间（秒）
 * ================================================================ */
void Update(float dt) {
    txtFocus = 0;                           // 每帧重置（由Txt在需要时设为1）

    if (mt > 0) mt -= dt;                   // 消息计时器递减（减到0消失）

    /* 更新所有无人机的闪烁效果 */
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;              // 跳过不存在的

        if (d->light == L_BLINK) {          // 只有闪烁模式需要处理
            d->bt += dt;                    // 累计时间
            if (d->bt >= 0.5f) {            // 每0.5秒切换
                d->bt -= 0.5f;              // 重置计时器（保留超出部分）
                d->bon = !d->bon;           // 翻转亮/灭状态
            }
        }
    }

    /* Show模式：驱动回放动画 */
    if (M == M_SHOW) {
        Upd(dt);
        /* 实时安全检测：播放中若越界或碰撞，暂停并弹窗 */
        if (play && !pause && LiveCheck())
            pause = true;
        /* 终端实时状态输出：动态显示每架无人机的位置和灯光 */
        if (play && !pause) ShowStatus(dt);
    }
}
