/******************************************************************************
 *  json_store.c  -  项目场景存取（SaveShow / LoadShow）
 *
 *  在 JSON 库之上，把整个无人机数组序列化保存到文件、或从文件还原。
 *  保存格式见 README「文件格式」一节。
 ******************************************************************************/
#include "json_internal.h"
#include <stdio.h>      // fopen / fseek / fread / fwrite

/* ============================ 场景存取 ============================ */

/* DroneToJson() - 把一架无人机序列化成一个 JSON 对象（含起点与航点数组） */
static JVal* DroneToJson(const Drone* d) {
    JVal* jo = JNew(J_OBJ);

    ObjAdd(jo, "name",   JStrVal(d->name));
    ObjAdd(jo, "color",  JNumVal(d->color));
    ObjAdd(jo, "light",  JNumVal(d->light));
    ObjAdd(jo, "espeed", JNumVal(d->espeed));
    ObjAdd(jo, "pm",     JNumVal(d->pm));

    /* 起始位置 */
    JVal* st = JNew(J_OBJ);
    ObjAdd(st, "x", JNumVal(d->start.x));
    ObjAdd(st, "y", JNumVal(d->start.y));
    ObjAdd(st, "z", JNumVal(d->start.z));
    ObjAdd(jo, "start", st);

    /* 航点数组 */
    JVal* wps = JNew(J_ARR);
    for (int w = 0; w < d->wc; w++) {
        JVal* wp = JNew(J_OBJ);
        ObjAdd(wp, "x", JNumVal(d->wp[w].p.x));
        ObjAdd(wp, "y", JNumVal(d->wp[w].p.y));
        ObjAdd(wp, "z", JNumVal(d->wp[w].p.z));
        ArrAdd(wps, wp);
    }
    ObjAdd(jo, "waypoints", wps);

    return jo;
}

/* BuildShowJson() - 把当前所有无人机序列化成 JSON 文本
 * 返回 malloc 分配的字符串（调用方负责 free）。 */
static char* BuildShowJson(void) {
    JVal* root = JNew(J_OBJ);
    ObjAdd(root, "version",  JNumVal(3.0));
    ObjAdd(root, "count",    JNumVal(N));

    /* 每架无人机一个对象，放进 drones 数组 */
    JVal* drones = JNew(J_ARR);
    for (int i = 0; i < N; i++)
        ArrAdd(drones, DroneToJson(&D[i]));
    ObjAdd(root, "drones", drones);

    char* text = JsonEmit(root);        // 序列化为文本
    JsonFree(root);                     // 释放 JSON 树（文本已独立分配）
    return text;
}

/* WriteTextFile() - 把文本写入文件（覆盖写），成功返回 1 */
static int WriteTextFile(const char* path, const char* text) {
    FILE* f = fopen(path, "w");
    if (!f) return 0;                   // 打不开 → 失败
    fputs(text, f);
    fclose(f);
    return 1;
}

/* SaveShow() - 把当前所有无人机保存为 JSON 文件 */
int SaveShow(const char* path) {
    if (!path) return 0;                // 路径为空 → 保存失败

    char* text = BuildShowJson();       // 序列化
    int ok = WriteTextFile(path, text); // 写文件
    free(text);
    return ok;
}

/* DroneFromJson() - 从 JSON 对象还原一架无人机（字段缺省时用默认值）。
 * 注意：灯光效果相位 ph 不在这里设置，由调用方按无人机序号赋值。 */
static void DroneFromJson(Drone* d, JVal* jo) {
    memset(d, 0, sizeof(Drone));

    d->act    = 1;
    d->light  = L_ON;
    d->bon    = 1;
    d->espeed = 1.0f;

    snprintf(d->name, MAX_NAME, "%s", JsonStr(jo, "name", "D"));
    d->color  = (int)JsonNum(jo, "color", 0);
    d->light  = (int)JsonNum(jo, "light", L_ON);
    d->espeed = (float)JsonNum(jo, "espeed", 1.0);
    d->pm     = (int)JsonNum(jo, "pm", PM_EASED);

    /* 起始位置 */
    JVal* st = JsonGet(jo, "start");
    if (st) {
        d->start.x = (float)JsonNum(st, "x", 0);
        d->start.y = (float)JsonNum(st, "y", 0.5);
        d->start.z = (float)JsonNum(st, "z", 0);
    }
    d->pos = d->start;
    d->h   = d->start.z;

    /* 航点 */
    JVal* wps = JsonGet(jo, "waypoints");
    if (wps && wps->type == J_ARR) {
        int wc = wps->count;
        if (wc > MAX_WP) wc = MAX_WP;
        d->wc = wc;
        for (int w = 0; w < wc; w++) {
            JVal* wp = wps->items[w];
            d->wp[w].p.x = (float)JsonNum(wp, "x", 0);
            d->wp[w].p.y = (float)JsonNum(wp, "y", 0.5);
            d->wp[w].p.z = (float)JsonNum(wp, "z", 0);
        }
    }
}

/* ReadTextFile() - 读入整个文件为字符串（malloc 分配，末尾补 0）
 * 失败返回 NULL。 */
static char* ReadTextFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);              // 移到文件末尾
    long sz = ftell(f);                 // 得到文件大小
    fseek(f, 0, SEEK_SET);              // 回到开头

    char* text = (char*)malloc(sz + 1); // 多留 1 字节给结尾 0
    fread(text, 1, sz, f);
    text[sz] = 0;
    fclose(f);
    return text;
}

/* RestoreDrones() - 从 JSON 的 drones 数组还原无人机到全局数组 */
static void RestoreDrones(JVal* root) {
    int cnt    = (int)JsonNum(root, "count", 0);
    JVal* drones = JsonGet(root, "drones");

    int limit = (drones && drones->type == J_ARR) ? drones->count : 0;
    if (limit > cnt)        limit = cnt;
    if (limit > MAX_DRONES) limit = MAX_DRONES;

    /* 逐架还原 */
    for (int i = 0; i < limit; i++) {
        DroneFromJson(&D[i], drones->items[i]);
        D[i].ph = (float)i;                 // 追逐效果相位 = 序号
    }

    N = limit;
    S = -1;                                 // 加载后取消选中
}

/* LoadShow() - 从 JSON 文件还原所有无人机 */
int LoadShow(const char* path) {
    if (!path) return 0;                // 路径为空 → 加载失败

    char* text = ReadTextFile(path);    // 读入整个文件
    if (!text) return 0;

    JVal* root = JsonParse(text);       // 解析 JSON
    free(text);
    if (!root) return 0;

    RestoreDrones(root);                // 还原无人机
    JsonFree(root);
    return 1;
}
