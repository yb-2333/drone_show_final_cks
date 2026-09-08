/******************************************************************************
 *  json_emit.c  -  JSON 序列化器
 *
 *  把内存中的 JSON 树（JVal）转成带缩进、可读的文本字符串。
 *  用一个可增长的字符串缓冲（Str）逐段拼出完整 JSON。
 ******************************************************************************/
#include "json_internal.h"
#include <stdio.h>      // snprintf

/* ============================ 序列化 ============================ */

/* 可增长的字符串缓冲（用于拼出完整 JSON 文本） */
typedef struct {
    char* b;        // 缓冲区
    int   len;      // 当前长度
    int   cap;      // 容量
} Str;

static void sbInit(Str* s) {
    s->cap = 256;
    s->len = 0;
    s->b = (char*)malloc(s->cap);
    s->b[0] = 0;
}

static void sbApp(Str* s, const char* t) {
    int n = (int)strlen(t);
    if (s->len + n + 1 > s->cap) {
        while (s->len + n + 1 > s->cap) s->cap *= 2;
        s->b = (char*)realloc(s->b, s->cap);
    }
    strcpy(s->b + s->len, t);
    s->len += n;
}

static void sbCh(Str* s, char c) { char t[2] = { c, 0 }; sbApp(s, t); }

/* emitEscape() - 输出单个字符，遇到需转义的字符写出反斜杠形式 */
static void emitEscape(Str* s, char c) {
    switch (c) {
        case '"':  sbApp(s, "\\\""); break;     // 双引号 → \"
        case '\\': sbApp(s, "\\\\"); break;     // 反斜杠 → \\
        case '\n': sbApp(s, "\\n");  break;     // 换行 → \n
        case '\t': sbApp(s, "\\t");  break;     // 制表符 → \t
        case '\r': sbApp(s, "\\r");  break;     // 回车 → \r
        default:   sbCh(s, c);       break;     // 普通字符原样输出
    }
}

/* 输出带转义的字符串字面量 */
static void emitStr(Str* s, const char* str) {
    sbCh(s, '"');
    for (const char* c = str; *c; c++)
        emitEscape(s, *c);                      // 逐字符输出（含转义）
    sbCh(s, '"');
}

/* 输出缩进（每层 2 空格） */
static void emitIndent(Str* s, int depth) {
    for (int i = 0; i < depth; i++) sbApp(s, "  ");
}

static void emitValue(Str* s, JVal* v, int depth);

/* emitArray() - 序列化数组 [ ... ]，每个元素一行、逐层缩进 */
static void emitArray(Str* s, JVal* v, int depth) {
    sbApp(s, "[\n");
    for (int i = 0; i < v->count; i++) {
        emitIndent(s, depth + 1);
        emitValue(s, v->items[i], depth + 1);
        if (i < v->count - 1) sbCh(s, ',');
        sbCh(s, '\n');
    }
    emitIndent(s, depth);
    sbCh(s, ']');
}

/* emitObject() - 序列化对象 { ... }，每个键值对一行、逐层缩进 */
static void emitObject(Str* s, JVal* v, int depth) {
    sbApp(s, "{\n");
    for (int i = 0; i < v->count; i++) {
        emitIndent(s, depth + 1);
        emitStr(s, v->keys[i]);
        sbApp(s, ": ");
        emitValue(s, v->vals[i], depth + 1);
        if (i < v->count - 1) sbCh(s, ',');
        sbCh(s, '\n');
    }
    emitIndent(s, depth);
    sbCh(s, '}');
}

/* emitValue() - 按类型分派序列化：标量直接写，数组/对象交给上面两个 */
static void emitValue(Str* s, JVal* v, int depth) {
    if (!v) { sbApp(s, "null"); return; }

    switch (v->type) {
        case J_NULL: sbApp(s, "null"); break;
        case J_BOOL: sbApp(s, v->b ? "true" : "false"); break;
        case J_NUM: {
            char t[32];
            snprintf(t, sizeof(t), "%g", v->num);
            sbApp(s, t);
            break;
        }
        case J_STR: emitStr(s, v->str); break;
        case J_ARR: emitArray(s, v, depth); break;
        case J_OBJ: emitObject(s, v, depth); break;
    }
}

/* JsonEmit() - 序列化入口，返回 malloc 分配的字符串 */
char* JsonEmit(JVal* v) {
    Str s;
    sbInit(&s);
    emitValue(&s, v, 0);
    return s.b;
}
