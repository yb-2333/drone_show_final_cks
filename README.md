# Drone Formation Light Show Simulator v3.0 / 无人机编队灯光秀模拟器

基于 C 语言 + raylib 的无人机编队灯光秀 3D 模拟器，支持轨迹编辑与回放。

## 功能 Features

- **三段式设置** -- Setup/Edit/Show 三种模式，设置与编辑分离
- **轨迹系统** -- 每架无人机可添加多个航点(waypoint)，回放时插值移动
- **平滑轨迹** -- 三种轨迹模式：Linear 直线 / Eased 缓动 / Spline 曲线(Catmull-Rom)
- **灯光效果** -- 关/常亮/闪烁/呼吸/追逐/彩虹 6 种模式，8 种预设颜色
- **边界警告** -- 可配置飞行边界，越界时红色告警
- **安全检测** -- 静态碰撞预判 + 播放中实时告警
- **编队变换** -- 圆形/直线/网格队形之间一键变换
- **复制/镜像** -- 复制整架无人机、沿 X 中线镜像路径（制作对称编队）
- **鼠标拖拽** -- 编辑模式下按住左键拖拽移动选中无人机
- **撤销/重做** -- 快照式 Undo/Redo（Ctrl+Z / Ctrl+Y）
- **回放模式** -- 播放/暂停/停止，时间线拖动，速度调节，轨迹平滑模式切换
- **统计面板** -- 显示无人机/航点数量、路径总长、预计时长、包围盒
- **自检** -- 一键运行核心算法断言测试（T 键 / Self-Test 按钮）
- **文件存取** -- 保存/加载完整场景到 JSON 文件（Ctrl+S / Ctrl+L）

## 编译 Compile

raylib 6.0 库已随项目附带在 `raylib/` 目录（头文件 + 导入库），`raylib.dll` 在项目根目录，无需额外下载。

```bash
gcc common.c utils.c drone.c render.c ui.c input.c safety.c \
    trajectory.c undo.c json.c stats.c test.c main.c \
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
| X/Y/Z 输入框 | 设置新无人机起始坐标 |
| 颜色块 | 选择颜色（8 色） |
| + Create Drone（或 A 键） | 创建无人机 |
| 无人机列表 | 点击某行选中该机并进入编辑模式 |
| Circle/Line/Grid | 把所有无人机排成对应队形 |
| Continue to Edit | 进入编辑模式 |

### 编辑模式 (EDIT)
| 操作 | 功能 |
|------|------|
| 点击 3D 无人机 / Tab | 选中 / 切换选中无人机 |
| 1/2/3 | 灯光 OFF/ON/Blink |
| 灯光按钮 | 6 种模式：OFF/ON/Blink/Pulse/Chase/Rainbow |
| 方向键 / Shift+方向键 | 移动选中无人机（Shift 加速） |
| 鼠标左键拖拽 | 在 3D 区域拖拽移动选中无人机 |
| + Add Waypoint | 添加航点 |
| 航点列表 ▲ ▼ C X | 上移/下移/复制/删除航点 |
| Paste / Clear All | 粘贴 / 清空航点 |
| Delete Drone | 删除选中无人机 |
| Duplicate / Mirror X | 复制无人机 / 沿 X 中线镜像路径 |
| Safety Check | 运行碰撞安全检测 |
| Self-Test / T 键 | 运行核心算法自检 |
| Ctrl+Z / Ctrl+Y | 撤销 / 重做 |
| Ctrl+S / Ctrl+L | 保存 / 加载场景 (show.json) |
| Undo/Redo/Save/Load 按钮 | 撤销/重做/保存/加载（鼠标操作） |

### 回放模式 (SHOW)
| 操作 | 功能 |
|------|------|
| Space | 播放/暂停 |
| Esc | 停止并重置 |
| Speed 滑块 | 调节播放速度 (0.5x - 8x) |
| Linear/Eased/Spline | 切换轨迹平滑模式 |
| Play/Pause/Stop 按钮 | 播放控制 |
| 进度条 | 显示播放进度 |
| 统计面板 | 数量/路径总长/预计时长/已用时间/包围盒 |

## 项目文件

- `main.c` -- 程序入口
- `common.c` / `drone.c` / `render.c` / `ui.c` / `input.c` / `utils.c` / `safety.c` -- 各模块实现（对应同名 `.h` 头文件）
- `trajectory.c` -- 轨迹数学（缓动、Catmull-Rom 样条、位置采样）
- `undo.c` -- 撤销/重做（快照栈）
- `json.c` -- JSON 解析/序列化 + 场景存取
- `stats.c` -- 演出统计（数量、路径长、时长、包围盒）
- `test.c` -- 自检（轨迹/缓动/样条/JSON 断言测试）
- `raylib/` -- raylib 6.0 头文件与导入库
- `raylib.dll` -- raylib 动态链接库（运行时需与 exe 同目录）
- `build.sh` -- 一键编译脚本
- `设计文档.md` / `设计文档.docx` -- 项目设计文档
- `show.json` -- 场景保存文件

## 文件格式

保存的 `show.json` 文件包含:
- `version` / `count` / `pathMode` -- 版本号、无人机数量、轨迹平滑模式
- `drones` 数组 -- 每架无人机的名称、颜色、灯光模式、效果速度、起始位置和航点序列
