#ifndef DRONE_H
#define DRONE_H

#include "common.h"

void MakeDrone(void);

void DelDrone(int i);

void DuplicateDrone(int i);

const char* LightName(Light l);

void PrintDrone(const Drone* d);

void FillCoords(const Drone* d);

void ApplySetupCoords(void);

Color RC(Drone* d);

void DD(Drone* d);

void Rst(void);

void Upd(float dt);

void FormCircle(void);

void FormLine(void);

void FormGrid(void);

void FormTransition(int type);

void FormRotate(float deg);

void MakeDemo(void);

#endif
