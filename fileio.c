/******************************************************************************
 *  fileio.c  -  轨迹文件存取模块实现
 *
 *  文件格式（纯文本，便于查看和调试）：
 *     DRONE_LIGHT_SHOW 1          ← 魔术头 + 版本号
 *     <无人机数量 N>
 *     D <名称> <x> <y> <z> <灯光> <颜色> <路径点数>   ← 每架无人机一行
 *     W <x> <y> <z>               ← 路径点，重复「路径点数」行
 *     ...（下一架无人机 D 行 + 其 W 行，依此类推）
 *
 *  保存内容：名称、起点坐标、灯光模式、颜色、航点序列。
 *  运行时状态（当前位置、播放进度等）不保存，加载后通过 Rst() 复位。
 ******************************************************************************/
#include "fileio.h"     // 自己的头文件
#include "common.h"     // 全局变量：D, N, S, M 等
#include "utils.h"      // Msg() 消息函数
#include "drone.h"      // Rst() 重置回放
#include "safety.h"     // nCollisions / nViolations / safetyChecked（加载后清空）

/* ================================================================
 *  SaveTraj() - 保存轨迹到文件
 *
 *  遍历所有无人机，把名称、起点、灯光、颜色、航点序列写入文件。
 *  返回 1=成功, 0=失败（打开文件失败）。
 * ================================================================ */
int SaveTraj(const char* path) {
    FILE* f = fopen(path, "w");             // 以写文本模式打开
    if (!f) {                               // 打开失败（如目录不可写）
        Msg("Save failed: cannot write %s", path);
        return 0;
    }

    fprintf(f, "DRONE_LIGHT_SHOW 1\n");     // 魔术头 + 版本
    fprintf(f, "%d\n", N);                  // 无人机数量

    for (int i = 0; i < N; i++) {
        Drone* d = &D[i];
        /* 无人机行：名称 起点(x,y,z) 灯光 颜色 路径点数 */
        fprintf(f, "D %s %.3f %.3f %.3f %d %d %d\n",
                d->name, d->start.x, d->start.y, d->start.z,
                d->light, d->color, d->wc);
        /* 每个路径点一行 */
        for (int w = 0; w < d->wc; w++)
            fprintf(f, "W %.3f %.3f %.3f\n",
                    d->wp[w].p.x, d->wp[w].p.y, d->wp[w].p.z);
    }

    fclose(f);
    Msg("Saved %d drone(s) to %s", N, path);
    return 1;
}

/* ================================================================
 *  LoadTraj() - 从文件加载轨迹
 *
 *  读取文件并覆盖当前所有无人机数据，然后复位回放状态，
 *  使加载后的轨迹可直接进入 Show 模式重新演示。
 *  返回 1=成功, 0=失败（文件不存在 / 格式错误）。
 * ================================================================ */
int LoadTraj(const char* path) {
    FILE* f = fopen(path, "r");             // 以读文本模式打开
    if (!f) {
        Msg("Load failed: %s not found", path);
        return 0;
    }

    char line[512];                         // 行缓冲

    /* ---- 校验魔术头 ---- */
    char magic[32];
    int ver;
    if (!fgets(line, sizeof line, f) ||
        sscanf(line, "%31s %d", magic, &ver) != 2 ||
        strcmp(magic, "DRONE_LIGHT_SHOW") != 0) {
        fclose(f);
        Msg("Load failed: not a trajectory file");
        return 0;
    }

    /* ---- 读取无人机数量 ---- */
    int count = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "%d", &count) != 1 ||
        count < 0 || count > MAX_DRONES) {
        fclose(f);
        Msg("Load failed: invalid drone count");
        return 0;
    }

    memset(D, 0, sizeof D);                 // 清空现有无人机

    /* ---- 逐架读取 ---- */
    for (int i = 0; i < count; i++) {
        Drone* d = &D[i];
        char name[MAX_NAME];
        float x, y, z;
        int light, color, wc;

        if (!fgets(line, sizeof line, f) ||
            sscanf(line, "D %19s %f %f %f %d %d %d",
                   name, &x, &y, &z, &light, &color, &wc) != 7) {
            fclose(f);
            Msg("Load failed: bad drone entry #%d", i);
            return 0;
        }
        if (wc > MAX_WP) wc = MAX_WP;       // 防止路径点越界

        strncpy(d->name, name, MAX_NAME - 1);
        d->name[MAX_NAME - 1] = '\0';
        d->start = (Pt){x, y, z};           // 起点
        d->pos   = d->start;                // 当前位置=起点
        d->h     = y;                       // 高度（与起点Y一致，同 MakeDrone）
        d->light = (Light)light;            // 灯光模式
        d->color = color;                   // 颜色
        d->wc    = wc;                      // 路径点数量
        d->act   = 1;                       // 激活
        d->bon   = 1;                       // 闪烁初始为亮

        /* 读取该无人机的所有路径点 */
        for (int w = 0; w < wc; w++) {
            if (!fgets(line, sizeof line, f) ||
                sscanf(line, "W %f %f %f", &x, &y, &z) != 3) {
                fclose(f);
                Msg("Load failed: bad waypoint #%d of drone #%d", w, i);
                return 0;
            }
            d->wp[w].p = (Pt){x, y, z};
        }
    }
    fclose(f);

    /* ---- 复位运行时状态 ---- */
    N = count;                              // 更新数量
    S = count > 0 ? 0 : -1;                 // 选中第一架（或清空）
    if (S >= 0) D[S].sel = 1;
    Rst();                                  // 回放状态归零（pos/ci/fin/play/prog）
    nCollisions  = 0;                       // 清空旧的安全检测结果
    nViolations  = 0;
    safetyChecked = false;

    Msg("Loaded %d drone(s) from %s", count, path);
    return 1;
}
