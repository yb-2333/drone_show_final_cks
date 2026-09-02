#!/usr/bin/env bash
# 一键编译脚本：编译多文件源码并生成 drone_light_show.exe
# 用法（在项目根目录执行）：bash build.sh
set -e
cd "$(dirname "$0")"

gcc common.c utils.c drone.c render.c ui.c input.c safety.c main.c \
    -o drone_light_show.exe \
    -Iraylib/include -Lraylib/lib \
    -lraylibdll -lopengl32 -lgdi32 -lwinmm

echo "✓ 编译完成: drone_light_show.exe"
echo "  运行: ./drone_light_show.exe"
