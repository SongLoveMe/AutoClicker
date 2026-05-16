# 🎯 通用连点器 (AutoClicker)

一个基于 Qt 6 开发的跨平台自动点击工具，支持多种点击模式和自定义配置。

![版本](https://img.shields.io/badge/version-1.0-blue)
![语言](https://img.shields.io/badge/language-C++17-orange)
![框架](https://img.shields.io/badge/framework-Qt%206.5.3-green)
![许可](https://img.shields.io/badge/license-MIT-purple)

---

## ✨ 功能特性

### 点击模式

| 模式 | 说明 | 状态 |
|------|------|------|
| 📍 **固定位置** | 在指定坐标位置连续点击 | ✅ 已实现 |
| 🖱 **跟随鼠标** | 自动跟随当前鼠标位置点击 | ✅ 已实现 |
| 📋 **序列点击** | 按预设坐标序列循环点击 | ✅ 已实现 |
| 🎲 **随机区域** | 在指定区域内随机位置点击 | 🚧 规划中 |
| 🔄 **拖拽模式** | 模拟拖拽操作（按下-移动-释放） | 🚧 规划中 |

### 点击配置

| 参数 | 说明 |
|------|------|
| **点击按键** | 左键、右键、中键 |
| **点击方式** | 单击、双击、三击、长按 |
| **点击间隔** | 可设置最小/最大间隔（毫秒），支持随机波动 |
| **点击次数** | 无限循环或指定次数 |
| **防检测** | 启用随机位置偏移，防止被检测 |

### 热键控制

| 热键 | 功能 |
|------|------|
| **F6** | 开始/停止点击 |
| **F7** | 暂停/继续 |

### 其他特性

- ✅ 中文界面
- ✅ 配置文件保存/加载 (JSON)
- 🚧 脚本录制与执行
- 🚧 窗口绑定功能
- 🚧 图像识别触发

---

## 📖 使用指南

### 界面说明

```
┌─────────────────────────────────────────────────────┐
│  工具栏: [▶ 开始] [⏹ 停止] [⏸ 暂停]              │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌───────────┐  ┌───────────┐  ┌───────────┐       │
│  │ 点击模式  │  │ 参数配置  │  │ 坐标选取  │       │
│  │ ○ 固定位置│  │ 按键: 左键│  │ 当前:(0,0)│       │
│  │ ○ 跟随鼠标│  │ 方式: 单击│  │ [选取坐标]│       │
│  │ ○ 序列点击│  │ 间隔:100ms│  │ [添加序列]│       │
│  │ ○ 随机区域│  │ 次数: 无限│  │ 序列列表  │       │
│  │           │  │ ☑ 防检测 │  │           │       │
│  └───────────┘  └───────────┘  └───────────┘       │
│                                                     │
│  ┌─────────────────────────────────────────────────┐│
│  │ 状态: 就绪 / 运行中 / 已暂停 / 已停止           ││
│  │ 点击次数: 0                                     ││
│  └─────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────┤
│  状态栏: 就绪 | 热键: F6 开始/停止 | F7 暂停       │
└─────────────────────────────────────────────────────┘
```

### 使用步骤

#### 1. 固定位置点击

1. 选择 "📍 固定位置" 模式
2. 点击 "选取坐标" 按钮，将鼠标移到目标位置
3. 配置点击参数（按键、方式、间隔、次数）
4. 点击 "▶ 开始" 或按 **F6** 启动

#### 2. 跟随鼠标点击

1. 选择 "🖱 跟随鼠标" 模式
2. 配置点击参数
3. 点击 "▶ 开始" 或按 **F6** 启动
4. 程序会自动跟随鼠标当前位置点击

#### 3. 序列点击

1. 选择 "📋 序列点击" 模式
2. 点击 "➕ 添加序列" 添加多个坐标点
3. 配置点击参数
4. 点击 "▶ 开始" 启动，程序会按序列循环点击

### 防检测说明

启用 "🛡 防检测随机化" 后：
- 点击位置会有 ±5 像素的随机偏移
- 点击间隔会在设定范围内随机波动
- 模拟更自然的点击行为

---

## 🔧 构建指南

### 环境要求

| 组件 | 版本 | 说明 |
|------|------|------|
| Qt | 6.5.3+ | GUI框架 |
| CMake | 3.16+ | 构建系统 |
| MinGW | 13.1.0 | C++17编译器 |
| Ninja | 最新版 | 构建工具 |

### 开发环境配置

本项目使用以下路径（可根据实际安装位置调整）：

```
Qt 6.5.3:     D:\Qt\6.5.3\mingw_64
MinGW:        D:\Qt\Tools\mingw1310_64
CMake:        D:\Qt\Tools\CMake_64
Ninja:        D:\Qt\Tools\Ninja
```

### 构建步骤

```bash
# 1. 设置环境变量
set PATH=D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.5.3\mingw_64\bin;D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;%PATH%

# 2. 进入项目目录
cd d:\other\Tool_Collection\AutoClicker

# 3. 配置项目
cmake -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1310_64/bin/gcc.exe ^
    -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe ^
    -DCMAKE_PREFIX_PATH=D:/Qt/6.5.3/mingw_64 ^
    -B build

# 4. 构建
cmake --build build

# 5. 部署Qt依赖（生成可独立运行的exe）
D:\Qt\6.5.3\mingw_64\bin\windeployqt.exe build\AutoClicker.exe

# 6. 运行
build\AutoClicker.exe
```

### 输出目录

构建完成后，`build` 目录包含：
- `AutoClicker.exe` - 主程序
- Qt相关DLL文件
- 插件目录（platforms、imageformats等）

---

## 📁 项目结构

```
AutoClicker/
├── README.md                 # 项目说明文档
├── CLAUDE.md                 # 开发规则和配置
├── CMakeLists.txt            # CMake构建配置
├── configs/
│   └── default.json          # 默认配置文件
├── src/
│   ├── main.cpp              # 程序入口
│   ├── MainWindow.h/cpp      # 主窗口
│   ├── core/
│   │   ├── PlatformAdapter.h     # 平台适配接口
│   │   ├── WindowsPlatformAdapter.h/cpp  # Windows实现
│   │   └── ClickEngine.h/cpp     # 点击引擎核心
│   └── utils/
│       └── ConfigManager.h/cpp   # 配置管理
├── build/                    # 构建输出目录
└── .gitignore                # Git忽略规则
```

---

## 🗺 开发计划

### v1.1 规划

- [ ] 完善随机区域模式
- [ ] 实现拖拽模式
- [ ] 添加配置保存/加载菜单
- [ ] 优化界面交互体验

### v1.2 规划

- [ ] 脚本录制与执行
- [ ] 窗口绑定功能
- [ ] 多语言支持（中/英切换）

### v1.3 规划

- [ ] 图像识别触发
- [ ] Linux/macOS平台适配
- [ ] CLI命令行版本

---

## 🤝 参与贡献

欢迎提交 Issue 和 Pull Request！

**仓库地址：** https://github.com/SongLoveMe/AutoClicker

---

## 📜 许可证

本项目采用 MIT 许可证。

```
MIT License

Copyright (c) 2026 SongLoveMe

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 📝 更新日志

### v1.0 (2026-05-16)

- ✅ 项目初始化，基于 Qt 6 + CMake
- ✅ 实现主窗口UI框架
- ✅ Windows平台适配（鼠标模拟、热键）
- ✅ ClickEngine核心功能
- ✅ 配置管理系统
- ✅ 中文界面本地化
- ✅ GitHub仓库建立