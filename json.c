/******************************************************************************
 *  json.c  -  JSON 值模型核心（构造 / 组装 / 查询 / 释放）
 *
 *  负责 JVal 值的生命周期管理：
 *     - 构造：JNew / JNumVal / JStrVal
 *     - 组装：ArrAdd / ObjAdd
 *     - 查询：JsonGet / JsonNum / JsonStr
 *     - 释放：JsonFree
 *
 *  解析、序列化、场景存取分别放在 json_parse.c / json_emit.c / json_store.c。
 *  JVal 的完整结构定义见 json_internal.h（对外不透明）。
 ******************************************************************************/
#include "json_internal.h"

/* ============================ 小工具 ============================ */

/* 安全的字符串复制（malloc + memcpy，代替非标准的 strdup） */
char* xstrdup(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* 新建一个指定类型的 JSON 值（calloc 保证字段全为 0/NULL） */
JVal* JNew(JType t) {
    JVal* v = (JVal*)calloc(1, sizeof(JVal));
    v->type = t;
    return v;
}

/* 快速构造标量值 */
JVal* JNumVal(double x) { JVal* v = JNew(J_NUM); v->num = x; return v; }
JVal* JStrVal(const char* s) { JVal* v = JNew(J_STR); v->str = xstrdup(s); return v; }

/* 向数组追加一个元素 */
void ArrAdd(JVal* a, JVal* v) {
    a->items = (JVal**)realloc(a->items, (a->count + 1) * sizeof(JVal*));
    a->items[a->count++] = v;
}

/* 向对象追加一个键值对 */
void ObjAdd(JVal* o, const char* k, JVal* v) {
    o->keys = (char**)realloc(o->keys, (o->count + 1) * sizeof(char*));
    o->vals = (JVal**)realloc(o->vals, (o->count + 1) * sizeof(JVal*));
    o->keys[o->count] = xstrdup(k);
    o->vals[o->count] = v;
    o->count++;
}

/* ============================ 释放 ============================ */

/* JsonFree() - 递归释放 JSON 树 */
void JsonFree(JVal* v) {
    if (!v) return;

    if (v->type == J_STR) {
        free(v->str);
    } else if (v->type == J_ARR) {
        for (int i = 0; i < v->count; i++) JsonFree(v->items[i]);
        free(v->items);
    } else if (v->type == J_OBJ) {
        for (int i = 0; i < v->count; i++) {
            free(v->keys[i]);
            JsonFree(v->vals[i]);
        }
        free(v->keys);
        free(v->vals);
    }
    free(v);
}

/* ============================ 查询 ============================ */

/* JsonGet() - 从对象中按键名取子值 */
JVal* JsonGet(JVal* obj, const char* key) {
    if (!obj || obj->type != J_OBJ) return NULL;
    for (int i = 0; i < obj->count; i++)
        if (strcmp(obj->keys[i], key) == 0) return obj->vals[i];
    return NULL;
}

/* JsonNum() - 取数字字段 */
double JsonNum(JVal* obj, const char* key, double def) {
    JVal* v = JsonGet(obj, key);
    return (v && v->type == J_NUM) ? v->num : def;
}

/* JsonStr() - 取字符串字段 */
const char* JsonStr(JVal* obj, const char* key, const char* def) {
    JVal* v = JsonGet(obj, key);
    return (v && v->type == J_STR) ? v->str : def;
}
