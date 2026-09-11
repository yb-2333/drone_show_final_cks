#ifndef JSON_INTERNAL_H
#define JSON_INTERNAL_H

#include "json.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
    J_NULL,
    J_BOOL,
    J_NUM,
    J_STR,
    J_ARR,
    J_OBJ
} JType;

struct JVal {
    JType  type;
    double num;
    int    b;
    char*  str;
    JVal** items;
    char** keys;
    JVal** vals;
    int    count;
};

char* xstrdup(const char* s);

JVal* JNew(JType t);

JVal* JNumVal(double x);
JVal* JStrVal(const char* s);

void ArrAdd(JVal* a, JVal* v);

void ObjAdd(JVal* o, const char* k, JVal* v);

#endif
