/******************************************************************************
 *  无人机灯光秀模拟器  (Drone Light Show Simulator)
 *
 *  工作流程： 1.Setup（创建无人机）→ 2.Edit（编辑灯光和路径）→ 3.Show（播放）
 *
 *  这是一个用C语言 + raylib库编写的3D无人机编队灯光秀模拟程序。
 *  你可以在3D场景中创建无人机，设置它们的灯光颜色和飞行路径，然后播放动画。
 *
 *  本文件已逐行添加中文注释，适合编程初学者阅读学习。
 ******************************************************************************/

/* ============================ 头文件包含 ============================ */
/*
 * #include 是C语言的"导入"指令，相当于把别人写好的代码拿过来用。
 * 尖括号 < > 表示从系统库目录找，双引号 " " 表示从当前目录找。
 */

#include "raylib.h"      // raylib 图形库的主头文件，提供窗口、绘图、3D等功能
#include "raymath.h"     // raylib 数学库，提供向量运算等3D数学工具
#include <stdio.h>       // 标准输入输出库，提供 printf、文件读写等功能
#include <stdlib.h>      // 标准库，提供 atof（字符串转浮点数）、内存分配等功能
#include <string.h>      // 字符串处理库，提供 strlen、strchr、memset 等功能
#include <stdarg.h>      // 可变参数库，让函数可以接收不定数量的参数（如 printf）
#include <math.h>        // 数学库，提供 sqrtf（开平方）、sin、cos 等数学函数

/* ============================ 常量定义 ============================ */
/*
 * #define 是宏定义，相当于给一个值取个名字。
 * 编译时会把所有出现这个名字的地方替换成对应的值。
 * 用大写字母命名是C语言的约定，方便区分常量和变量。
 */

#define MAX_DRONES 200       // 最多支持的无人机数量（200架）
#define MAX_WP      50       // 每架无人机最多支持的路径点数量（50个）
#define MAX_NAME    20       // 无人机名字的最大长度（20个字符）
#define GROUND      40.0f    // 地面（3D场景）的边长，单位是"米"，f表示float类型
#define DR          0.3f     // 无人机3D模型的半径（0.3米≈30厘米）
#define PW          240      // 右侧UI面板的宽度（240像素）

/* ============================ 类型定义 ============================ */
/*
 * typedef 用于给已有的类型取一个新名字/别名，让代码更简洁。
 * enum 是"枚举"，给一组相关的整数常量取名字，提高代码可读性。
 * struct 是"结构体"，把多个不同类型的数据打包成一个整体。
 */

/* 枚举：无人机的灯光状态（本质是整数：L_OFF=0, L_ON=1, L_BLINK=2） */
typedef enum {
    L_OFF   = 0,    // 灯灭——无人机灯光关闭
    L_ON,           // 灯亮——无人机灯光常亮（自动=1）
    L_BLINK         // 闪烁——无人机灯光一闪一闪（自动=2）
} Light;

/* 枚举：程序的三种工作模式（M_INTRO=0, M_SETUP=1, M_EDIT=2, M_SHOW=3） */
typedef enum {
    M_INTRO = 0,    // 初始界面——显示欢迎画面
    M_SETUP,        // 创建模式——添加无人机（自动=1）
    M_EDIT,         // 编辑模式——设置灯光和飞行路径（自动=2）
    M_SHOW          // 播放模式——播放飞行动画（自动=3）
} Mode;

/* 结构体：三维坐标点（包含 x, y, z 三个浮点数） */
typedef struct {
    float x;    // X轴坐标（水平方向，左右）
    float y;    // Y轴坐标（垂直方向，高度）
    float z;    // Z轴坐标（深度方向，前后）
} Pt;

/* 结构体：路径点（无人机飞行途中的一个目标位置） */
typedef struct {
    Pt p;       // 路径点的3D坐标
} Waypoint;

/* 结构体：无人机（包含所有与一架无人机相关的数据） */
typedef struct {
    char    name[MAX_NAME];   // 无人机名称（如 "D-1", "D-2"）
    Pt      start;            // 无人机的起始位置（创建时设定的位置）
    Pt      pos;              // 无人机的当前位置（播放动画时会变化）
    float   h;                // 无人机的高度（与 start.y 相同，冗余存储）
    Waypoint wp[MAX_WP];      // 路径点数组，最多 MAX_WP 个（每个路径点是一个3D坐标）
    int     wc;               // 路径点数量（waypoint count，实际使用了几个路径点）
    Light   light;            // 当前灯光模式（L_OFF / L_ON / L_BLINK）
    int     color;            // 灯光颜色编号（0=红色, 1=绿色, 2=蓝色，对应 LC 数组）
    float   bt;               // 闪烁计时器（blink timer），累计时间用于控制闪烁节奏
    bool    bon;              // 当前闪烁状态（blink on），true=亮, false=灭
    int     ci;               // 当前到达的路径点索引（current index），播放时用
    bool    fin;              // 是否已完成所有路径点（finished），true=已飞完
    bool    act;              // 是否激活（active），true=存在, false=已删除/不存在
    bool    sel;              // 是否被选中（selected），true=当前正在编辑这架无人机
} Drone;

/* ============================ 灯光颜色数据 ============================ */
/*
 * static 表示这些变量/函数只在当前文件内可见，不会被外部文件访问。
 * Color 是 raylib 库定义的结构体，包含 r, g, b, a 四个分量（红、绿、蓝、透明度）。
 * 每个分量的取值范围是 0-255。
 */

/* LC = Light Colors，三种预设灯光颜色 */
static Color LC[3] = {
    {255, 60,  60,  255},   // 颜色0：红色  (R=255满红, G=60少绿, B=60少蓝)
    {60,  255, 60,  255},   // 颜色1：绿色  (R=60少红, G=255满绿, B=60少蓝)
    {60,  120, 255, 255}    // 颜色2：蓝色  (R=60少红, G=120中绿, B=255满蓝)
};

/* LCN = Light Color Names，颜色名称数组，与上面的颜色一一对应 */
static const char* LCN[] = {"Red", "Green", "Blue"};

/* ============================ 全局变量 ============================ */
/*
 * 全局变量在整个程序运行期间一直存在，所有函数都可以访问。
 * 它们保存程序的"状态"——比如当前有多少架无人机、用户选中了哪一架等。
 */

static Drone    D[MAX_DRONES];  // 无人机数组（D = Drones），最多存 MAX_DRONES 架
static int      N       = 0;    // 当前无人机数量（Number of drones），初始为0
static int      S       = -1;   // 当前选中的无人机索引（Selected index），-1表示没选中
static Mode     M       = M_INTRO; // 当前模式（Mode），初始为"初始界面"

static Camera3D Cam;    // 3D相机（Camera3D是raylib的结构体），控制3D视角

/* Setup 模式的表单数据——用于输入新建无人机的位置 */
static char sx[16] = "0";   // Start X：起始X坐标字符串（用字符串是为了方便文字输入框编辑）
static char sy[16] = "0";   // Start Y：起始Y坐标（高度）
static char sz[16] = "0";   // Start Z：起始Z坐标
static int  ic     = 0;     // 当前选中的颜色索引（Initial Color），0=红,1=绿,2=蓝

/* Edit 模式的表单数据——用于输入新增路径点的坐标 */
static char wx[16] = "0";   // Waypoint X：路径点X坐标
static char wy[16] = "0";   // Waypoint Y：路径点Y坐标（高度）
static char wz[16] = "0";   // Waypoint Z：路径点Z坐标

/* Show 模式——播放控制 */
static bool  play = false;  // 是否正在播放动画
static bool  pause = false; // 是否暂停播放
static float spd  = 2;      // 播放速度倍数（speed），值越大飞得越快
static float prog = 0;      // 播放进度（progress），0.0=开始, 1.0=完成

/* 消息系统——用于在屏幕顶部显示提示文字 */
static char  msg[256] = ""; // 消息文本缓冲区
static float mt       = 0;  // 消息剩余显示时间（message timer），>0时显示，每秒递减

static int   txtFocus = 0;  // 是否有文字输入框正在接收键盘输入（0=没有, 1=有）

/* ============================ UI 颜色常量 ============================ */
/*
 * 下面定义了一系列颜色，用于绘制UI界面。
 * {R, G, B, A} 格式，A=Alpha(透明度)，255=完全不透明。
 */

static Color Wh = {255, 255, 255, 255};  // White    - 白色（用于文字）
static Color Gr = {150, 150, 165, 255};  // Gray     - 灰色（用于次要文字）
static Color Bl = {80,  150, 255, 255};  // Blue     - 蓝色（用于强调、按钮高亮）
static Color Gn = {80,  220, 120, 255};  // Green    - 绿色（用于成功/确认按钮）
static Color Rd = {255, 80,  80,  255};  // Red      - 红色（用于危险/删除按钮）
static Color Ye = {255, 210, 60,  255};  // Yellow   - 黄色（用于警告/高亮）

static Color Bg = {22,  24,  34,  255};  // Background   - 3D场景背景色（深蓝黑）
static Color Pn = {32,  34,  46,  245};  // Panel        - 右侧UI面板背景色
static Color Br = {55,  55,  75,  255};  // Border       - 边框颜色（深灰蓝）
static Color Bt = {45,  45,  60,  255};  // Button       - 按钮默认颜色


/* ================================================================
 *                        工具函数（Utility）
 * ================================================================ */

/*
 * Msg() - 显示一条状态消息
 *
 * 用法和 printf 一样：Msg("创建了 %s", name);
 * 消息会在屏幕顶部显示约2.5秒后自动消失。
 *
 * 参数:
 *   f   - 格式化字符串（类似 printf 的第一个参数）
 *   ... - 可变参数，填入格式化字符串中 %s/%d/%f 等占位符的值
 *
 * 涉及的C语言知识点：
 *   va_list / va_start / va_end 是C语言处理可变参数的标准方式。
 *   vsnprintf 是 sprintf 的安全版本，限制最大写入长度防止溢出。
 */
static void Msg(const char* f, ...) {
    va_list a;                                  // 声明一个可变参数列表变量
    va_start(a, f);                             // 初始化：让 a 指向 f 后面的第一个可变参数
    vsnprintf(msg, sizeof(msg), f, a);          // 将格式化后的字符串写入 msg 缓冲区（最多256字节）
    va_end(a);                                  // 清理可变参数列表（必须调用，与va_start配对）
    mt = 2.5f;                                  // 设置消息显示倒计时为2.5秒（之后每帧递减，减到0消失）
}

/*
 * In() - 判断鼠标是否在一个矩形区域内
 *
 * 这是UI交互的核心函数——用来判断用户是否把鼠标移到了某个按钮/输入框上。
 *
 * 返回: 1（true，鼠标在里面） 或 0（false，鼠标不在里面）
 *
 * 参数:
 *   r - 一个 Rectangle 结构体（raylib定义），包含 x, y, width, height
 */
static int In(Rectangle r) {
    Vector2 m = GetMousePosition();             // 获取鼠标当前的屏幕坐标（Vector2包含x和y）
    /* 判断鼠标的x坐标是否在矩形的左右边界之间，
       且鼠标的y坐标是否在矩形的上下边界之间 */
    return m.x >= r.x            // 鼠标X ≥ 矩形左边界
        && m.x <= r.x + r.width  // 鼠标X ≤ 矩形右边界（左边界+宽度）
        && m.y >= r.y            // 鼠标Y ≥ 矩形上边界
        && m.y <= r.y + r.height;// 鼠标Y ≤ 矩形下边界（上边界+高度）
}

/*
 * Btn() - 绘制一个按钮，并检测是否被点击
 *
 * 这个函数做两件事：
 *   1. 在屏幕上画一个按钮（矩形背景 + 边框 + 文字）
 *   2. 检测鼠标是否点击了这个按钮
 *
 * 返回: 1（被点击了） 或 0（没被点击）
 *
 * 参数:
 *   r - 按钮的矩形区域
 *   t - 按钮上显示的文字
 *   c - 按钮的背景颜色
 */
static int Btn(Rectangle r, const char* t, Color c) {
    DrawRectangleRec(r, c);                     // 画一个填充矩形作为按钮背景
    DrawRectangleLinesEx(r, 1, Br);             // 画矩形的边框线（1像素宽，深灰色）
    int tw = MeasureText(t, 16);                // 测量文字的像素宽度（字号16），用于居中
    /* 画按钮文字：x = 矩形中心 - 文字宽度的一半，实现水平居中；
       y = 矩形中心 - 字号的一半（8），实现垂直居中 */
    DrawText(t, (int)(r.x + r.width / 2 - tw / 2),
                (int)(r.y + r.height / 2 - 8), 16, Wh);
    /* 返回是否被点击：鼠标在矩形内  并且  鼠标左键刚被按下 */
    return In(r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/*
 * Txt() - 文字输入框
 *
 * 这是整个程序中最复杂的UI组件。它：
 *   1. 绘制输入框（标签 + 背景 + 边框 + 文字内容）
 *   2. 处理鼠标点击切换激活状态
 *   3. 处理键盘输入（打字、退格、负号、小数点）
 *   4. 绘制闪烁光标
 *
 * 返回: 1（内容发生了变化） 或 0（没变化）
 *
 * 参数:
 *   r     - 输入框的矩形区域
 *   buf   - 存储输入内容的字符数组
 *   max   - buf 的最大容量
 *   label - 输入框上方显示的标签文字
 */
static int Txt(Rectangle r, char* buf, int max, const char* label) {
    DrawText(label, (int)r.x, (int)(r.y - 14), 12, Gr);  // 在输入框上方画标签文字（灰色，字号12）

    Color bc = Br;                          // 默认边框颜色 = 灰色
    if (In(r)) bc = Bl;                     // 如果鼠标悬停在上面，边框变蓝色（hover效果）

    DrawRectangleRec(r, (Color){20, 22, 32, 255});  // 画输入框背景（深色）
    DrawRectangleLinesEx(r, 1.5f, bc);              // 画输入框边框（1.5像素宽）
    DrawText(buf, (int)(r.x + 4), (int)(r.y + r.height / 2 - 8), 16, Wh); // 画已输入的文字

    /* ---- 焦点管理：决定哪个输入框接收键盘输入 ---- */
    static int activeIdx = -1;              // static变量在函数调用之间保持值不变
                                            // -1 表示没有输入框被激活

    /* 用 buf 的地址生成一个唯一ID，用于区分不同的输入框 */
    int id = (int)((long long)buf & 0xFFF); // 取 buf 地址的低12位作为ID

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {  // 如果鼠标左键被按下
        if (In(r))                          // 如果鼠标在当前输入框内 → 激活当前输入框
            activeIdx = id;
        else if (activeIdx == id)           // 如果鼠标在外部且当前输入框是激活的 → 取消激活
            activeIdx = -1;
    }

    if (activeIdx == id) txtFocus = 1;      // 标记"有输入框正在接收键盘输入"
                                            // 这个标记用于阻止快捷键干扰文字输入

    /* ---- 键盘输入处理（仅当本输入框被激活时） ---- */
    if (activeIdx == id) {
        int changed = 0;                    // 标记内容是否发生变化

        /* 处理普通字符输入 */
        int key = GetCharPressed();         // 获取按下的字符（raylib函数，返回Unicode码点）
        while (key > 0) {                   // 只要还有待处理的按键（可能一帧内按了多个键）
            if (key >= 32 && key <= 126) {  // 只接受可打印的ASCII字符（空格~波浪号）
                int len = (int)strlen(buf); // 获取当前字符串长度
                if (len < max - 1) {        // 如果还有空间（留一个位置给结尾的'\0'）
                    buf[len] = (char)key;   // 把新字符追加到末尾
                    buf[len + 1] = 0;       // 添加字符串结束符 '\0'（C字符串必须以0结尾）
                    changed = 1;            // 标记内容已变化
                }
            }
            key = GetCharPressed();         // 继续获取下一个按键
        }

        /* 处理退格键——删除最后一个字符 */
        if (IsKeyPressed(KEY_BACKSPACE)) {  // 如果按下退格键
            int len = (int)strlen(buf);     // 获取当前字符串长度
            if (len > 0) {                  // 如果至少有一个字符
                buf[len - 1] = 0;           // 把最后一个字符替换为'\0'（相当于删除）
                changed = 1;
            }
        }

        /* 处理负号键——在开头输入负号（仅当输入框为空时） */
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
            int len = (int)strlen(buf);
            if (len == 0) {                 // 只有输入框为空时才能输入负号
                buf[0] = '-';               // 第一个字符设为负号
                buf[1] = 0;                 // 第二个字符设为结束符
                changed = 1;
            }
        }

        /* 处理小数点键——输入小数点（仅当还没有小数点时） */
        if (IsKeyPressed(KEY_PERIOD) || IsKeyPressed(KEY_KP_DECIMAL)) {
            int len = (int)strlen(buf);
            if (len < max - 1 && !strchr(buf, '.')) {  // 有空间 且 还没有小数点
                buf[len] = '.';             // 在末尾添加小数点
                buf[len + 1] = 0;           // 添加结束符
                changed = 1;
            }
        }

        /* ---- 绘制闪烁光标 ---- */
        /* GetTime()返回程序运行的秒数，乘以2使光标每秒闪烁2次，
           取整后对2取余，结果在0和1之间交替 */
        if (((int)(GetTime() * 2) % 2) == 0) {  // 偶数秒时显示光标
            int tw = MeasureText(buf, 16);       // 测量当前文字的像素宽度
            DrawText("|", (int)(r.x + 5 + tw),   // 光标位置 = 输入框左边距 + 文字宽度
                         (int)(r.y + r.height / 2 - 8), 16, Bl);  // 画竖线作为光标（蓝色）
        }

        return changed;                     // 返回内容是否变化
    }
    return 0;                               // 输入框未激活，返回无变化
}

/*
 * Sep() - 画一条水平分隔线
 *
 * 参数:
 *   x, y - 分隔线起点的屏幕坐标
 *   w    - 分隔线的像素宽度
 */
static void Sep(int x, int y, int w) {
    DrawRectangle(x, y, w, 1, Br);          // 画一个高1像素的矩形 = 一条横线（深灰色）
}

/*
 * Sld() - 滑块控件（声明/原型，具体实现在文件末尾）
 *
 * 参数:
 *   r  - 滑块的矩形区域
 *   v  - 当前值
 *   lo - 最小值
 *   hi - 最大值
 *   f  - 显示格式（如 "Speed: %.1fx"）
 * 返回: 用户调整后的新值
 */
static float Sld(Rectangle r, float v, float lo, float hi, const char* f);


/* ================================================================
 *                      无人机操作（Drone Operations）
 * ================================================================ */

/*
 * MakeDrone() - 创建一架新的无人机
 *
 * 从全局变量 sx, sy, sz（字符串）读取位置坐标，
 * 用 atof() 将字符串转为浮点数，然后在 D[N] 位置初始化一架新无人机。
 * 创建成功后自动选中它，并切换到编辑模式。
 */
static void MakeDrone(void) {
    /* 检查数量上限 */
    if (N >= MAX_DRONES) {                  // 如果已达到最大数量
        Msg("Max %d drones!", MAX_DRONES);  // 显示提示消息
        return;                             // 直接返回，不创建
    }

    /* 将字符串坐标转为浮点数（atof = ASCII to Float） */
    float px = (float)atof(sx);             // 把sx字符串（如"3.5"）转成浮点数3.5
    float py = (float)atof(sy);             // 把sy字符串转成高度值
    float pz = (float)atof(sz);             // 把sz字符串转成Z坐标

    /* 限制高度范围：最低0.5米，最高30米 */
    if (py < 0.5f) py = 0.5f;              // 太低不安全，至少离地0.5米
    if (py > 30)   py = 30;                // 太高看不到，最多30米

    Drone* d = &D[N];                       // 获取第N个无人机槽位的指针（d指向D[N]）
    memset(d, 0, sizeof(Drone));            // 将该槽位的所有内存清零（安全初始化）
    d->act   = 1;                           // 标记为激活状态
    d->light = L_ON;                        // 默认灯光常亮
    d->color = ic;                          // 使用用户在UI中选择的颜色编号
    d->bon   = 1;                           // 闪烁初始状态为"亮"
    d->h     = py;                          // 保存高度值

    d->start = (Pt){px, py, pz};            // 设置起始位置（复合字面量语法，一次性赋值xyz）
    d->pos   = d->start;                    // 当前位置也设为起始位置

    snprintf(d->name, MAX_NAME, "D-%d", N + 1); // 生成默认名称，如"D-1"、"D-2"
    N++;                                    // 无人机总数+1
    S = N - 1;                              // 自动选中最新创建的无人机（S=索引=N-1）

    Msg("Created %s (%.0f,%.0f,%.0f) [%s]", // 显示创建成功的消息
        d->name, px, py, pz, LCN[ic]);      // 格式：Created D-1 (3,5,0) [Red]
}

/*
 * DelDrone() - 删除第i架无人机
 *
 * 使用"向前移动覆盖"的方式删除数组中的元素：
 * 把后面的所有元素依次往前挪一位，然后总数减一。
 *
 * 参数:
 *   i - 要删除的无人机索引（0 ~ N-1）
 */
static void DelDrone(int i) {
    if (i < 0 || i >= N) return;            // 安全检查：索引越界则什么也不做

    /* 从位置i开始，把后面的每个元素复制到前一个位置 */
    for (int j = i; j < N - 1; j++)
        D[j] = D[j + 1];                    // 第j+1个覆盖第j个

    N--;                                    // 总数减一

    /* 如果之前选中的无人机被删了，选中索引也要调整 */
    if (S >= N) S = N - 1;                  // 如果S超出范围，改为选中最后一架
}


/* ================================================================
 *                      回放系统（Playback）
 * ================================================================ */

/*
 * Rst() - 重置回放（Reset）
 *
 * 把所有无人机移回起始位置，重置路径点进度，
 * 停止播放，进度归零。
 */
static void Rst(void) {
    for (int i = 0; i < N; i++) {           // 遍历所有无人机
        D[i].pos = D[i].start;              // 位置回到起点
        D[i].ci  = 0;                       // 路径点索引归零（从第一个路径点开始）
        D[i].fin = 0;                       // 标记为"未完成"
    }
    play  = false;                          // 停止播放
    pause = false;                          // 取消暂停
    prog  = 0;                              // 播放进度归零
}

/*
 * Upd() - 更新回放动画（Update playback）
 *
 * 每帧调用一次，根据时间间隔 dt 移动所有无人机向它们的下一个路径点前进。
 * 这是播放模式的核心逻辑——让无人机沿着预设的路径点飞行。
 *
 * 参数:
 *   dt - 上一帧到这一帧的时间间隔（秒），用于保证不同帧率下飞行速度一致
 */
static void Upd(float dt) {
    if (!play || pause) return;             // 如果没在播放或暂停了，直接返回不更新

    int done = 0;                           // 计数器：已经飞完所有路径的无人机数量

    /* 更新每架无人机的位置 */
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];                   // 获取第i架无人机的指针

        /* 跳过不激活或已飞完的无人机 */
        if (!d->act || d->fin) {            // 如果无人机不存在 或 已经飞完全部路径点
            done++;                         // 计入"完成"计数
            continue;                       // 跳过这架，处理下一架
        }

        /* 如果所有路径点都飞完了，标记完成 */
        if (d->ci >= d->wc) {               // 当前路径索引 ≥ 路径点总数
            d->fin = 1;                     // 标记为完成
            done++;
            continue;
        }

        /* 获取目标路径点的坐标 */
        Pt t = d->wp[d->ci].p;              // t = 当前要飞向的那个路径点坐标

        /* 计算当前位置到目标点的向量差（dx, dy, dz） */
        float dx = t.x - d->pos.x;          // X方向的距离（目标 - 当前）
        float dy = t.y - d->pos.y;          // Y方向的距离（高度差）
        float dz = t.z - d->pos.z;          // Z方向的距离

        /* 计算到目标点的直线距离（三维勾股定理/欧几里得距离） */
        float ds = sqrtf(dx * dx + dy * dy + dz * dz);

        /* 如果已经非常接近目标点（小于0.15米），视为到达 */
        if (ds < 0.15f) {
            d->ci++;                        // 切换到下一个路径点
            if (d->ci >= d->wc) d->fin = 1; // 如果这是最后一个路径点，标记完成
        } else {
            /* 还没到达，继续向目标移动 */
            float step = spd * dt * 3;      // 本帧移动的距离 = 速度 × 时间 × 3倍系数
            if (step > ds) step = ds;       // 如果步长超过剩余距离，只移动剩余距离（防止飞过头）

            /* 沿方向向量移动：dx/ds 是X方向的单位分量，乘以step得到实际位移 */
            d->pos.x += dx / ds * step;     // X方向位移 = 单位方向分量 × 步长
            d->pos.y += dy / ds * step;     // Y方向位移
            d->pos.z += dz / ds * step;     // Z方向位移
        }
    }

    /* 计算播放进度百分比 */
    int total = 0, cur = 0;                 // total=总路径点数, cur=已完成的路径点数
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;            // 跳过不存在的无人机
        total += D[i].wc > 0 ? D[i].wc : 1; // 至少算1个（没有路径点的也算1个单位）
        if (D[i].fin)
            cur += D[i].wc > 0 ? D[i].wc : 1; // 已完成的加上全部路径点数
        else
            cur += D[i].ci;                  // 未完成的加上已到达的路径点索引
    }
    prog = total > 0 ? (float)cur / total : 0; // 进度 = 已完成 / 总数（防止除零）

    /* 如果所有无人机都完成了，结束播放 */
    if (done >= N) {
        play = 0;                           // 停止播放
        Msg("Show finished!");              // 显示完成提示
    }
}


/* ================================================================
 *                    3D 渲染（3D Rendering）
 * ================================================================ */

/*
 * RC() - 获取无人机的渲染颜色（Render Color）
 *
 * 根据无人机的灯光模式、颜色编号和选中状态，计算它在3D场景中应该显示的颜色。
 *
 * 参数:
 *   d - 无人机指针
 * 返回: 计算后的 Color 结构体
 */
static Color RC(Drone* d) {
    Color b;                                // 声明返回的颜色变量

    switch (d->light) {                     // 根据灯光模式决定基础颜色
        case L_OFF:
            b = (Color){50, 50, 60, 255};   // 灯灭 → 暗灰色（像关灯了）
            break;
        case L_ON:
            b = LC[d->color];               // 灯亮 → 使用无人机设定的颜色（红/绿/蓝）
            break;
        case L_BLINK:
            /* 闪烁模式：bon为true时显示灯光颜色，false时显示暗色 */
            b = d->bon ? LC[d->color] : (Color){35, 35, 45, 255};
            break;
        default:
            b = GRAY;                       // 未知模式 → 灰色（兜底）
    }

    /* 如果无人机被选中，颜色加亮30%（每个分量乘以1.3，但不超过255） */
    if (d->sel) {
        b.r = (unsigned char)(b.r * 1.3f > 255 ? 255 : b.r * 1.3f);  // 红色分量加亮
        b.g = (unsigned char)(b.g * 1.3f > 255 ? 255 : b.g * 1.3f);  // 绿色分量加亮
        b.b = (unsigned char)(b.b * 1.3f > 255 ? 255 : b.b * 1.3f);  // 蓝色分量加亮
        /* 三元运算符 ?:  条件 ? 真时的值 : 假时的值
           即：如果加亮后超过255，就取255（上限），否则取加亮后的值 */
    }

    return b;                               // 返回最终颜色
}

/*
 * DD() - 在3D场景中绘制一架无人机（Draw Drone）
 *
 * 绘制内容包括：
 *   - 发光球体（主灯光）
 *   - 半透明光晕（两层，模拟灯光散射）
 *   - 暗色核心（无人机机身）
 *   - 选中时的绿色高亮环
 *   - 编辑模式下的路径点和路径线（黄色）
 *
 * 参数:
 *   d - 要绘制的无人机指针
 */
static void DD(Drone* d) {
    if (!d->act) return;                    // 不激活的无人机不画

    Pt    p = d->pos;                       // 获取无人机当前位置
    Color c = RC(d);                        // 获取计算后的渲染颜色

    /* 主灯光球（半径1.3倍无人机半径，使用灯光颜色） */
    DrawSphere((Vector3){p.x, p.y, p.z}, DR * 1.3f, c);

    /* 第一层光晕（半径1.9倍，30%透明度）——模拟灯光在空气中的散射 */
    DrawSphere((Vector3){p.x, p.y, p.z}, DR * 1.9f, Fade(c, 0.3f));
    /* Fade(color, alpha) 是raylib函数，返回一个调整了透明度的颜色副本 */

    /* 无人机机身核心（半径0.5倍，深灰色小球）——代表无人机本体 */
    DrawSphere((Vector3){p.x, p.y, p.z}, DR * 0.5f, (Color){28, 28, 36, 255});

    /* 如果被选中，画一个绿色的高亮环围绕无人机 */
    if (d->sel)
        DrawCircle3D((Vector3){p.x, p.y, p.z},    // 环心位置
                     DR * 2.0f,                    // 环的半径
                     (Vector3){0, 1, 0},           // 环的法线方向（0,1,0=水平环，Y轴朝上）
                     0,                            // 旋转角度（0=不旋转）
                     Bl);                          // 环的颜色（蓝色）

    /* 编辑模式下：绘制路径点和路径连线 */
    if (M == M_EDIT && d->wc > 0) {         // 只在编辑模式且有路径点时绘制
        for (int i = 0; i < d->wc; i++) {
            /* 画路径点小球（黄色，半径0.7倍无人机半径） */
            DrawSphere((Vector3){d->wp[i].p.x, d->wp[i].p.y, d->wp[i].p.z},
                       DR * 0.7f, Ye);

            /* 画从上一个路径点到当前路径点的连线 */
            Pt pr = (i == 0) ? d->start : d->wp[i - 1].p;  // 上一个点：第0个路径点的"上一个"是起点
            DrawLine3D((Vector3){pr.x, pr.y, pr.z},         // 线段起点
                       (Vector3){d->wp[i].p.x, d->wp[i].p.y, d->wp[i].p.z}, // 线段终点
                       Fade(Ye, 0.5f));                     // 线段颜色（黄色半透明）
        }
    }
}

/*
 * Draw3D() - 绘制整个3D场景
 *
 * 包括：
 *   - 地面网格（1/4象限，X≥0, Z≥0）
 *   - 坐标轴（红=X, 绿=Y, 蓝=Z）
 *   - 原点标记
 *   - 所有无人机
 */
static void Draw3D(void) {
    BeginMode3D(Cam);                       // 进入3D渲染模式（raylib函数，使用全局相机Cam）

    /* ---- 地面 ---- */
    float S = GROUND;                       // S = 地面边长（40米）
    /* 画一个半透明的地面平面，中心在(S/2, 0, S/2)，
       即第一象限（X≥0, Z≥0）的区域 */
    DrawPlane((Vector3){S / 2, -0.01f, S / 2},  // 平面中心点
              (Vector2){S, S},                   // 平面的宽和高
              (Color){35, 40, 50, 90});          // 半透明深色（Alpha=90）

    /* ---- 地面网格线（20x20的格子） ---- */
    for (int i = 0; i <= 20; i++) {
        float v = i * S / 20;               // v = 当前网格线的位置（从0到S，共21条线）

        /* 沿X方向的网格线（平行于X轴，在不同Z值处） */
        DrawLine3D((Vector3){v, 0, 0},      // 起点(X=v, Y=0, Z=0)
                   (Vector3){v, 0, S},      // 终点(X=v, Y=0, Z=S)
                   Fade(Gr, 0.25f));         // 灰色半透明

        /* 沿Z方向的网格线（平行于Z轴，在不同X值处） */
        DrawLine3D((Vector3){0, 0, v},      // 起点(X=0, Y=0, Z=v)
                   (Vector3){S, 0, v},      // 终点(X=S, Y=0, Z=v)
                   Fade(Gr, 0.25f));         // 灰色半透明
    }

    /* ---- 原点标记（小红球） ---- */
    DrawSphere((Vector3){0, 0.02f, 0}, 0.25f, Rd);  // 在原点画一个红色小球

    /* ---- 三色坐标轴 ---- */
    /* X、Z 轴平行于地面网格；Y 轴垂直于网格（向上，即高度方向） */
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){S, 0, 0}, Rd);  // X轴=红色（平行网格）
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, S, 0}, Bl);  // Y轴=蓝色（垂直网格）
    DrawLine3D((Vector3){0, 0, 0}, (Vector3){0, 0, S}, Gn);  // Z轴=绿色（平行网格）

    /* ---- 绘制所有无人机 ---- */
    for (int i = 0; i < N; i++)
        DD(&D[i]);                          // 调用DD函数画每一架无人机

    EndMode3D();                            // 退出3D渲染模式（必须与BeginMode3D配对）

    /* ---- 坐标轴标签（大写 X / Y / Z） ---- */
    float L = S + 2.0f;                     // 标签位置：略超出轴末端
    Vector2 ptx = GetWorldToScreen((Vector3){L, 0, 0}, Cam);   // X轴末端的屏幕位置
    Vector2 pty = GetWorldToScreen((Vector3){0, L, 0}, Cam);   // Y轴末端的屏幕位置
    Vector2 ptz = GetWorldToScreen((Vector3){0, 0, L}, Cam);   // Z轴末端的屏幕位置
    DrawText("X", (int)ptx.x, (int)ptx.y, 16, Rd);   // X标签（红色）
    DrawText("Y", (int)pty.x, (int)pty.y, 16, Bl);   // Y标签（蓝色）
    DrawText("Z", (int)ptz.x, (int)ptz.y, 16, Gn);   // Z标签（绿色）
}

/*
 * Pick() - 3D鼠标拾取：检测用户点击了哪架无人机
 *
 * 原理：从鼠标位置发出一条射线（Ray），检查射线是否穿过无人机的包围球。
 *       找到距离相机最近的那架被点击的无人机。
 *
 * 返回: 被点击的无人机索引，-1表示没有点击到任何无人机
 */
static int Pick(void) {
    Vector2 m = GetMousePosition();         // 获取鼠标屏幕坐标

    /* 如果鼠标在右侧UI面板上，不进行3D拾取（防止点UI时误选无人机） */
    if (m.x > GetScreenWidth() - PW)        // 屏幕宽度减去面板宽度 = 3D视图的右边界
        return -1;

    /* 只在鼠标左键刚按下时检测 */
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return -1;

    /* 从屏幕坐标发射一条射线进入3D世界 */
    Ray r = GetMouseRay(m, Cam);            // raylib函数：屏幕坐标 → 3D射线

    float b = 3.5f;                         // 记录最近的距离，初始设一个较大的值
    int   h = -1;                           // 记录最近的无人机索引，-1=没找到

    /* 遍历所有无人机，检测射线与哪个的包围球相交 */
    for (int i = 0; i < N; i++) {
        if (!D[i].act) continue;            // 跳过不存在的无人机

        /* 检测射线与无人机包围球的碰撞 */
        RayCollision rc = GetRayCollisionSphere(r,
            (Vector3){D[i].pos.x, D[i].pos.y, D[i].pos.z},  // 球的中心（无人机位置）
            DR * 4);                                         // 球的半径（放大4倍方便点击）

        /* 如果射线碰到了这个球，且距离比之前找到的更近 */
        if (rc.hit && rc.distance < b) {
            b = rc.distance;                // 更新最近距离
            h = i;                          // 更新最近的无人机索引
        }
    }

    return h;                               // 返回找到的无人机索引（或-1）
}


/* ================================================================
 *                      UI 绘制（User Interface）
 * ================================================================ */

/*
 * 下面的宏定义简化了右侧面板的坐标计算。
 * PX = Panel X（面板的左边界X坐标）
 * X  = 面板内容区的X坐标（面板左边距+10像素边距）
 * W  = 面板内容区的宽度（面板宽度-20像素边距）
 */
#define PX (GetScreenWidth() - PW)          // 面板左边界 = 屏幕宽度 - 面板宽度
#define X  (PX + 10)                        // 内容起始X = 面板左边界 + 10像素左边距
#define W  (PW - 20)                        // 内容宽度 = 面板宽度 - 20像素（左右各10）

/*
 * DrawUI() - 绘制整个右侧UI面板
 *
 * 这是程序中最长的函数，负责绘制三种模式下的所有UI元素。
 * 虽然很长，但结构清晰——按照三种模式分成三大块。
 */
static void DrawUI(void) {
    int sh = GetScreenHeight();             // 屏幕高度（像素）
    int px = PX;                            // 面板左边界
    int x  = X;                             // 内容区X坐标
    int w  = W;                             // 内容区宽度
    int y  = 8;                             // 当前绘制的Y坐标（从上往下画）

    /* ---- 面板背景 ---- */
    DrawRectangle(px, 0, PW, sh, Pn);       // 画面板背景矩形（深色半透明）
    DrawLine(px, 0, px, sh, Br);            // 画面板左边框线（分隔3D视图和UI面板）

    /* ---- 标题 ---- */
    DrawText("Drone Light Show", x, y, 16, Bl);  // 面板标题（蓝色，字号16）
    y += 22;                                // 标题后下移22像素

    /* ---- 模式切换标签栏（三个按钮） ---- */
    const char* ms[]  = {"1.Setup", "2.Edit", "3.Show"};  // 三个标签的文字
    const Mode  mm[]  = {M_SETUP, M_EDIT, M_SHOW};        // 对应的模式枚举值
    Color       mc[]  = {Bl, Gn, Ye};       // 对应的颜色（蓝、绿、黄）
    float       bw    = (w - 10) / 3.0f;    // 每个标签的宽度 = (总宽-间距)÷3

    for (int i = 0; i < 3; i++) {
        Color bg = (M == mm[i]) ? mc[i] : Bt;  // 当前模式用彩色，其他用暗色
        if (Btn((Rectangle){x + i * (bw + 4), (float)y, bw, 26}, ms[i], bg)) {
            M = mm[i];                      // 切换到对应的模式
            if (M == M_SHOW) Rst();         // 进入播放模式时自动重置
        }
    }
    y += 34;                                // 标签栏后下移
    Sep(x, y, w);                           // 画分隔线
    y += 8;

    /* ==================== SETUP 模式 ==================== */
    if (M == M_SETUP) {
        DrawText("[ Setup ] Create Drones", x, y, 14, Bl);  // 区块标题
        y += 18;

        /* 位置输入区 */
        DrawText("Position:", x, y, 12, Gr);                // 标签：Position
        y += 14;

        /* X坐标输入 */
        DrawText("X", x, y + 2, 20, Rd);                    // 标签"X"（红色）
        Txt((Rectangle){x + 16, (float)y, 60, 24}, sx, 15, "");  // X输入框

        /* Y坐标输入 */
        DrawText("Y", x + 82, y + 2, 20, Gn);               // 标签"Y"（绿色）
        Txt((Rectangle){x + 96, (float)y, 60, 24}, sy, 15, "");  // Y输入框

        /* Z坐标输入 */
        DrawText("Z", x + 164, y + 2, 20, Bl);              // 标签"Z"（蓝色）
        Txt((Rectangle){x + 178, (float)y, 60, 24}, sz, 15, ""); // Z输入框
        y += 30;

        /* 颜色选择区 */
        DrawText("Color:", x, y, 12, Gr);                   // 标签：Color
        for (int i = 0; i < 3; i++) {
            Rectangle cr = {x + 46 + i * 52.0f, (float)y - 1, 48, 20};  // 颜色按钮
            DrawRectangleRec(cr, LC[i]);                    // 画颜色块
            if (ic == i)
                DrawRectangleLinesEx(cr, 2.5f, Wh);        // 选中的颜色加粗白色边框
            else
                DrawRectangleLinesEx(cr, 1, Br);           // 未选中的用普通边框
            DrawText(LCN[i], (int)cr.x + 4, (int)cr.y + 2, 12, Wh);  // 颜色名称
            if (In(cr) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                ic = i;                                     // 点击切换颜色
        }
        y += 24;

        /* 创建按钮 */
        if (Btn((Rectangle){x, (float)y, w, 26}, "+ Create Drone", Gn))
            MakeDrone();                                    // 点击创建无人机
        y += 30;
        Sep(x, y, w);                                       // 分隔线
        y += 6;

        /* 已创建无人机列表 */
        DrawText(TextFormat("Drones: %d", N), x, y, 13, Ye); // "Drones: 5"（黄色）
        y += 16;

        for (int i = 0; i < N; i++) {
            Drone* d = &D[i];

            /* 画一个小色块表示无人机颜色 */
            Color lc = LC[d->color];
            DrawRectangle(x + 4, (int)y + 2, 10, 10, lc);
            DrawRectangleLines(x + 4, (int)y + 2, 10, 10, Wh);

            /* 画无人机信息文字 */
            DrawText(TextFormat("#%d %s (%.0f,%.0f,%.0f)",
                i + 1, d->name, d->start.x, d->start.y, d->start.z),
                x + 18, y, 12, d->sel ? Wh : Gr);          // 选中的白色，未选中的灰色

            /* 点击某一行 → 选中该无人机并切换到编辑模式 */
            Rectangle row = {x, (float)y, w, 15};
            if (In(row) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (S >= 0) D[S].sel = 0;                   // 取消之前的选中状态
                S = i;                                      // 更新选中索引
                d->sel = 1;                                 // 标记当前为选中
                M = M_EDIT;                                 // 自动切换到编辑模式
            }
            y += 15;                                        // 每行高度15像素
        }

        /* 如果没有无人机，显示提示 */
        if (N == 0) {
            DrawText("No drones yet.", x, y, 12, Gr);
            y += 15;
        }

        Sep(x, y, w);                                       // 分隔线
        y += 6;

        /* 底部：跳转到编辑模式的按钮 */
        if (N > 0 && Btn((Rectangle){x, (float)y, w, 22}, "-> Continue to Edit", Bl))
            M = M_EDIT;
    }

    /* ==================== EDIT 模式 ==================== */
    else if (M == M_EDIT) {
        DrawText("[ Edit ] Light & Trajectory", x, y, 14, Gn);  // 区块标题（绿色）
        y += 18;

        /* 检查是否有选中的无人机 */
        if (S >= 0 && S < N && D[S].act) {
            Drone* d = &D[S];                               // 获取选中无人机的指针

            DrawText(TextFormat("Selected: %s", d->name), x, y, 13, Wh);  // 显示名称
            y += 17;

            /* ---- 灯光模式切换（三个按钮） ---- */
            float lw = (w - 8) / 3.0f;                      // 每个按钮宽度

            /* OFF按钮：灯灭 */
            if (Btn((Rectangle){x, (float)y, lw, 22}, "OFF",
                    d->light == L_OFF ? (Color){100, 100, 110, 255} : Bt))
                d->light = L_OFF;

            /* ON按钮：常亮 */
            if (Btn((Rectangle){x + lw + 3, (float)y, lw, 22}, "ON",
                    d->light == L_ON ? Gn : Bt))
                d->light = L_ON;

            /* Blink按钮：闪烁 */
            if (Btn((Rectangle){x + 2 * (lw + 3), (float)y, lw, 22}, "Blink",
                    d->light == L_BLINK ? Ye : Bt))
                d->light = L_BLINK;

            y += 26;
            Sep(x, y, w);                                   // 分隔线
            y += 6;

            /* ---- 添加路径点 ---- */
            DrawText("Add Waypoint:", x, y, 12, Gr);        // 标签
            y += 14;

            /* 路径点X/Y/Z输入框 */
            DrawText("X", x,         y + 2, 20, Rd);
            Txt((Rectangle){x + 16,  (float)y, 60, 24}, wx, 15, "");
            DrawText("Y", x + 82,    y + 2, 20, Gn);
            Txt((Rectangle){x + 96,  (float)y, 60, 24}, wy, 15, "");
            DrawText("Z", x + 164,   y + 2, 20, Bl);
            Txt((Rectangle){x + 178, (float)y, 60, 24}, wz, 15, "");
            y += 30;

            /* "添加路径点"按钮 */
            if (Btn((Rectangle){x, (float)y, w, 24}, "+ Add Waypoint", Bl)) {
                if (d->wc < MAX_WP) {                       // 检查是否达到上限
                    float px = (float)atof(wx);             // 字符串转浮点数
                    float py = (float)atof(wy);
                    float pz = (float)atof(wz);
                    if (py < 0.5f) py = 0.5f;               // 高度下限
                    if (py > 30)   py = 30;                 // 高度上限
                    d->wp[d->wc].p = (Pt){px, py, pz};     // 设置路径点坐标
                    d->wc++;                                // 路径点数量+1
                } else {
                    Msg("Max waypoints!");                  // 超限提示
                }
            }
            y += 26;

            /* ---- 路径点列表（最多显示6个） ---- */
            DrawText(TextFormat("Waypoints: %d", d->wc), x, y, 12, Gr);
            y += 15;

            for (int i = 0; i < d->wc && i < 6; i++) {     // 最多显示前6个
                /* 显示路径点编号和坐标 */
                DrawText(TextFormat("#%d (%.0f,%.0f,%.0f)",
                    i + 1, d->wp[i].p.x, d->wp[i].p.y, d->wp[i].p.z),
                    x + 4, y, 12, Wh);

                /* 每行右侧的删除按钮（红色X） */
                if (Btn((Rectangle){x + w - 26, (float)y, 22, 15}, "X", Rd)) {
                    /* 删除第i个路径点：把后面的元素前移 */
                    for (int j = i; j < d->wc - 1; j++)
                        d->wp[j] = d->wp[j + 1];
                    d->wc--;                                // 数量-1
                    break;                                  // 跳出循环（因为数组结构变了）
                }
                y += 16;
            }

            /* 如果超过6个路径点，显示省略提示 */
            if (d->wc > 6)
                DrawText("... more ...", x + 4, y, 11, Gr);

            Sep(x, y, w);                                   // 分隔线
            y += 6;

            /* 底部按钮 */
            if (Btn((Rectangle){x, (float)y, w / 2 - 3, 22}, "Delete Drone", Rd))
                DelDrone(S);                                // 删除选中的无人机

            if (Btn((Rectangle){x + w / 2 + 3, (float)y, w / 2 - 3, 22}, "<- Setup", Bt))
                M = M_SETUP;                                // 返回Setup模式

        } else {
            /* 没有选中无人机时的提示 */
            DrawText("No drone selected.", x, y, 12, Gr);
            y += 14;
            DrawText("Click in 3D or go Setup.", x, y, 12, Gr);
            y += 14;
            if (Btn((Rectangle){x, (float)y, w, 22}, "<- Back to Setup", Bt))
                M = M_SETUP;
        }
    }

    /* ==================== SHOW 模式 ==================== */
    else if (M == M_SHOW) {
        DrawText("[ Show ] Playback", x, y, 14, Ye);        // 区块标题（黄色）
        y += 18;

        /* 统计有路径点的无人机数量 */
        int h = 0;
        for (int i = 0; i < N; i++)
            if (D[i].wc > 0) h++;                           // 路径点数>0的才算"就绪"

        DrawText(TextFormat("Ready: %d/%d drones", h, N), x, y, 12, Gr);
        y += 17;

        /* 速度滑块 */
        spd = Sld((Rectangle){x, (float)y, (float)w, 22}, spd, 0.5f, 8, "Speed: %.1fx");
        y += 28;

        /* 播放控制按钮（Play / Pause / Stop） */
        float pw = (w - 8) / 3.0f;                          // 每个按钮宽度

        if (Btn((Rectangle){x, (float)y, pw, 26}, "Play", Gn)) {
            if (!play) { Rst(); play = 1; pause = 0; }      // 首次点击→重置并开始播放
            else       pause = 0;                           // 已播放中→取消暂停
        }

        if (Btn((Rectangle){x + pw + 3, (float)y, pw, 26}, "Pause", Ye))
            pause = 1;                                      // 暂停播放

        if (Btn((Rectangle){x + 2 * (pw + 3), (float)y, pw, 26}, "Stop", Rd))
            Rst();                                          // 停止播放并重置
        y += 32;

        /* 进度条 */
        DrawRectangle(x, y, w, 10, (Color){40, 40, 55, 255});        // 进度条背景（深色）
        DrawRectangle(x, y, (int)(w * prog), 10, Ye);                // 进度条前景（黄色）
        DrawText(TextFormat("%.0f%%", prog * 100), x, y + 14, 12, Gr); // 百分比文字
        y += 26;

        /* 快捷键提示 */
        DrawText("Space=Play/Pause  Esc=Stop", x, y, 11, Gr);
    }
}


/* ================================================================
 *                    键盘快捷键（Keyboard Shortcuts）
 * ================================================================ */

/*
 * Keys() - 处理键盘快捷键
 *
 * 这个函数在每帧被调用，检查是否有特定按键被按下并执行相应操作。
 * 快捷键让操作更高效，不需要用鼠标点来点去。
 */
static void Keys(void) {
    /* Tab键：切换到下一架无人机（循环） */
    if (IsKeyPressed(KEY_TAB) && N > 0) {   // 按Tab且至少有一架无人机
        if (S >= 0) D[S].sel = 0;           // 取消当前选中
        S = (S + 1) % N;                    // 索引+1，用%取余实现循环（到末尾后回到开头）
        D[S].sel = 1;                       // 选中新的
    }

    /* A键：在Setup模式下快速创建无人机（仅当没有文字输入框激活时） */
    if (IsKeyPressed(KEY_A) && M == M_SETUP && !txtFocus)
        MakeDrone();

    /* Delete键：删除选中的无人机 */
    if (IsKeyPressed(KEY_DELETE) && S >= 0)
        DelDrone(S);

    /* 数字键切换灯光模式（仅当有选中无人机且文字输入框未激活时） */
    if (S >= 0 && S < N && !txtFocus) {
        Drone* d = &D[S];

        if (IsKeyPressed(KEY_ONE))   d->light = L_OFF;     // 按1→灯灭
        if (IsKeyPressed(KEY_TWO))   d->light = L_ON;      // 按2→常亮
        if (IsKeyPressed(KEY_THREE)) d->light = L_BLINK;   // 按3→闪烁

        /* 方向键微调无人机位置 */
        /* 按住Shift键时移动速度更快（2倍），否则正常速度（0.5倍） */
        float st = IsKeyDown(KEY_LEFT_SHIFT) ? 2 : 0.5f;   // 移动步长
        float ft = GetFrameTime() * 20;                     // 帧时间×20用于平滑移动

        if (IsKeyDown(KEY_UP))    d->pos.z -= st * ft;      // 上箭头→Z轴负方向（前移）
        if (IsKeyDown(KEY_DOWN))  d->pos.z += st * ft;      // 下箭头→Z轴正方向（后移）
        if (IsKeyDown(KEY_LEFT))  d->pos.x -= st * ft;      // 左箭头→X轴负方向（左移）
        if (IsKeyDown(KEY_RIGHT)) d->pos.x += st * ft;      // 右箭头→X轴正方向（右移）
    }

    /* 初始界面：按任意键进入Setup模式 */
    if (M == M_INTRO) {
        for (int k = 0; k < 512; k++) {     // 遍历所有可能的按键码（0~511）
            if (IsKeyPressed(k)) {
                M = M_SETUP;                // 切换到Setup模式
                break;                      // 找到按键就退出循环
            }
        }
    }

    /* Show模式专用快捷键 */
    if (M == M_SHOW) {
        if (IsKeyPressed(KEY_SPACE)) {      // 空格键：播放/暂停切换
            if (!play) { Rst(); play = 1; } // 还没播放→开始播放
            else pause = !pause;            // 已经在播放→切换暂停状态
        }
        if (IsKeyPressed(KEY_ESCAPE))       // Esc键：停止播放
            Rst();
    }

    /* F键：将相机聚焦到选中的无人机 */
    if (IsKeyPressed(KEY_F) && S >= 0)
        Cam.target = (Vector3){D[S].pos.x, D[S].pos.y, D[S].pos.z};
        // 把相机的"注视点"移到无人机位置，这样画面就会以该无人机为中心

    /* F1/F2/F3：快速切换模式 */
    if (IsKeyPressed(KEY_F1)) M = M_SETUP;  // F1→Setup
    if (IsKeyPressed(KEY_F2)) M = M_EDIT;   // F2→Edit
    if (IsKeyPressed(KEY_F3)) { M = M_SHOW; Rst(); }  // F3→Show（自动重置）
}

/*
 * DrawStartScreen() - 绘制初始欢迎界面
 *
 * 在程序启动时显示，包含标题和操作提示。
 * 按任意键后进入Setup模式。
 */
static void DrawStartScreen(void) {
    int sw = GetScreenWidth();              // 屏幕宽度
    int sh = GetScreenHeight();             // 屏幕高度

    /* 所有文字相对于屏幕中心定位 */
    DrawText("Drone Light Show",                             // 主标题（黄色大字）
             sw / 2 - 260, sh / 2 - 80, 48, Ye);
    DrawText("Drone Formation Light Show Simulator",         // 副标题（白色）
             sw / 2 - 300, sh / 2 - 16, 22, Wh);
    DrawText("Press any key to start",                       // 操作提示（绿色）
             sw / 2 - 140, sh / 2 + 40, 20, Gn);
    DrawText("F1=Setup  F2=Edit  F3=Show",                  // 快捷键提示（灰色）
             sw / 2 - 145, sh / 2 + 90, 16, Gr);
}


/* ================================================================
 *                      更新逻辑（Update）
 * ================================================================ */

/*
 * Update() - 每帧调用一次，更新所有游戏逻辑
 *
 * 包括：
 *   - 重置文字输入焦点标记
 *   - 递减消息计时器
 *   - 更新闪烁无人机的闪烁状态
 *   - 如果在Show模式，更新回放动画
 *
 * 参数:
 *   dt - 帧间隔时间（delta time，单位秒）
 */
static void Update(float dt) {
    txtFocus = 0;                           // 每帧开始时重置焦点标记
                                            // （之后由Txt函数在需要时设为1）

    if (mt > 0) mt -= dt;                   // 消息计时器递减（从2.5秒减到0就消失）

    /* 更新所有无人机的闪烁效果 */
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        if (!d->act) continue;              // 跳过不存在的

        if (d->light == L_BLINK) {          // 只有闪烁模式的无人机需要处理
            d->bt += dt;                    // 累计闪烁计时器
            if (d->bt >= 0.5f) {            // 每0.5秒切换一次亮/灭
                d->bt -= 0.5f;              // 计时器减0.5（保留超出部分，平滑计时）
                d->bon = !d->bon;           // 翻转闪烁状态（亮→灭，灭→亮）
            }
        }
    }

    /* 如果在Show模式，更新回放动画 */
    if (M == M_SHOW) Upd(dt);
}


/* ================================================================
 *                    滑块控件（Slider，Show模式用）
 * ================================================================ */

/*
 * Sld() - 绘制并处理滑块控件
 *
 * 滑块用于调节播放速度。用户拖动滑块可以改变spd变量（0.5x ~ 8x）。
 *
 * 绘制内容：
 *   - 标签文字（如 "Speed: 2.0x"）
 *   - 滑轨（深色长条）
 *   - 已填充部分（蓝色）
 *   - 手柄（白色小方块）
 *
 * 参数:
 *   r  - 滑块的矩形区域
 *   v  - 当前值
 *   lo - 最小值
 *   hi - 最大值
 *   f  - 标签格式字符串（如 "Speed: %.1fx"）
 * 返回: 用户调整后的新值（如果没拖动则返回原值v）
 */
static float Sld(Rectangle r, float v, float lo, float hi, const char* f) {
    /* 画标签文字 */
    DrawText(TextFormat(f, v), (int)r.x, (int)(r.y - 15), 13, Gr);
    /* TextFormat 是 raylib 的格式化函数，类似 sprintf 但返回一个内部缓冲区 */

    /* 画滑轨背景（深色横条，高6像素，在矩形中下部） */
    DrawRectangleRec((Rectangle){r.x, r.y + r.height / 2 - 3, r.width, 6},
                     (Color){50, 50, 65, 255});

    /* 计算手柄位置 */
    float t  = (v - lo) / (hi - lo);        // 将当前值映射到0~1范围（归一化）
    float hx = r.x + t * r.width;           // 手柄的X坐标 = 轨道起点 + 比例×轨道宽度

    /* 画已填充部分（蓝色，从轨道起点到手柄位置） */
    DrawRectangle((int)r.x, (int)(r.y + r.height / 2 - 3),
                  (int)(hx - r.x), 6, Bl);

    /* 画手柄（白色小方块，10×滑块高度） */
    DrawRectangle((int)(hx - 5), (int)r.y, 10, (int)r.height, Wh);

    /* 交互：鼠标按住时拖动 */
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && In(r)) {  // 鼠标左键按住 且 在滑块区域内
        t = (GetMousePosition().x - r.x) / r.width;       // 根据鼠标位置重新计算比例
        if (t < 0) t = 0;                                 // 限制比例不低于0
        if (t > 1) t = 1;                                 // 限制比例不高于1
        return lo + t * (hi - lo);                        // 比例转回实际值并返回
    }

    return v;                               // 没有拖动，返回原值
}


/* ================================================================
 *                        主函数（Main）
 * ================================================================ */

/*
 * main() - 程序入口点
 *
 * 每个C程序都从main函数开始执行。这个main函数做了以下事情：
 *   1. 初始化窗口和3D相机
 *   2. 进入主循环（每帧执行一次，直到窗口关闭）
 *      - 处理键盘输入
 *      - 处理鼠标滚轮缩放
 *      - 处理3D点击拾取
 *      - 更新逻辑
 *      - 绘制3D场景和UI
 *   3. 关闭窗口并退出
 */
int main(void) {
    /* ---- 窗口初始化 ---- */
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    /* FLAG_MSAA_4X_HINT：启用4倍多重采样抗锯齿（让3D画面边缘更平滑）
       FLAG_WINDOW_RESIZABLE：允许用户拖拽调整窗口大小 */

    InitWindow(1280, 720, "Drone Light Show");  // 创建1280×720像素的窗口
    SetTargetFPS(60);                            // 设定目标帧率为60帧/秒

    /* ---- 3D相机初始化 ---- */
    /* 设置一个固定的斜视角，让用户能从侧上方看到整个3D场景 */
    Cam.position   = (Vector3){55, 48, 70};      // 相机位置（从哪个点观察）
    Cam.target     = (Vector3){20, 0, 15};       // 注视目标（看向哪个点）
    Cam.up         = (Vector3){0, 1, 0};          // 上方向（Y轴朝上）
    Cam.fovy       = 50;                         // 视场角（视野广度，单位：度）
    Cam.projection = CAMERA_PERSPECTIVE;          // 透视投影（近大远小）

    /* ---- 主循环 ---- */
    /* WindowShouldClose() 在用户点击关闭按钮或按Esc时返回true */
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();              // 获取本帧的时间间隔（秒）

        Keys();                                 // 处理键盘快捷键

        /* ---- 鼠标滚轮缩放 ---- */
        float wh = GetMouseWheelMove();         // 获取滚轮滚动量（正值=放大，负值=缩小）
        if (wh != 0) {                          // 滚轮有滚动
            Vector3 dir = Vector3Subtract(Cam.position, Cam.target);
            /* Vector3Subtract：向量减法，得到从target指向position的方向向量 */

            float len = Vector3Length(dir);     // 方向向量的长度 = 相机到目标的距离

            /* 根据滚轮方向缩放距离 */
            if (wh > 0) len *= 0.9f;            // 向前滚→距离×0.9（靠近=放大）
            else        len *= 1.1f;            // 向后滚→距离×1.1（远离=缩小）

            /* 限制缩放范围 */
            if (len < 5)  len = 5;              // 最近距离5米
            if (len > 60) len = 60;             // 最远距离60米

            /* 重新计算相机位置 */
            dir = Vector3Scale(Vector3Normalize(dir), len);
            /* Vector3Normalize：将向量归一化（变成单位向量，只保留方向）
               Vector3Scale：将单位向量乘以新长度 */

            Cam.position = Vector3Add(Cam.target, dir);
            /* Vector3Add：向量加法，target + 方向向量 = 新相机位置 */
        }

        /* ---- 初始界面特殊处理 ---- */
        if (M == M_INTRO) {
            Update(dt);                         // 更新逻辑
            BeginDrawing();                     // 开始绘制
            ClearBackground(Bg);                // 清屏（深色背景）
            DrawStartScreen();                  // 画欢迎界面
            EndDrawing();                       // 结束绘制（刷新到屏幕）
            continue;                           // 跳过本帧剩余代码，回到循环开头
            /* continue 用于跳过本次循环的剩余部分，直接开始下一次迭代 */
        }

        /* ---- 3D拾取：检测点击了哪架无人机 ---- */
        int pk = Pick();                        // 检测鼠标点击
        if (pk >= 0) {                          // 如果点到了某架无人机
            if (S >= 0) D[S].sel = 0;           // 取消之前的选中
            S = pk;                             // 更新选中索引
            D[S].sel = 1;                       // 标记新无人机为选中
            if (M == M_SETUP) M = M_EDIT;       // 在Setup模式下点击3D无人机→自动进入Edit模式
        }

        /* ---- 绘制 ---- */
        Update(dt);                             // 先更新逻辑
        BeginDrawing();                         // 开始绘制帧
        ClearBackground(Bg);                    // 清屏
        Draw3D();                               // 画3D场景
        DrawUI();                               // 画UI面板

        /* ---- 左下角帮助面板 ---- */
        int hy = GetScreenHeight() - 100;       // 帮助面板的Y坐标（距底部100像素）
        DrawRectangle(6, hy, 210, 96, Fade(BLACK, 0.7f));  // 半透明黑色背景

        DrawText("Help", 12, hy + 4, 13, Bl);                   // 标题"Help"
        DrawText("F1=Setup F2=Edit F3=Show", 12, hy + 20, 11, Gr);
        DrawText("Tab=Next  1/2/3=Light",    12, hy + 34, 11, Gr);
        DrawText("Click 3D=Select  F=Focus", 12, hy + 48, 11, Gr);
        DrawText("Scroll=Zoom",              12, hy + 62, 11, Gr);

        /* 状态栏：当前模式和无人机数量 */
        DrawText(TextFormat("Mode:%s  Drones:%d",
            M == M_INTRO ? "Intro" :          // 三元运算符链式判断当前模式名称
            M == M_SETUP ? "Setup" :
            M == M_EDIT  ? "Edit"  : "Show",
            N),
            12, hy + 78, 11, Ye);

        /* 状态消息（如果计时器>0则显示） */
        if (mt > 0)
            DrawText(msg, GetScreenWidth() / 2 - 150, 6, 13, Gn);  // 屏幕顶部居中

        /* FPS显示（右上角，3D视图区域） */
        DrawText(TextFormat("FPS:%d", GetFPS()),
                 GetScreenWidth() - PW - 50, 6, 11, Gr);

        EndDrawing();                           // 结束绘制
    }

    CloseWindow();                              // 关闭窗口，释放资源
    return 0;                                   // 返回0表示程序正常退出
}
