#include "json_internal.h"
#include <stdio.h>

typedef struct {
    char* b;
    int   len;
    int   cap;
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

static void emitEscape(Str* s, char c) {
    switch (c) {
        case '"':  sbApp(s, "\\\""); break;
        case '\\': sbApp(s, "\\\\"); break;
        case '\n': sbApp(s, "\\n");  break;
        case '\t': sbApp(s, "\\t");  break;
        case '\r': sbApp(s, "\\r");  break;
        default:   sbCh(s, c);       break;
    }
}

static void emitStr(Str* s, const char* str) {
    sbCh(s, '"');
    for (const char* c = str; *c; c++)
        emitEscape(s, *c);
    sbCh(s, '"');
}

static void emitIndent(Str* s, int depth) {
    for (int i = 0; i < depth; i++) sbApp(s, "  ");
}

static void emitValue(Str* s, JVal* v, int depth);

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

char* JsonEmit(JVal* v) {
    Str s;
    sbInit(&s);
    emitValue(&s, v, 0);
    return s.b;
}
