# Drone Formation Light Show Simulator v3.0 / 无人机编队灯光秀模拟器

基于 C 语言 + raylib 的无人机编队灯光秀 3D 模拟器，支持轨迹编辑与回放。

## 功能 Features

- **三段式设置** -- Setup/Edit/Replay 三种模式，设置与编辑分离
- **轨迹系统** -- 每架无人机可添加多个航点(waypoint)，回放时线性插值移动
- **灯光控制** -- 开关/常亮/闪烁效果，8种预设颜色
- **边界警告** -- 可配置飞行边界，越界时红色告警
- **编队变换** -- 圆形/直线/网格队形之间一键变换
- **回放模式** -- 播放/暂停/停止，时间线拖动，速度调节
- **文件存取** -- 保存/加载完整轨迹数据到 .trajectory 文件

## 编译 Compile

raylib 6.0 库已随项目附带在 `raylib/` 目录（头文件 + 导入库），`raylib.dll` 在项目根目录，无需额外下载。

```bash
gcc common.c utils.c drone.c render.c ui.c input.c safety.c main.c \
    -o drone_light_show.exe \
    -Iraylib/include -Lraylib/lib \
    -lraylibdll -lopengl32 -lgdi32 -lwinmm
```

或直接运行一键脚本：`bash build.sh`

> 说明：`raylib/lib/libraylib.a`（静态库）是 MSVC 编译的，与 MinGW 不兼容，需使用 `-lraylibdll` 动态链接。

## 操作说明

### 设置模式 (SETUP)
| 操作 | 功能 |
|------|------|
| +/- 按钮 或 数字按钮 | 调整无人机数量 (1-100) |
| Circle/Line/Grid 按钮 | 选择编队类型 |
| Size/Height 滑块 | 调整编队尺寸和飞行高度 |
| Initialize 按钮 | 创建编队并进入编辑模式 |

### 编辑模式 (EDIT)
| 操作 | 功能 |
|------|------|
| A / Add Drone | 添加无人机 |
| Delete / Del 按钮 | 删除选中无人机 |
| W / Add Waypoint | 记录当前位置为航点 |
| Del Waypoint | 删除最后一个航点 |
| 1/2/3 | 灯光 OFF/ON/Blink |
| 方向键 | 移动选中无人机 |
| PageUp/PageDown | 调整高度 |
| 右键拖拽 | 在3D视图中拖拽无人机 |
| Tab | 切换选中无人机 |
| F | 聚焦选中无人机 |
| R | 进入回放模式 |
| Ctrl+S | 保存 |
| Ctrl+L | 加载 |
| Circle/Line/Grid 变换 | 编队变换 |
| Check Boundaries | 边界检查 |

### 回放模式 (REPLAY)
| 操作 | 功能 |
|------|------|
| Space | 播放/暂停 |
| Left/Right | 快进/快退 1 秒 |
| Home/End | 跳转到开始/结束 |
| 时间线滑块 | 拖动跳转到任意时间 |
| Speed 滑块 | 调节播放速度 (0.25x - 4.0x) |
| Stop/Play/Pause/Reset | 播放控制按钮 |
| Esc | 返回编辑模式 |

## 项目文件

- `main.c` -- 程序入口
- `drone.c` / `common.c` / `input.c` / `render.c` / `ui.c` / `utils.c` / `safety.c` -- 各模块实现（对应同名 `.h` 头文件）
- `raylib/` -- raylib 6.0 头文件与导入库
- `raylib.dll` -- raylib 动态链接库（运行时需与 exe 同目录）
- `build.sh` -- 一键编译脚本
- `设计文档.md` / `设计文档.docx` -- 项目设计文档
- `*.trajectory` -- 轨迹保存文件

## 文件格式

保存的 .trajectory 文件包含:
- 无人机数量和表演总时长
- 边界配置
- 每架无人机的名称、位置、灯光状态和航点序列
