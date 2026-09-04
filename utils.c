/******************************************************************************
 *  utils.c  -  工具函数实现
 *
 *  包含6个UI基础组件：Msg（消息）、In（鼠标检测）、Btn（按钮）、
 *  Txt（文字输入框）、Sep（分隔线）、Sld（滑块）。
 *
 *  【给初学者】
 *  注意这里每个函数都不再加 static 关键字——因为其他 .c 文件也需要调用它们。
 *  static 的作用是"限制函数/变量只在当前文件内可见"。
 ******************************************************************************/
#include "utils.h"      // 自己的头文件（函数声明）
#include "common.h"     // 所有全局变量（msg, mt, txtFocus, Br, Bl, Wh, Bt 等）

/* ================================================================
 *  Msg() - 显示一条状态消息
 *
 *  用法和 printf 一样：Msg("创建了 %s", name);
 *  消息会在屏幕顶部显示约2.5秒后自动消失。
 *
 *  涉及的C语言知识点：
 *    va_list / va_start / va_end 是C语言处理可变参数的标准方式。
 *    vsnprintf 是 sprintf 的安全版本，限制最大写入长度防止溢出。
 * ================================================================ */
void Msg(const char* f, ...) {
    va_list a;                                  // 声明一个可变参数列表变量
    va_start(a, f);                             // 初始化：让 a 指向 f 后面的第一个可变参数
    vsnprintf(msg, sizeof(msg), f, a);          // 将格式化后的字符串写入 msg 缓冲区
    va_end(a);                                  // 清理可变参数列表（必须与va_start配对）
    mt = 2.5f;                                  // 设置消息显示倒计时2.5秒
}

/* ================================================================
 *  In() - 判断鼠标是否在一个矩形区域内
 *
 *  这是UI交互的核心函数——判断用户是否把鼠标移到了某个按钮/输入框上。
 *  返回: 1（true，在里面）或 0（false，不在里面）
 * ================================================================ */
int In(Rectangle r) {
    Vector2 m = GetMousePosition();             // 获取鼠标当前的屏幕坐标
    /* 判断鼠标坐标是否在矩形四条边界之内 */
    return m.x >= r.x                           // 鼠标X ≥ 矩形左边界
        && m.x <= r.x + r.width                 // 鼠标X ≤ 矩形右边界
        && m.y >= r.y                           // 鼠标Y ≥ 矩形上边界
        && m.y <= r.y + r.height;               // 鼠标Y ≤ 矩形下边界
}

/* ================================================================
 *  Btn() - 绘制按钮并检测点击
 *
 *  做两件事：1) 画按钮（背景+边框+文字） 2) 检测是否被点击
 *  返回: 1（被点击了）或 0（没被点击）
 * ================================================================ */
int Btn(Rectangle r, const char* t, Color c) {
    DrawRectangleRec(r, c);                     // 画按钮背景填充矩形
    DrawRectangleLinesEx(r, 1, Br);             // 画边框线（1像素宽，深灰色）
    int tw = MeasureText(t, 18);                // 测量文字宽度（字号18），用于居中计算
    /* 水平居中：矩形中心X - 文字宽度的一半；垂直居中：矩形中心Y - 字号的一半 */
    DrawText(t, (int)(r.x + r.width / 2 - tw / 2),
                (int)(r.y + r.height / 2 - 9), 18, Wh);
    /* 鼠标在矩形内 并且 鼠标左键刚被按下 → 返回1（被点击） */
    return In(r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/* ================================================================
 *  Txt() - 文字输入框
 *
 *  这是最复杂的UI组件，负责：
 *    1. 绘制输入框外观（标签、背景、边框、文字）
 *    2. 处理鼠标点击激活/取消激活
 *    3. 处理键盘输入（打字、退格、负号、小数点）
 *    4. 绘制闪烁光标
 *
 *  返回: 1（内容变化了）或 0（没变化）
 *
 *  参数:
 *    r     - 输入框的矩形区域
 *    buf   - 存储输入内容的字符数组指针
 *    max   - buf 的最大容量
 *    label - 输入框上方显示的标签
 * ================================================================ */
int Txt(Rectangle r, char* buf, int max, const char* label) {
    /* 在输入框上方画标签文字 */
    DrawText(label, (int)r.x, (int)(r.y - 14), 12, Gr);

    /* 边框颜色：默认深灰，鼠标悬停时变蓝（hover效果） */
    Color bc = Br;
    if (In(r)) bc = Bl;

    DrawRectangleRec(r, (Color){20, 22, 32, 255});      // 输入框背景（深色）
    DrawRectangleLinesEx(r, 1.5f, bc);                   // 输入框边框
    DrawText(buf, (int)(r.x + 4),                        // 已输入的文字
             (int)(r.y + r.height / 2 - 8), 16, Wh);

    /* ---- 焦点管理 ---- */
    /* activeIdx 是 static 局部变量——在函数调用之间保持值不变。
       它记录当前被激活的输入框ID，确保只有一个输入框接收键盘输入。 */
    static int activeIdx = -1;              // -1 表示没有输入框被激活

    /* 用 buf 指针的地址生成唯一ID（取低12位），区分不同的输入框 */
    int id = (int)((long long)buf & 0xFFF);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {      // 鼠标左键按下
        if (In(r))                          // 点在当前输入框内 → 激活它
            activeIdx = id;
        else if (activeIdx == id)           // 点在外部且当前是激活的 → 取消激活
            activeIdx = -1;
    }

    if (activeIdx == id) txtFocus = 1;      // 告诉系统"有输入框正在接收键盘输入"
                                            // 用于阻止快捷键干扰打字

    /* ---- 键盘输入处理 ---- */
    if (activeIdx == id) {                  // 只有激活的输入框才处理按键
        int changed = 0;                    // 标记内容是否改变

        /* 处理普通字符输入（可打印ASCII字符：空格~波浪号） */
        int key = GetCharPressed();         // 获取按下的字符码
        while (key > 0) {                   // 可能一帧内按了多个键，全部处理
            if (key >= 32 && key <= 126) {  // 只接受可打印字符
                int len = (int)strlen(buf);
                if (len < max - 1) {        // 留一个位置给字符串结束符 '\0'
                    buf[len] = (char)key;   // 追加字符到末尾
                    buf[len + 1] = 0;       // 添加结束符
                    changed = 1;
                }
            }
            key = GetCharPressed();         // 继续获取下一个待处理按键
        }

        /* 退格键：删除最后一个字符 */
        if (IsKeyPressed(KEY_BACKSPACE)) {
            int len = (int)strlen(buf);
            if (len > 0) {                  // 至少有一个字符才删
                buf[len - 1] = 0;           // 将最后一个字符替换为'\0'
                changed = 1;
            }
        }

        /* 负号键：在空输入框开头输入负号 */
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
            int len = (int)strlen(buf);
            if (len == 0) {                 // 只有输入框为空时才能输入负号
                buf[0] = '-';
                buf[1] = 0;
                changed = 1;
            }
        }

        /* 小数点键：输入小数点（每个数字只能有一个小数点） */
        if (IsKeyPressed(KEY_PERIOD) || IsKeyPressed(KEY_KP_DECIMAL)) {
            int len = (int)strlen(buf);
            if (len < max - 1 && !strchr(buf, '.')) {  // 有空间 且 没小数点
                buf[len] = '.';
                buf[len + 1] = 0;
                changed = 1;
            }
        }

        /* ---- 闪烁光标 ---- */
        /* GetTime() 是程序运行秒数，乘以2让光标每秒闪烁2次 */
        if (((int)(GetTime() * 2) % 2) == 0) {  // 偶数半秒显示光标
            int tw = MeasureText(buf, 16);       // 文字像素宽度
            DrawText("|",                        // 竖线作为光标
                     (int)(r.x + 5 + tw),        // 位置紧跟文字末尾
                     (int)(r.y + r.height / 2 - 8), 16, Bl);
        }

        return changed;                     // 返回内容是否变化
    }
    return 0;                               // 未激活，无变化
}

/* ================================================================
 *  Sep() - 画水平分隔线
 * ================================================================ */
void Sep(int x, int y, int w) {
    DrawRectangle(x, y, w, 1, Br);          // 1像素高的矩形 = 一条横线
}

/* ================================================================
 *  Sld() - 滑块控件（Show模式调节播放速度）
 *
 *  绘制内容：标签文字 + 滑轨背景 + 已填充部分 + 拖动手柄
 *  返回: 用户调整后的新值（没拖动则返回原值v）
 *
 *  参数:
 *    r  - 滑块区域
 *    v  - 当前值
 *    lo - 最小值
 *    hi - 最大值
 *    f  - 标签格式字符串，如 "Speed: %.1fx"
 * ================================================================ */
float Sld(Rectangle r, float v, float lo, float hi, const char* f) {
    /* 画标签文字（如 "Speed: 2.0x"） */
    DrawText(TextFormat(f, v), (int)r.x, (int)(r.y - 15), 13, Gr);

    /* 画滑轨背景（深色横条，高6像素） */
    DrawRectangleRec((Rectangle){r.x, r.y + r.height / 2 - 3, r.width, 6},
                     (Color){50, 50, 65, 255});

    /* 将当前值映射到0~1范围（归一化），计算手柄X坐标 */
    float t  = (v - lo) / (hi - lo);        // 归一化比例
    float hx = r.x + t * r.width;           // 手柄在轨道上的X位置

    /* 画已填充部分（蓝色，从起点到手柄位置） */
    DrawRectangle((int)r.x, (int)(r.y + r.height / 2 - 3),
                  (int)(hx - r.x), 6, Bl);

    /* 画手柄（白色小方块） */
    DrawRectangle((int)(hx - 5), (int)r.y, 10, (int)r.height, Wh);

    /* 交互：鼠标按住时拖动 */
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && In(r)) {
        t = (GetMousePosition().x - r.x) / r.width;     // 根据鼠标位置重算比例
        if (t < 0) t = 0;                               // 限制最小值
        if (t > 1) t = 1;                               // 限制最大值
        return lo + t * (hi - lo);                      // 比例→实际值
    }

    return v;                               // 没拖动，返回原值
}

/* ================================================================
 *  Hsv2Rgb() - HSV 色相转 RGB 颜色
 *
 *  HSV（色相/饱和度/明度）比 RGB 更适合表达"颜色循环"，
 *  因为色相 h 从 0 变到 1 就能绕整个色环一圈（红→绿→蓝→红）。
 *  用于彩虹灯光效果：让 h 随时间增长，颜色就不断循环变化。
 *
 *  参数:
 *    h - 色相（0~1，超出的部分会回绕）
 *    s - 饱和度（0~1）
 *    v - 明度（0~1）
 * ================================================================ */
Color Hsv2Rgb(float h, float s, float v) {
    float r = 0, g = 0, b = 0;

    if (s <= 0) {                           // 无饱和度 → 灰色
        r = g = b = v;
    } else {
        if (h >= 1.0f) h -= (int)h;         // 色相回绕到 0~1
        if (h < 0.0f)  h += 1.0f;
        h *= 6.0f;                          // 色相 → 六段扇区
        int   i = (int)h;                   // 落在哪个扇区
        float f = h - i;                    // 扇区内的小数部分
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));

        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
        }
    }

    return (Color){
        (unsigned char)(r * 255.0f),
        (unsigned char)(g * 255.0f),
        (unsigned char)(b * 255.0f),
        255
    };
}
