#ifndef COMMON_H
#define COMMON_H

#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define MAX_DRONES 200
#define MAX_WP      50
#define MAX_NAME    20
#define GROUND      40.0f
#define DR          0.3f
#define PW          240

#define CAM_DIST_MIN  5.0f
#define CAM_DIST_MAX  120.0f
#define CAM_PITCH_MAX 1.5f
#define GRID_LINES    20
#define BLINK_PERIOD  0.5f
#define TERM_PERIOD   0.5f
#define FORM_HEIGHT   5.0f
#define PATH_SAMPLES  48
#define COLOR_COUNT   8
#define DEMO_COUNT    12
#define UI_MSG_TIME   2.5f

typedef enum {
    L_OFF     = 0,
    L_ON,
    L_BLINK,
    L_PULSE,
    L_CHASE,
    L_RAINBOW
} Light;

typedef enum {
    PM_LINEAR = 0,
    PM_EASED,
    PM_SPLINE
} PathMode;

typedef enum {
    M_INTRO = 0,
    M_SETUP,
    M_EDIT,
    M_SHOW
} Mode;

typedef struct {
    float x;
    float y;
    float z;
} Pt;

typedef struct {
    Pt p;
} Waypoint;

typedef struct {
    char    name[MAX_NAME];
    Pt      start;
    Pt      pos;
    float   h;
    Waypoint wp[MAX_WP];
    int     wc;
    Light   light;
    int     color;
    float   bt;
    bool    bon;
    int     ci;
    bool    fin;
    bool    act;
    bool    sel;
    float   flown;
    float   ph;
    float   espeed;
    int     pm;
} Drone;

extern Color LC[8];
extern const char* LCN[8];

extern Drone D[MAX_DRONES];
extern int   N;
extern int   S;
extern Mode  M;

extern Camera3D Cam;

extern char sx[16];
extern char sy[16];
extern char sz[16];
extern int  ic;

extern char wx[16];
extern char wy[16];
extern char wz[16];

extern bool  play;
extern bool  pause;
extern float spd;
extern float prog;

extern char  msg[256];
extern float mt;
extern int   txtFocus;

extern Color Wh;
extern Color Gr;
extern Color Bl;
extern Color Gn;
extern Color Rd;
extern Color Ye;
extern Color Bg;
extern Color Pn;
extern Color Br;
extern Color Bt;

#endif
