/******************************************************************************
 *  json_parse.c  -  JSON 解析器（递归下降）
 *
 *  从文本逐个字符读取，遇 '{' 解析对象、遇 '[' 解析数组，递归处理。
 *
 *  【给初学者】
 *  递归下降解析 = 每个语法结构写一个函数（parseObject/parseArray/parseValue），
 *  函数里遇到子结构就调用对应的函数，层层递归，直到把整段文本读完。
 ******************************************************************************/
#include "json_internal.h"
#include <ctype.h>      // isdigit

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

/* parseEscape() - 解析一个转义字符（反斜杠后的字符）
 * 返回它代表的实际字符：\n→换行、\t→制表符等，未知转义原样返回。 */
static char parseEscape(Parser* p) {
    char e = p->s[p->pos];                  // 反斜杠后的那个字符
    switch (e) {
        case 'n':  return '\n';             // 换行
        case 't':  return '\t';             // 制表符
        case 'r':  return '\r';             // 回车
        case '"':  return '"';              // 双引号
        case '\\': return '\\';             // 反斜杠本身
        default:   return e;                // 未知转义：原样返回该字符
    }
}

/* parseString() - 解析一个双引号字符串，处理转义字符 */
static char* parseString(Parser* p) {
    p->pos++;                                   // 跳过开头引号
    char buf[1024];                             // 临时缓冲（名字/坐标足够用）
    int  n = 0;

    while (p->s[p->pos] != '"' && p->s[p->pos] != 0) {
        char c = p->s[p->pos];
        if (c == '\\') {                        // 遇到转义
            p->pos++;                           // 跳过反斜杠，指向转义字符
            buf[n++] = parseEscape(p);          // 解析转义并写入缓冲
        } else {
            buf[n++] = c;                       // 普通字符直接复制
        }
        p->pos++;
    }
    p->pos++;                                   // 跳过结尾引号
    buf[n] = 0;
    return xstrdup(buf);
}

/* IsNumChar() - 判断字符是否属于数字（含符号/小数点/指数标记） */
static int IsNumChar(char c) {
    return isdigit((unsigned char)c) || c == '-' || c == '+' ||
           c == '.' || c == 'e' || c == 'E';
}

/* parseNumber() - 解析数字，用 atof 转换，并推进位置到数字结束 */
static double parseNumber(Parser* p) {
    double v = atof(p->s + p->pos);
    while (p->s[p->pos] && IsNumChar(p->s[p->pos]))
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

/* parseLiteral() - 解析 true/false/null 字面量。
 * 首字符匹配则返回对应的 JSON 值并推进位置，否则返回 NULL。 */
static JVal* parseLiteral(Parser* p) {
    char c = p->s[p->pos];

    /* 字面量 true：t r u e 共 4 个字符 */
    if (c == 't') { JVal* v = JNew(J_BOOL); v->b = 1; p->pos += 4; return v; }
    /* 字面量 false：f a l s e 共 5 个字符 */
    if (c == 'f') { JVal* v = JNew(J_BOOL); v->b = 0; p->pos += 5; return v; }
    /* 字面量 null：n u l l 共 4 个字符 */
    if (c == 'n') { p->pos += 4; return JNew(J_NULL); }

    return NULL;
}

/* parseValue() - 按首字符分派到对应的解析函数 */
static JVal* parseValue(Parser* p) {
    skipWs(p);
    char c = p->s[p->pos];

    if (c == '{') return parseObject(p);
    if (c == '[') return parseArray(p);
    if (c == '"') { JVal* v = JNew(J_STR); v->str = parseString(p); return v; }

    /* 字面量 true/false/null */
    JVal* lit = parseLiteral(p);
    if (lit) return lit;

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
