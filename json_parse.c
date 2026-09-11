#include "json_internal.h"
#include <ctype.h>

typedef struct {
    const char* s;
    int         pos;
} Parser;

static void skipWs(Parser* p) {
    while (p->s[p->pos] == ' '  || p->s[p->pos] == '\t' ||
           p->s[p->pos] == '\n' || p->s[p->pos] == '\r')
        p->pos++;
}

static JVal* parseValue(Parser* p);

static char parseEscape(Parser* p) {
    char e = p->s[p->pos];
    switch (e) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'r':  return '\r';
        case '"':  return '"';
        case '\\': return '\\';
        default:   return e;
    }
}

static char* parseString(Parser* p) {
    p->pos++;
    char buf[1024];
    int  n = 0;

    while (p->s[p->pos] != '"' && p->s[p->pos] != 0) {
        char c = p->s[p->pos];
        if (c == '\\') {
            p->pos++;
            buf[n++] = parseEscape(p);
        } else {
            buf[n++] = c;
        }
        p->pos++;
    }
    p->pos++;
    buf[n] = 0;
    return xstrdup(buf);
}

static int IsNumChar(char c) {
    return isdigit((unsigned char)c) || c == '-' || c == '+' ||
           c == '.' || c == 'e' || c == 'E';
}

static double parseNumber(Parser* p) {
    double v = atof(p->s + p->pos);
    while (p->s[p->pos] && IsNumChar(p->s[p->pos]))
        p->pos++;
    return v;
}

static JVal* parseArray(Parser* p) {
    p->pos++;
    JVal* a = JNew(J_ARR);
    skipWs(p);
    if (p->s[p->pos] == ']') { p->pos++; return a; }

    for (;;) {
        ArrAdd(a, parseValue(p));
        skipWs(p);
        if (p->s[p->pos] == ',') { p->pos++; continue; }
        if (p->s[p->pos] == ']') { p->pos++; break; }
        break;
    }
    return a;
}

static JVal* parseObject(Parser* p) {
    p->pos++;
    JVal* o = JNew(J_OBJ);
    skipWs(p);
    if (p->s[p->pos] == '}') { p->pos++; return o; }

    for (;;) {
        skipWs(p);
        char* key = parseString(p);
        skipWs(p);
        p->pos++;
        ObjAdd(o, key, parseValue(p));
        free(key);
        skipWs(p);
        if (p->s[p->pos] == ',') { p->pos++; continue; }
        if (p->s[p->pos] == '}') { p->pos++; break; }
        break;
    }
    return o;
}

static JVal* parseLiteral(Parser* p) {
    char c = p->s[p->pos];

    if (c == 't') { JVal* v = JNew(J_BOOL); v->b = 1; p->pos += 4; return v; }

    if (c == 'f') { JVal* v = JNew(J_BOOL); v->b = 0; p->pos += 5; return v; }

    if (c == 'n') { p->pos += 4; return JNew(J_NULL); }

    return NULL;
}

static JVal* parseValue(Parser* p) {
    skipWs(p);
    char c = p->s[p->pos];

    if (c == '{') return parseObject(p);
    if (c == '[') return parseArray(p);
    if (c == '"') { JVal* v = JNew(J_STR); v->str = parseString(p); return v; }

    JVal* lit = parseLiteral(p);
    if (lit) return lit;

    JVal* v = JNew(J_NUM);
    v->num = parseNumber(p);
    return v;
}

JVal* JsonParse(const char* text) {
    if (!text) return NULL;
    Parser p = { text, 0 };
    return parseValue(&p);
}
