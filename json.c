/******************************************************************************
 *  json.c  -  JSON 存取模块实现
 *
 *  包含两部分：
 *    1. 一个极简 JSON 库：递归下降解析器 + 序列化器。
 *       - 支持 object / array / string / number / bool / null。
 *       - 解析器从文本逐个字符读，遇 '{' 解析对象、遇 '[' 解析数组，递归处理。
 *    2. 项目场景存取：SaveShow / LoadShow，把无人机数组序列化成 JSON 或还原。
 *
 *  【给初学者】
 *    递归下降解析 = 每个语法结构写一个函数（parseObject/parseArray/parseValue），
 *    函数里遇到子结构就调用对应的函数，层层递归，直到把整段文本读完。
 ******************************************************************************/
#include "json.h"
#include "common.h"
#include <ctype.h>      // isdigit

/* ============================ JSON 值类型 ============================ */
typedef enum {
    J_NULL,     // null
    J_BOOL,     // true / false
    J_NUM,      // 数字
    J_STR,      // 字符串
    J_ARR,      // 数组
    J_OBJ       // 对象
} JType;

/* JSON 值结构 */
struct JVal {
    JType  type;        // 类型标记
    double num;         // J_NUM：数值
    int    b;           // J_BOOL：0/1
    char*  str;         // J_STR：字符串（堆分配）
    JVal** items;       // J_ARR：元素指针数组
    char** keys;        // J_OBJ：键名字符串数组
    JVal** vals;        // J_OBJ：值指针数组
    int    count;       // 元素 / 键值对数量
};

/* ============================ 小工具 ============================ */

/* 安全的字符串复制（malloc + strcpy，代替非标准的 strdup） */
static char* xstrdup(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* 新建一个指定类型的 JSON 值（calloc 保证字段全为 0/NULL） */
static JVal* JNew(JType t) {
    JVal* v = (JVal*)calloc(1, sizeof(JVal));
    v->type = t;
    return v;
}

/* 快速构造标量值 */
static JVal* JNumVal(double x) { JVal* v = JNew(J_NUM); v->num = x; return v; }
static JVal* JStrVal(const char* s) { JVal* v = JNew(J_STR); v->str = xstrdup(s); return v; }

/* 向数组追加一个元素 */
static void ArrAdd(JVal* a, JVal* v) {
    a->items = (JVal**)realloc(a->items, (a->count + 1) * sizeof(JVal*));
    a->items[a->count++] = v;
}

/* 向对象追加一个键值对 */
static void ObjAdd(JVal* o, const char* k, JVal* v) {
    o->keys = (char**)realloc(o->keys, (o->count + 1) * sizeof(char*));
    o->vals = (JVal**)realloc(o->vals, (o->count + 1) * sizeof(JVal*));
    o->keys[o->count] = xstrdup(k);
    o->vals[o->count] = v;
    o->count++;
}

/* ============================ 解析器 ============================ */

/* 解析状态：指向文本和当前读取位置 */
typedef struct {
    const char* s;      // 源文本
    int         pos;    // 当前位置
} Parser;

/* 跳过空白字符（空格、制表符、换行、回车） */
static void skipWs(Parser* p) {
    while (p->s[p->pos] == ' '  || p->s[p->pos] == '\t' ||
           p->s[p->pos] == '\n' || p->s[p->pos] == '\r')
        p->pos++;
}

static JVal* parseValue(Parser* p);

/* parseString() - 解析一个双引号字符串，处理转义字符 */
static char* parseString(Parser* p) {
    p->pos++;                                   // 跳过开头引号
    char buf[1024];                             // 临时缓冲（名字/坐标足够用）
    int  n = 0;

    while (p->s[p->pos] != '"' && p->s[p->pos] != 0) {
        char c = p->s[p->pos];
        if (c == '\\') {                        // 遇到转义
            p->pos++;
            char e = p->s[p->pos];
            switch (e) {
                case 'n':  buf[n++] = '\n'; break;
                case 't':  buf[n++] = '\t'; break;
                case 'r':  buf[n++] = '\r'; break;
                case '"':  buf[n++] = '"';  break;
                case '\\': buf[n++] = '\\'; break;
                default:   buf[n++] = e;    break;
            }
        } else {
            buf[n++] = c;                       // 普通字符直接复制
        }
        p->pos++;
    }
    p->pos++;                                   // 跳过结尾引号
    buf[n] = 0;
    return xstrdup(buf);
}

/* parseNumber() - 解析数字，用 atof 转换，并推进位置到数字结束 */
static double parseNumber(Parser* p) {
    double v = atof(p->s + p->pos);
    while (p->s[p->pos] &&
           (isdigit((unsigned char)p->s[p->pos]) || p->s[p->pos] == '-' ||
            p->s[p->pos] == '+' || p->s[p->pos] == '.'  ||
            p->s[p->pos] == 'e' || p->s[p->pos] == 'E'))
        p->pos++;
    return v;
}

/* parseArray() - 解析 [ ... ] 数组 */
static JVal* parseArray(Parser* p) {
    p->pos++;                                   // 跳过 '['
    JVal* a = JNew(J_ARR);
    skipWs(p);
    if (p->s[p->pos] == ']') { p->pos++; return a; }   // 空数组

    for (;;) {
        ArrAdd(a, parseValue(p));               // 解析一个元素
        skipWs(p);
        if (p->s[p->pos] == ',') { p->pos++; continue; }   // 逗号 → 继续下一个
        if (p->s[p->pos] == ']') { p->pos++; break; }      // 右括号 → 结束
        break;                                  // 非法 → 结束（容错）
    }
    return a;
}

/* parseObject() - 解析 { ... } 对象 */
static JVal* parseObject(Parser* p) {
    p->pos++;                                   // 跳过 '{'
    JVal* o = JNew(J_OBJ);
    skipWs(p);
    if (p->s[p->pos] == '}') { p->pos++; return o; }       // 空对象

    for (;;) {
        skipWs(p);
        char* key = parseString(p);             // 解析键名
        skipWs(p);
        p->pos++;                               // 跳过 ':'
        ObjAdd(o, key, parseValue(p));          // 解析值
        free(key);
        skipWs(p);
        if (p->s[p->pos] == ',') { p->pos++; continue; }   // 逗号 → 下一对
        if (p->s[p->pos] == '}') { p->pos++; break; }      // 右花括号 → 结束
        break;
    }
    return o;
}

/* parseValue() - 按首字符分派到对应的解析函数 */
static JVal* parseValue(Parser* p) {
    skipWs(p);
    char c = p->s[p->pos];

    if (c == '{') return parseObject(p);
    if (c == '[') return parseArray(p);
    if (c == '"') { JVal* v = JNew(J_STR); v->str = parseString(p); return v; }

    /* 字面量 true */
    if (c == 't') { JVal* v = JNew(J_BOOL); v->b = 1; p->pos += 4; return v; }
    /* 字面量 false */
    if (c == 'f') { JVal* v = JNew(J_BOOL); v->b = 0; p->pos += 5; return v; }
    /* 字面量 null */
    if (c == 'n') { p->pos += 4; return JNew(J_NULL); }

    /* 数字 */
    JVal* v = JNew(J_NUM);
    v->num = parseNumber(p);
    return v;
}

/* JsonParse() - 解析入口 */
JVal* JsonParse(const char* text) {
    if (!text) return NULL;
    Parser p = { text, 0 };
    return parseValue(&p);
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

/* 输出带转义的字符串字面量 */
static void emitStr(Str* s, const char* str) {
    sbCh(s, '"');
    for (const char* c = str; *c; c++) {
        switch (*c) {
            case '"':  sbApp(s, "\\\""); break;
            case '\\': sbApp(s, "\\\\"); break;
            case '\n': sbApp(s, "\\n");  break;
            case '\t': sbApp(s, "\\t");  break;
            case '\r': sbApp(s, "\\r");  break;
            default:   sbCh(s, *c);      break;
        }
    }
    sbCh(s, '"');
}

/* 输出缩进（每层 2 空格） */
static void emitIndent(Str* s, int depth) {
    for (int i = 0; i < depth; i++) sbApp(s, "  ");
}

static void emitValue(Str* s, JVal* v, int depth);

/* emitArray / emitObject 也走 emitValue，这里统一处理 */
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

        case J_ARR: {
            sbApp(s, "[\n");
            for (int i = 0; i < v->count; i++) {
                emitIndent(s, depth + 1);
                emitValue(s, v->items[i], depth + 1);
                if (i < v->count - 1) sbCh(s, ',');
                sbCh(s, '\n');
            }
            emitIndent(s, depth);
            sbCh(s, ']');
            break;
        }

        case J_OBJ: {
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
            break;
        }
    }
}

/* JsonEmit() - 序列化入口，返回 malloc 分配的字符串 */
char* JsonEmit(JVal* v) {
    Str s;
    sbInit(&s);
    emitValue(&s, v, 0);
    return s.b;
}

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
