/******************************************************************************
 *  json_store.c  -  项目场景存取（SaveShow / LoadShow）
 *
 *  在 JSON 库之上，把整个无人机数组序列化保存到文件、或从文件还原。
 *  保存格式见 README「文件格式」一节。
 ******************************************************************************/
#include "json_internal.h"
#include <stdio.h>      // fopen / fseek / fread / fwrite

/* ============================ 场景存取 ============================ */

/* SaveShow() - 把当前所有无人机保存为 JSON 文件 */
int SaveShow(const char* path) {
    JVal* root = JNew(J_OBJ);

    ObjAdd(root, "version",  JNumVal(3.0));
    ObjAdd(root, "count",    JNumVal(N));
    ObjAdd(root, "pathMode", JNumVal(pathMode));

    /* 每架无人机一个对象，放进 drones 数组 */
    JVal* drones = JNew(J_ARR);
    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        JVal* jo = JNew(J_OBJ);

        ObjAdd(jo, "name",   JStrVal(d->name));
        ObjAdd(jo, "color",  JNumVal(d->color));
        ObjAdd(jo, "light",  JNumVal(d->light));
        ObjAdd(jo, "espeed", JNumVal(d->espeed));

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

        ArrAdd(drones, jo);
    }
    ObjAdd(root, "drones", drones);

    /* 序列化并写文件 */
    char* text = JsonEmit(root);
    FILE* f = fopen(path, "w");
    if (!f) { free(text); JsonFree(root); return 0; }
    fputs(text, f);
    fclose(f);
    free(text);
    JsonFree(root);
    return 1;
}

/* LoadShow() - 从 JSON 文件还原所有无人机 */
int LoadShow(const char* path) {
    /* 读入整个文件 */
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* text = (char*)malloc(sz + 1);
    fread(text, 1, sz, f);
    text[sz] = 0;
    fclose(f);

    JVal* root = JsonParse(text);
    free(text);
    if (!root) return 0;

    int cnt  = (int)JsonNum(root, "count", 0);
    pathMode = (int)JsonNum(root, "pathMode", PM_EASED);
    JVal* drones = JsonGet(root, "drones");

    int limit = (drones && drones->type == J_ARR) ? drones->count : 0;
    if (limit > cnt)       limit = cnt;
    if (limit > MAX_DRONES) limit = MAX_DRONES;

    /* 逐架还原 */
    for (int i = 0; i < limit; i++) {
        JVal* jo = drones->items[i];
        Drone* d = &D[i];
        memset(d, 0, sizeof(Drone));

        d->act    = 1;
        d->light  = L_ON;
        d->bon    = 1;
        d->espeed = 1.0f;
        d->ph     = (float)i;                 // 追逐效果相位 = 序号

        snprintf(d->name, MAX_NAME, "%s", JsonStr(jo, "name", "D"));
        d->color  = (int)JsonNum(jo, "color", 0);
        d->light  = (int)JsonNum(jo, "light", L_ON);
        d->espeed = (float)JsonNum(jo, "espeed", 1.0);

        /* 起始位置 */
        JVal* st = JsonGet(jo, "start");
        if (st) {
            d->start.x = (float)JsonNum(st, "x", 0);
            d->start.y = (float)JsonNum(st, "y", 0.5);
            d->start.z = (float)JsonNum(st, "z", 0);
        }
        d->pos = d->start;
        d->h   = d->start.y;

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

    N = limit;
    S = -1;                                     // 加载后取消选中
    JsonFree(root);
    return 1;
}
