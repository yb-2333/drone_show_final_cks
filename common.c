#include "common.h"

Color LC[8] = {
    {255, 60,  60,  255},
    {60,  255, 60,  255},
    {60,  120, 255, 255},
    {255, 220, 60,  255},
    {60,  220, 220, 255},
    {230, 60,  200, 255},
    {255, 150, 40,  255},
    {240, 240, 240, 255}
};

const char* LCN[8] = {"Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "Orange", "White"};

Drone D[MAX_DRONES];
int   N = 0;
int   S = -1;
Mode  M = M_INTRO;

Camera3D Cam;

char sx[16] = "0";
char sy[16] = "0";
char sz[16] = "0";
int  ic     = 0;

char wx[16] = "5";
char wy[16] = "5";
char wz[16] = "0";

bool  play  = false;
bool  pause = false;
float spd   = 2;
float prog  = 0;

char  msg[256] = "";
float mt       = 0;
int   txtFocus = 0;

Color Wh = {255, 255, 255, 255};
Color Gr = {150, 150, 165, 255};
Color Bl = {80,  150, 255, 255};
Color Gn = {80,  220, 120, 255};
Color Rd = {255, 80,  80,  255};
Color Ye = {255, 210, 60,  255};

Color Bg = {22,  24,  34,  255};
Color Pn = {32,  34,  46,  245};
Color Br = {55,  55,  75,  255};
Color Bt = {45,  45,  60,  255};
