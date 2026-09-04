/******************************************************************************
 *  json.h  -  JSON 存取模块声明
 *
 *  一个极简 JSON 库（递归下降解析器 + 序列化器），并在此之上提供
 *  项目的场景存取接口：
 *    SaveShow() - 把当前所有无人机保存成 JSON 文件
 *    LoadShow() - 从 JSON 文件还原无人机
 *
 *  JVal 是不透明类型（内部结构只在 json.c 中定义），
 *  外部通过 JsonParse / JsonGet / JsonNum / JsonStr / JsonEmit 访问。
 ******************************************************************************/
#ifndef JSON_H
#define JSON_H

#include "common.h"

/* JSON 值的不透明类型（内部定义见 json.c） */
typedef struct JVal JVal;

/* ==================== 通用 JSON 库接口 ==================== */

/* 解析一段 JSON 文本，返回根值（文本非法时返回 NULL） */
JVal* JsonParse(const char* text);

/* 递归释放一个 JSON 值（会一并释放所有子节点） */
void JsonFree(JVal* v);

/* 把 JSON 值序列化为带缩进的字符串（返回值用 malloc 分配，需 free） */
char* JsonEmit(JVal* v);

/* 从 JSON 对象中按键名取值，找不到或不是对象时返回 NULL */
JVal* JsonGet(JVal* obj, const char* key);

/* 取对象中的数字字段，缺省返回 def */
double JsonNum(JVal* obj, const char* key, double def);

/* 取对象中的字符串字段，缺省返回 def */
const char* JsonStr(JVal* obj, const char* key, const char* def);

/* ==================== 项目级场景存取 ==================== */

/* 保存当前场景（D[]/N/pathMode）到 JSON 文件，返回 1 成功 / 0 失败 */
int SaveShow(const char* path);

/* 从 JSON 文件加载场景，返回 1 成功 / 0 失败。调用成功后应再调 Rst() 重置回放 */
int LoadShow(const char* path);

#endif  /* JSON_H */
