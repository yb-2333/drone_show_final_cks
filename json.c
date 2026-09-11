#include "json_internal.h"

char* xstrdup(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

JVal* JNew(JType t) {
    JVal* v = (JVal*)calloc(1, sizeof(JVal));
    v->type = t;
    return v;
}

JVal* JNumVal(double x) { JVal* v = JNew(J_NUM); v->num = x; return v; }
JVal* JStrVal(const char* s) { JVal* v = JNew(J_STR); v->str = xstrdup(s); return v; }

void ArrAdd(JVal* a, JVal* v) {
    a->items = (JVal**)realloc(a->items, (a->count + 1) * sizeof(JVal*));
    a->items[a->count++] = v;
}

void ObjAdd(JVal* o, const char* k, JVal* v) {
    o->keys = (char**)realloc(o->keys, (o->count + 1) * sizeof(char*));
    o->vals = (JVal**)realloc(o->vals, (o->count + 1) * sizeof(JVal*));
    o->keys[o->count] = xstrdup(k);
    o->vals[o->count] = v;
    o->count++;
}

static void JsonFreeArr(JVal* v) {
    for (int i = 0; i < v->count; i++)
        JsonFree(v->items[i]);
    free(v->items);
}

static void JsonFreeObj(JVal* v) {
    for (int i = 0; i < v->count; i++) {
        free(v->keys[i]);
        JsonFree(v->vals[i]);
    }
    free(v->keys);
    free(v->vals);
}

void JsonFree(JVal* v) {
    if (!v) return;

    if (v->type == J_STR) {
        free(v->str);
    } else if (v->type == J_ARR) {
        JsonFreeArr(v);
    } else if (v->type == J_OBJ) {
        JsonFreeObj(v);
    }
    free(v);
}

static int JsonIndex(JVal* obj, const char* key) {
    for (int i = 0; i < obj->count; i++)
        if (strcmp(obj->keys[i], key) == 0) return i;
    return -1;
}

JVal* JsonGet(JVal* obj, const char* key) {
    if (!obj || obj->type != J_OBJ) return NULL;

    int i = JsonIndex(obj, key);
    return (i >= 0) ? obj->vals[i] : NULL;
}

static int JsonIsNum(JVal* v) {
    return v && v->type == J_NUM;
}

static int JsonIsStr(JVal* v) {
    return v && v->type == J_STR;
}

double JsonNum(JVal* obj, const char* key, double def) {
    JVal* v = JsonGet(obj, key);
    return JsonIsNum(v) ? v->num : def;
}

const char* JsonStr(JVal* obj, const char* key, const char* def) {
    JVal* v = JsonGet(obj, key);
    return JsonIsStr(v) ? v->str : def;
}
