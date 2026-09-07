/******************************************************************************
 *  main.c  -  程序入口
 *
 *  这是整个程序的起点。main() 函数做三件事：
 *    1. 初始化（窗口、相机）
 *    2. 主循环（输入→更新→渲染，每帧一次）
 *    3. 清理退出
 *
 *  【给初学者】
 *  每个C程序都有且只有一个 main() 函数，操作系统从这里开始执行程序。
 *  main() 返回 0 表示正常退出，非0表示异常退出。
 *
 *  编译命令（在 drone_light_show_cks 目录下执行）：
 *    gcc common.c utils.c drone.c render.c ui.c input.c safety.c main.c \
 *        -o drone_light_show -lraylib -lopengl32 -lgdi32 -lwinmm
 ******************************************************************************/
/* ================================================================
 *  课程要求对照（无人机编队灯光秀模拟）
 *
 *  1. 编队初始化：设置数量/初始位置/高度  -> Setup 面板 + MakeDrone
 *  2. 轨迹设计：关键坐标点自动移动、队形变换 -> Edit 航点 + FormTransition
 *  3. 灯光控制：开关/颜色/闪烁          -> Edit 灯光+颜色 + RC()
 *  4. 实时模拟：终端显示位置和灯光状态   -> PrintDrone()（选中/播放时打印）
 *  5. 安全检测：越界/距离过近提示        -> InAirspace / CheckOverlap / LiveCheck
 *  6. 数据回放：自动保存轨迹、重开自动加载重现 -> SaveShow/LoadShow(show.json) + Show 播放
 * ================================================================ */
#include "common.h"     // 全局变量：M, Cam, D, S, N, mt, msg, Bg, Bl, Gr, Gn, Ye, PW
#include "utils.h"      // （main 不直接调用工具函数，但通过UI间接使用）
#include "drone.h"      // Rst（Show模式重置）
#include "render.h"     // Draw3D, Pick
#include "ui.h"         // DrawUI, DrawStartScreen
#include "input.h"      // Keys, Update
#include "json.h"       // SaveShow/LoadShow（自动保存/加载轨迹）

/* ================================================================
 *  main() - 程序入口函数
 * ================================================================ */
int main(void) {
    /* 关闭 stdout 缓冲，保证 printf 立即输出到终端（实时模拟用） */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* 终端启动提示：说明操作流程，播放时这里会实时打印无人机状态 */
    printf("=== Drone Light Show Simulator ===\n");
    printf("Flow: Setup -> Edit -> Show   (F1/F2/F3)\n\n");

    /* ---- 窗口初始化 ---- */
    /* 设置窗口属性：4倍抗锯齿（画面更平滑）+ 可调整大小 */
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Drone Light Show");  // 1280×720窗口
    SetTargetFPS(60);                            // 目标60帧/秒

    /* ---- 3D相机初始化 ---- */
    /* 设置一个固定的斜视角，从侧上方俯瞰场景 */
    Cam.position   = (Vector3){55, 48, 70};      // 相机位置（站在哪个点看）
    Cam.target     = (Vector3){20, 0, 15};       // 注视目标（看向场景中央）
    Cam.up         = (Vector3){0, 1, 0};          // 上方向（Y轴向上）
    Cam.fovy       = 50;                         // 视场角（视角广度，度）
    Cam.projection = CAMERA_PERSPECTIVE;          // 透视投影（近大远小）

    /* ---- 相机球坐标参数（用于旋转视角） ---- */
    /* 把初始位置换算成「距离 + 水平角 + 俯仰角」，方便后面绕目标环绕 */
    Vector3 off0  = Vector3Subtract(Cam.position, Cam.target);  // 目标指向相机的向量
    float camDist  = Vector3Length(off0);       // 相机到目标的距离
    float camYaw   = atan2f(off0.x, off0.z);    // 水平环绕角（绕Y轴，弧度）
    float camPitch = asinf(off0.y / camDist);   // 俯仰角（相对水平面，弧度）

    /* ---- 自动加载上次保存的轨迹（数据回放：关掉重开能重现灯光秀） ----
     * 第一次运行没有 show.json，LoadShow 失败就静默跳过，从头开始。 */
    if (LoadShow("show.json")) {
        Rst();                          // 恢复到起点，准备播放
        printf("Loaded saved show: %d drones restored (F3 to replay)\n", N);
    } else {
        printf("No saved show yet - build a new one\n");
    }

    /* ==================== 主循环 ==================== */
    /* WindowShouldClose() 在用户点关闭按钮时返回 true，循环结束 */
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();              // 本帧时间间隔（秒）

        Keys();                                 // ① 处理键盘快捷键

        /* ---- 鼠标滚轮缩放 ---- */
        float wh = GetMouseWheelMove();         // 滚轮滚动量（+放大/-缩小）
        if (wh != 0) {
            if (wh > 0) camDist *= 0.9f;        // 前滚→靠近（放大）
            else        camDist *= 1.1f;        // 后滚→远离（缩小）
            if (camDist < 5)  camDist = 5;      // 最近5米
            if (camDist > 120) camDist = 120;   // 最远120米（初始约81米，保证能缩回原大小）
        }

        /* ---- 鼠标右键拖拽：环绕旋转视角 ---- */
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            Vector2 d = GetMouseDelta();        // 本帧鼠标移动量
            camYaw   -= d.x * 0.01f;            // 左右拖动→水平环绕
            camPitch += d.y * 0.01f;            // 上下拖动→改变俯仰
            if (camPitch >  1.5f) camPitch =  1.5f;  // 限制俯仰角（≈86°）
            if (camPitch < -1.5f) camPitch = -1.5f;  // 避免翻转到地面以下
        }

        /* ---- 根据球坐标重新计算相机位置（缩放/旋转/聚焦后都生效） ---- */
        {
            float cy = cosf(camPitch);          // 俯仰角余弦 = 水平分量系数
            Vector3 off = {
                camDist * cy * sinf(camYaw),    // X = 水平分量 × 水平角正弦
                camDist * sinf(camPitch),       // Y = 高度
                camDist * cy * cosf(camYaw)     // Z = 水平分量 × 水平角余弦
            };
            Cam.position = Vector3Add(Cam.target, off);  // 目标 + 偏移 = 相机位置
        }

        /* ---- 初始界面处理 ---- */
        if (M == M_INTRO) {
            Update(dt);                         // 更新逻辑
            BeginDrawing();                     // 开始绘制帧
            ClearBackground(Bg);                // 清屏
            DrawStartScreen();                  // 画欢迎界面
            EndDrawing();                       // 提交绘制
            continue;                           // 跳回循环开头（跳过3D/UI渲染）
        }

        /* ---- 3D拾取（点击选中无人机） ---- */
        int pk = Pick();                        // 检测点击了哪架
        if (pk >= 0) {                          // 点到了
            if (S >= 0) D[S].sel = 0;           // 取消旧选中
            S = pk;                             // 更新选中索引
            D[S].sel = 1;                       // 标记新选中
            if (M == M_SETUP) M = M_EDIT;       // Setup下点击→自动进Edit
            PrintDrone(&D[S]);                  // 终端实时显示选中无人机
        }

        /* ---- 渲染 ---- */
        Update(dt);                             // ② 更新逻辑
        BeginDrawing();                         // ③ 开始绘制
        ClearBackground(Bg);                    // 清屏（深色背景）
        Draw3D();                               // 画3D场景
        DrawUI();                               // 画UI面板

        /* ---- 左下角帮助面板 ---- */
        int hy = GetScreenHeight() - 100;
        DrawRectangle(6, hy, 210, 96, Fade(BLACK, 0.7f));      // 半透明背景

        DrawText("Help", 12, hy + 4, 13, Bl);
        DrawText("F1=Setup F2=Edit F3=Show", 12, hy + 20, 11, Gr);
        DrawText("Tab=Next  1/2/3=Light",    12, hy + 34, 11, Gr);
        DrawText("Click=Select  Enter=Move", 12, hy + 48, 11, Gr);
        DrawText("Scroll=Zoom  R-Drag=Rotate", 12, hy + 62, 11, Gr);

        /* 状态栏：当前模式 + 无人机数量 */
        DrawText(TextFormat("Mode:%s  Drones:%d",
            M == M_INTRO ? "Intro" :
            M == M_SETUP ? "Setup" :
            M == M_EDIT  ? "Edit"  : "Show",
            N),
            12, hy + 78, 11, Ye);

        /* 状态消息（计时器>0时显示） */
        if (mt > 0)
            DrawText(msg, GetScreenWidth() / 2 - 150, 6, 13, Gn);

        /* FPS显示（右上角） */
        DrawText(TextFormat("FPS:%d", GetFPS()),
                 GetScreenWidth() - PW - 50, 6, 11, Gr);

        DrawAlert();                            // 绘制安全告警弹窗（有告警时才显示）

        EndDrawing();                           // ④ 提交绘制帧
    }

    /* ---- 退出前自动保存轨迹（下次打开自动恢复） ---- */
    if (SaveShow("show.json"))
        printf("Show saved - it will restore next launch\n");

    CloseWindow();                              // 关闭窗口，释放资源
    return 0;                                   // 正常退出
}
