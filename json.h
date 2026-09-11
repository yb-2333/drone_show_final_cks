#ifndef JSON_H
#define JSON_H

#include "common.h"

typedef struct JVal JVal;

JVal* JsonParse(const char* text);

void JsonFree(JVal* v);

char* JsonEmit(JVal* v);

JVal* JsonGet(JVal* obj, const char* key);

double JsonNum(JVal* obj, const char* key, double def);

const char* JsonStr(JVal* obj, const char* key, const char* def);

int SaveShow(const char* path);

int LoadShow(const char* path);

#endif
