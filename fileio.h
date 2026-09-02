/******************************************************************************
 *  fileio.h  -  轨迹文件存取模块声明
 *
 *  实现「数据回放」：把当前所有无人机的轨迹（名称、起点、灯光、航点序列）
 *  保存到 .trajectory 文件，之后能读回并重新演示整个灯光秀。
 ******************************************************************************/
#ifndef FILEIO_H
#define FILEIO_H

#include "common.h"     // 需要 Drone 结构体等类型定义

/* 默认的轨迹文件名 */
#define TRAJ_FILENAME "show.trajectory"

/* 保存当前所有无人机的轨迹到指定文件，返回 1=成功, 0=失败 */
int SaveTraj(const char* path);

/* 从指定文件加载轨迹（覆盖当前无人机），返回 1=成功, 0=失败 */
int LoadTraj(const char* path);

#endif  /* FILEIO_H */
