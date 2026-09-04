/******************************************************************************
 *  test.c  -  自检模块实现
 *
 *  对核心算法做断言测试：缓动函数边界、Catmull-Rom 样条端点、
 *  距离/路径长度、位置采样，以及 JSON 解析/序列化往返。
 *
 *  【给初学者】
 *  断言（assert）测试的思路：给函数一个"已知答案"的输入，
 *  看它输出的结果是否和答案一致。若一致就通过，不一致就记一次失败。
 *  这样改动算法后，只要跑一遍自检，就知道有没有把数学搞错。
 ******************************************************************************/
#include "test.h"       // 自己的头文件
#include "common.h"     // Drone、Pt 等类型
#include "trajectory.h" // 缓动、样条、Dist3、PathLen、DronePosAt
#include "json.h"       // JsonParse / JsonEmit / JsonNum / JsonStr
#include "utils.h"      // Msg（打印自检结果）

/* ==================== 断言计数 ==================== */
static int  checks = 0;             // 已执行断言数
static int  fails  = 0;             // 失败断言数
static char firstFail[64] = "";     // 第一个失败的断言名（用于报告）

/* ================================================================
 *  Check() - 记录一条断言结果
 *
 *  ok=真 → 通过；ok=假 → 失败并累加，同时记下第一个失败的断言名。
 * ================================================================ */
static void Check(int ok, const char* name) {
    checks++;
    if (!ok) {
        if (fails == 0)                     // 只记第一个失败
            snprintf(firstFail, sizeof(firstFail), "%s", name);
        fails++;
    }
}

/* ================================================================
 *  Near() - 浮点数近似相等判断（误差 < 1e-3）
 *
 *  浮点数有精度误差，不能直接用 == 比较，用"接近"判断更稳妥。
 * ================================================================ */
static int Near(float a, float b) {
    return fabsf(a - b) < 1e-3f;
}

/* ================================================================
 *  testEasing() - 测试缓动函数
 *
 *  关键性质：所有缓动函数都必须满足 Ease(0)=0、Ease(1)=1，
 *  否则轨迹会"到不了终点"或"回退"。
 * ================================================================ */
static void testEasing(void) {
    Check(Near(EaseLinear(0), 0), "EaseLinear(0)=0");
    Check(Near(EaseLinear(1), 1), "EaseLinear(1)=1");
    Check(Near(EaseLinear(0.5f), 0.5f), "EaseLinear(0.5)=0.5");

    Check(Near(EaseInQuad(0), 0), "EaseInQuad(0)=0");
    Check(Near(EaseInQuad(1), 1), "EaseInQuad(1)=1");
    Check(EaseInQuad(0.5f) < 0.5f, "EaseInQuad(0.5)<0.5");  // 缓入前慢

    Check(Near(EaseOutQuad(0), 0), "EaseOutQuad(0)=0");
    Check(Near(EaseOutQuad(1), 1), "EaseOutQuad(1)=1");
    Check(EaseOutQuad(0.5f) > 0.5f, "EaseOutQuad(0.5)>0.5");  // 缓出前快

    Check(Near(EaseInOutCubic(0), 0), "EaseInOutCubic(0)=0");
    Check(Near(EaseInOutCubic(1), 1), "EaseInOutCubic(1)=1");
    Check(Near(EaseInOutCubic(0.5f), 0.5f), "EaseInOutCubic(0.5)=0.5");

    Check(Near(SmoothStep(0), 0), "SmoothStep(0)=0");
    Check(Near(SmoothStep(1), 1), "SmoothStep(1)=1");
    Check(Near(SmoothStep(0.5f), 0.5f), "SmoothStep(0.5)=0.5");

    /* 分发函数 Ease：PM_EASED 应等于 SmoothStep */
    Check(Near(Ease(0.25f, PM_EASED), SmoothStep(0.25f)), "Ease(EASED)=SmoothStep");
    Check(Near(Ease(0.25f, PM_LINEAR), 0.25f), "Ease(LINEAR)=linear");
}

/* ================================================================
 *  testSpline() - 测试 Catmull-Rom 样条与距离/长度
 * ================================================================ */
static void testSpline(void) {
    /* 距离：3-4-5 直角三角形 */
    Pt a = {0, 0, 0};
    Pt b = {3, 4, 0};
    Check(Near(Dist3(a, b), 5.0f), "Dist3(3,4,0)=5");

    /* Catmull-Rom 端点性质：u=0 返回 p1，u=1 返回 p2 */
    Pt p0 = {-1, 0, 0};
    Pt p1 = {0, 0, 0};
    Pt p2 = {1, 0, 0};
    Pt p3 = {2, 0, 0};
    Pt m0 = CatmullRom(p0, p1, p2, p3, 0);
    Pt m1 = CatmullRom(p0, p1, p2, p3, 1);
    Check(Near(m0.x, p1.x) && Near(m0.y, p1.y) && Near(m0.z, p1.z),
          "CatmullRom u=0 -> p1");
    Check(Near(m1.x, p2.x) && Near(m1.y, p2.y) && Near(m1.z, p2.z),
          "CatmullRom u=1 -> p2");
}

/* ================================================================
 *  testPath() - 测试路径长度与位置采样
 *
 *  构造一条"已知答案"的路径：起点(0,0,0)，航点(0,0,10)、(10,0,10)。
 *  总长应为 10+10=20；采样 s=0 回到起点，s=20 到终点，
 *  s=10（线性模式）正好落在中间拐点 (0,0,10)。
 * ================================================================ */
static void testPath(void) {
    Drone d;
    memset(&d, 0, sizeof(d));
    d.start = (Pt){0, 0, 0};
    d.wc = 2;
    d.wp[0].p = (Pt){0, 0, 10};
    d.wp[1].p = (Pt){10, 0, 10};

    /* 总长度 = 起点→wp0 (10) + wp0→wp1 (10) */
    Check(Near(PathLen(&d), 20.0f), "PathLen=20");

    /* 采样起点与终点 */
    Pt at0 = DronePosAt(&d, 0, PM_LINEAR);
    Pt atEnd = DronePosAt(&d, 20, PM_LINEAR);
    Check(Near(at0.x, 0) && Near(at0.z, 0), "DronePosAt s=0 -> start");
    Check(Near(atEnd.x, 10) && Near(atEnd.z, 10), "DronePosAt s=20 -> last");

    /* s=10 应落在拐点 (0,0,10) */
    Pt mid = DronePosAt(&d, 10, PM_LINEAR);
    Check(Near(mid.x, 0) && Near(mid.z, 10), "DronePosAt s=10 -> wp0");

    /* 无航点：任何 s 都应停在起点 */
    Drone e;
    memset(&e, 0, sizeof(e));
    e.start = (Pt){5, 2, 3};
    Pt st = DronePosAt(&e, 999, PM_SPLINE);
    Check(Near(st.x, 5) && Near(st.y, 2) && Near(st.z, 3), "no-waypoint stays at start");
}

/* ================================================================
 *  testJson() - 测试 JSON 解析/序列化往返
 *
 *  解析一段已知 JSON，检查取值；再序列化并重新解析，验证往返一致。
 * ================================================================ */
static void testJson(void) {
    const char* js =
        "{\"name\":\"D-1\",\"count\":3,\"pi\":3.14,\"arr\":[1,2,3]}";

    JVal* v = JsonParse(js);
    Check(v != NULL, "JsonParse ok");
    if (!v) return;

    Check(strcmp(JsonStr(v, "name", ""), "D-1") == 0, "JsonStr name");
    Check(Near((float)JsonNum(v, "count", 0), 3.0f), "JsonNum count");
    Check(Near((float)JsonNum(v, "pi", 0), 3.14f), "JsonNum float");
    Check(JsonGet(v, "arr") != NULL, "JsonGet arr");

    /* 序列化 → 再解析，验证往返 */
    char* out = JsonEmit(v);
    Check(out != NULL, "JsonEmit ok");
    if (out) {
        JVal* v2 = JsonParse(out);
        Check(v2 != NULL, "JsonEmit reparse ok");
        if (v2) {
            Check(Near((float)JsonNum(v2, "count", 0), 3.0f), "JSON roundtrip count");
            JsonFree(v2);
        }
        free(out);
    }

    /* 空指针输入应返回 NULL（防御性判断） */
    Check(JsonParse(NULL) == NULL, "JsonParse NULL input");
    JsonFree(v);
}

/* ================================================================
 *  RunSelfTest() - 运行全部自检并打印结果
 *
 *  返回失败的断言数量；通过 Msg() 在屏幕顶部显示结果。
 * ================================================================ */
int RunSelfTest(void) {
    checks = 0;                         // 重置计数
    fails  = 0;

    testEasing();                       // 缓动
    testSpline();                       // 样条 + 距离
    testPath();                         // 路径长度 + 采样
    testJson();                         // JSON 往返

    if (fails == 0)
        Msg("Self-test: %d/%d passed OK", checks, checks);
    else
        Msg("Self-test: %d/%d FAILED (first: %s)", checks - fails, checks, firstFail);

    return fails;                       // 供调用方判断
}
