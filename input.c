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
#include "json.h"       // SaveShow/LoadShow（Ctrl+S/L）
#include "utils.h"      // Msg（保存/加载结果提示）
#include "render.h"     // MouseGround（鼠标拖拽移动无人机）

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
 * ================================================================ */
void Keys(void) {
    /* Tab：切换选中 */
    if (IsKeyPressed(KEY_TAB) && N > 0) {
        if (S >= 0) D[S].sel = 0;           // 取消旧选中
        S = (S + 1) % N;                    // 索引+1，%取余实现循环
        D[S].sel = 1;                       // 标记新选中
        PrintDrone(&D[S]);                  // 终端显示新选中
    }

    /* Enter：Setup 模式下把坐标输入框的值应用到选中无人机（移动它） */
    if (IsKeyPressed(KEY_ENTER) && M == M_SETUP && S >= 0)
        ApplySetupCoords();

    /* A键：Setup模式下快速创建（文字输入框激活时不响应） */
    if (IsKeyPressed(KEY_A) && M == M_SETUP && !txtFocus)
        MakeDrone();

    /* Delete键：删除选中无人机 */
    if (IsKeyPressed(KEY_DELETE) && S >= 0)
        DelDrone(S);

    /* 数字键：切换选中无人机的灯光模式 */
    if (S >= 0 && S < N && !txtFocus) {     // 有选中 且 无文字输入框激活
        Drone* d = &D[S];

        /* 灯光切换：1=关 2=常亮 3=闪烁，切换后终端显示 */
        if (IsKeyPressed(KEY_ONE))   { d->light = L_OFF;   PrintDrone(d); } // 1→关灯
        if (IsKeyPressed(KEY_TWO))   { d->light = L_ON;    PrintDrone(d); } // 2→常亮
        if (IsKeyPressed(KEY_THREE)) { d->light = L_BLINK; PrintDrone(d); } // 3→闪烁

        /* 方向键：微调位置（Shift加速） */
        float st = IsKeyDown(KEY_LEFT_SHIFT) ? 2 : 0.5f;   // 加速/正常步长
        float ft = GetFrameTime() * 20;                     // 帧时间系数

        if (IsKeyDown(KEY_UP))    d->pos.y -= st * ft;      // 上→Y负（前）
        if (IsKeyDown(KEY_DOWN))  d->pos.y += st * ft;      // 下→Y正（后）
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
        Cam.target = (Vector3){D[S].pos.x, D[S].pos.z, D[S].pos.y};  // 数据Z=高度→raylib Y

    /* 鼠标左键拖拽移动选中无人机（Edit模式，3D区域内，非告警时）
     * 拖拽是连续操作，和方向键一样：按住全程只记录一次快照。 */
    {
        bool over3D = GetMousePosition().x <= GetScreenWidth() - PW;  // 不在UI面板上
        bool down = M == M_EDIT && S >= 0 && over3D && !alertActive &&
                    IsMouseButtonDown(MOUSE_LEFT_BUTTON);

        if (down) {
            Pt g = MouseGround(D[S].pos.z);         // 在当前高度（Z）平面求交点
            if (g.x < 0)          g.x = 0;          // 钳制到空域内
            if (g.x > GROUND)     g.x = GROUND;
            if (g.y < 0)          g.y = 0;
            if (g.y > GROUND)     g.y = GROUND;
            D[S].pos.x   = g.x;                     // 更新当前位置（水平 X/Y）
            D[S].pos.y   = g.y;
            D[S].start.x = g.x;                     // 同步起始位置（持久生效）
            D[S].start.y = g.y;
        }
    }

    /* F1/F2/F3：快速切换模式 */
    if (IsKeyPressed(KEY_F1)) M = M_SETUP;
    if (IsKeyPressed(KEY_F2)) M = M_EDIT;
    if (IsKeyPressed(KEY_F3)) { M = M_SHOW; Rst(); }

    /* Ctrl 快捷键：保存/加载（文字输入时不响应） */
    if (!txtFocus && IsKeyDown(KEY_LEFT_CONTROL)) {
        if (IsKeyPressed(KEY_S)) {
            if (SaveShow("show.json")) Msg("Saved to show.json");
            else                       Msg("Save failed!");
        }
        if (IsKeyPressed(KEY_L)) {
            if (LoadShow("show.json")) { Rst(); Msg("Loaded from show.json"); }
            else                        Msg("Load failed (no show.json?)");
        }
    }
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

    /* 更新所有无人机的灯光效果 */
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;              // 跳过不存在的

        if (d->light == L_BLINK) {          // 闪烁：每0.5秒翻转亮/灭
            d->bt += dt;                    // 累计时间
            if (d->bt >= 0.5f) {            // 每0.5秒切换
                d->bt -= 0.5f;              // 重置计时器（保留超出部分）
                d->bon = !d->bon;           // 翻转亮/灭状态
            }
        } else if (d->light == L_PULSE || d->light == L_CHASE ||
                   d->light == L_RAINBOW) {
            /* 呼吸/追逐/彩虹都靠相位 ph 驱动，相位随时间推进 */
            d->ph += dt * d->espeed * 2.0f;
        }
    }

    /* Show模式：驱动回放动画 */
    if (M == M_SHOW) {
        Upd(dt);
        /* 实时安全检测：播放中若越界或碰撞，暂停并弹窗 */
        if (play && !pause && LiveCheck())
            pause = true;

        /* 实时模拟：播放时每 0.5 秒在终端打印一次所有无人机的位置和灯光 */
        if (play && !pause) {
            static float termTimer = 0;          // 终端打印节流计时器
            termTimer += dt;
            if (termTimer >= 0.5f) {             // 每 0.5 秒打印一次
                termTimer = 0;
                printf("---- live ----\n");
                for (int i = 0; i < N; i++)
                    if (D[i].act) PrintDrone(&D[i]);
            }
        }
    }
}
