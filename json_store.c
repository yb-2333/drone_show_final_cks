#include "json_internal.h"
#include <stdio.h>

static JVal* DroneToJson(const Drone* d) {
    JVal* jo = JNew(J_OBJ);

    ObjAdd(jo, "name",   JStrVal(d->name));
    ObjAdd(jo, "color",  JNumVal(d->color));
    ObjAdd(jo, "light",  JNumVal(d->light));
    ObjAdd(jo, "espeed", JNumVal(d->espeed));
    ObjAdd(jo, "pm",     JNumVal(d->pm));

    JVal* st = JNew(J_OBJ);
    ObjAdd(st, "x", JNumVal(d->start.x));
    ObjAdd(st, "y", JNumVal(d->start.y));
    ObjAdd(st, "z", JNumVal(d->start.z));
    ObjAdd(jo, "start", st);

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

static char* BuildShowJson(void) {
    JVal* root = JNew(J_OBJ);
    ObjAdd(root, "version",  JNumVal(3.0));
    ObjAdd(root, "count",    JNumVal(N));

    JVal* drones = JNew(J_ARR);
    for (int i = 0; i < N; i++)
        ArrAdd(drones, DroneToJson(&D[i]));
    ObjAdd(root, "drones", drones);

    char* text = JsonEmit(root);
    JsonFree(root);
    return text;
}

static int WriteTextFile(const char* path, const char* text) {
    FILE* f = fopen(path, "w");
    if (!f) return 0;
    fputs(text, f);
    fclose(f);
    return 1;
}

int SaveShow(const char* path) {
    if (!path) return 0;

    char* text = BuildShowJson();
    int ok = WriteTextFile(path, text);
    free(text);
    return ok;
}

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

    JVal* st = JsonGet(jo, "start");
    if (st) {
        d->start.x = (float)JsonNum(st, "x", 0);
        d->start.y = (float)JsonNum(st, "y", 0.5);
        d->start.z = (float)JsonNum(st, "z", 0);
    }
    d->pos = d->start;
    d->h   = d->start.z;

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

static char* ReadTextFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* text = (char*)malloc(sz + 1);
    fread(text, 1, sz, f);
    text[sz] = 0;
    fclose(f);
    return text;
}

static void RestoreDrones(JVal* root) {
    int cnt    = (int)JsonNum(root, "count", 0);
    JVal* drones = JsonGet(root, "drones");

    int limit = (drones && drones->type == J_ARR) ? drones->count : 0;
    if (limit > cnt)        limit = cnt;
    if (limit > MAX_DRONES) limit = MAX_DRONES;

    for (int i = 0; i < limit; i++) {
        DroneFromJson(&D[i], drones->items[i]);
        D[i].ph = (float)i;
    }

    N = limit;
    S = -1;
}

int LoadShow(const char* path) {
    if (!path) return 0;

    char* text = ReadTextFile(path);
    if (!text) return 0;

    JVal* root = JsonParse(text);
    free(text);
    if (!root) return 0;

    RestoreDrones(root);
    JsonFree(root);
    return 1;
}
