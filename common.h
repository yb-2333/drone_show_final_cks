/******************************************************************************
 *  common.h  -  公共头文件（常量、类型定义、全局变量声明）
 *
 *  这个文件被所有其他 .c 文件包含，提供整个项目共享的定义。
 *
 *  【给初学者】
 *  .h 文件 = 头文件 = "目录/说明书"，告诉编译器有哪些东西存在。
 *  .c 文件 = 源文件 = "正文"，包含实际的代码实现。
 *
 *  #ifndef / #define / #endif 是 Include Guard（包含守卫），
 *  防止同一个头文件被重复包含导致编译错误。
 ******************************************************************************/
#ifndef COMMON_H        // 如果 COMMON_H 还没被定义过
#define COMMON_H        // 就定义它（之后再次 #include 时会跳过整个文件内容）

/* ============================ 库头文件 ============================ */
#include "raylib.h"     // raylib 图形库：窗口、绘图、3D渲染、输入处理
#include "raymath.h"    // raylib 数学库：向量运算（Vector3Add/Subtract/Scale等）
#include <stdio.h>      // 标准输入输出：字符串格式化
#include <stdlib.h>     // 标准库：atof（字符串转浮点数）
#include <string.h>     // 字符串处理：strlen、strchr、memset
#include <stdarg.h>     // 可变参数：va_list、va_start（用于Msg消息函数）
#include <math.h>       // 数学库：sqrtf（开平方）

/* ============================ 常量宏定义 ============================ */
#define MAX_DRONES 200      // 最多200架无人机
#define MAX_WP      50      // 每架无人机最多50个路径点
#define MAX_NAME    20      // 无人机名字最长20个字符
#define GROUND      40.0f   // 3D地面边长（40米），f后缀表示float类型
#define DR          0.3f    // 无人机模型半径（0.3米）
#define PW          240     // 右侧UI面板宽度（240像素）

/* ============================ 类型定义 ============================ */

/* 灯光模式枚举 */
typedef enum {
    L_OFF     = 0,  // 灯灭
    L_ON,           // 常亮（自动=1）
    L_BLINK,        // 闪烁（自动=2）
    L_PULSE,        // 呼吸：亮度缓慢起伏（自动=3）
    L_CHASE,        // 追逐：按序号相位依次亮（自动=4）
    L_RAINBOW       // 彩虹：色相随时间循环（自动=5）
} Light;

/* 轨迹平滑模式枚举 */
typedef enum {
    PM_LINEAR = 0,  // 直线匀速（无平滑）
    PM_EASED,       // 段内缓动（加速→减速）
    PM_SPLINE       // Catmull-Rom 曲线
} PathMode;

/* 程序工作模式枚举 */
typedef enum {
    M_INTRO = 0,    // 初始欢迎界面
    M_SETUP,        // 创建无人机模式（自动=1）
    M_EDIT,         // 编辑灯光/路径模式（自动=2）
    M_SHOW          // 播放动画模式（自动=3）
} Mode;

/* 三维坐标点 */
typedef struct {
    float x;        // X轴（水平左右）
    float y;        // Y轴（垂直高度）
    float z;        // Z轴（深度前后）
} Pt;

/* 路径点（无人机飞行路线上的一个目标位置） */
typedef struct {
    Pt p;           // 路径点的3D坐标
} Waypoint;

/* 无人机（包含一架无人机的所有数据） */
typedef struct {
    char    name[MAX_NAME];   // 名称，如"D-1"
    Pt      start;            // 起始位置
    Pt      pos;              // 当前位置（播放时会不断变化）
    float   h;                // 高度
    Waypoint wp[MAX_WP];      // 路径点数组
    int     wc;               // 路径点数量（waypoint count）
    Light   light;            // 灯光模式
    int     color;            // 颜色编号（0红/1绿/2蓝）
    float   bt;               // 闪烁计时器
    bool    bon;              // 当前闪烁亮/灭状态
    int     ci;               // 当前路径点索引
    bool    fin;              // 是否已飞完所有路径
    bool    act;              // 是否激活（存在）
    bool    sel;              // 是否被选中
    float   flown;            // 累计已飞行距离（米），平滑回放用
    float   ph;               // 灯光效果相位（pulse/chase/rainbow 用）
    float   espeed;           // 灯光效果速度倍率（默认 1.0）
} Drone;

/* ============================ 全局变量声明（extern） ============================ */
/*
 * extern 关键字告诉编译器："这些变量在某个 .c 文件中定义了，
 * 这里只是声明，不要在这里分配内存。"
 * 真正的变量定义在 common.c 中。
 */

/* 灯光颜色数组 */
extern Color LC[8];             // 八种预设灯光颜色
extern const char* LCN[8];      // 颜色名称字符串

/* 无人机数组和相关状态 */
extern Drone D[MAX_DRONES];     // 所有无人机
extern int   N;                 // 当前无人机数量
extern int   S;                 // 当前选中的无人机索引（-1=未选中）
extern Mode  M;                 // 当前工作模式

/* 3D相机 */
extern Camera3D Cam;            // 控制3D视角的相机

/* Setup模式表单数据 */
extern char sx[16];             // 新无人机起始X坐标（字符串形式）
extern char sy[16];             // 新无人机起始Y坐标
extern char sz[16];             // 新无人机起始Z坐标
extern int  ic;                 // 当前选中的颜色索引

/* Edit模式表单数据 */
extern char wx[16];             // 新增路径点X坐标
extern char wy[16];             // 新增路径点Y坐标
extern char wz[16];             // 新增路径点Z坐标

/* Show模式播放状态 */
extern bool  play;              // 是否正在播放
extern bool  pause;             // 是否暂停
extern float spd;               // 播放速度（0.5x ~ 8x）
extern float prog;              // 播放进度（0.0 ~ 1.0）
extern int   pathMode;          // 轨迹平滑模式（PM_LINEAR/PM_EASED/PM_SPLINE）

/* 消息系统 */
extern char  msg[256];          // 状态消息文本
extern float mt;                // 消息显示剩余时间（>0时可见）
extern int   txtFocus;          // 是否有文字输入框正在接收键盘输入

/* UI颜色常量 */
extern Color Wh;    // White    - 白色
extern Color Gr;    // Gray     - 灰色
extern Color Bl;    // Blue     - 蓝色（强调色）
extern Color Gn;    // Green    - 绿色（确认按钮）
extern Color Rd;    // Red      - 红色（删除按钮）
extern Color Ye;    // Yellow   - 黄色（高亮）
extern Color Bg;    // Background - 背景色
extern Color Pn;    // Panel    - 面板背景
extern Color Br;    // Border   - 边框色
extern Color Bt;    // Button   - 按钮默认色

#endif  /* COMMON_H — 结束包含守卫 */
