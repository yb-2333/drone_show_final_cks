/******************************************************************************
 *  json_internal.h  -  JSON 模块内部共享定义（不对外公开）
 *
 *  这里存放 JSON 值类型 JVal 的完整结构，以及构造/追加等内部工具函数的
 *  声明。json.h 里只暴露不透明类型 JVal 和公共 API，真正的字段定义在这里。
 *
 *  被 json.c（值模型）、json_parse.c（解析器）、json_emit.c（序列化器）、
 *  json_store.c（场景存取）共同包含。
 ******************************************************************************/
#ifndef JSON_INTERNAL_H
#define JSON_INTERNAL_H

#include "json.h"       // 公共接口 + JVal 前向声明
#include <stdlib.h>     // size_t / malloc / realloc / free
#include <string.h>     // strlen / memcpy / strcmp / strcpy

/* ============================ JSON 值类型 ============================ */
typedef enum {
    J_NULL,     // null
    J_BOOL,     // true / false
    J_NUM,      // 数字
    J_STR,      // 字符串
    J_ARR,      // 数组
    J_OBJ       // 对象
} JType;

/* JSON 值结构（对外不透明，只在 json_*.c 内部可见） */
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

/* ============================ 内部共享小工具 ============================ */

/* 安全的字符串复制（malloc + memcpy，代替非标准的 strdup） */
char* xstrdup(const char* s);

/* 新建一个指定类型的 JSON 值（calloc 保证字段全为 0/NULL） */
JVal* JNew(JType t);

/* 快速构造标量值 */
JVal* JNumVal(double x);
JVal* JStrVal(const char* s);

/* 向数组追加一个元素 */
void ArrAdd(JVal* a, JVal* v);

/* 向对象追加一个键值对 */
void ObjAdd(JVal* o, const char* k, JVal* v);

#endif  /* JSON_INTERNAL_H */
