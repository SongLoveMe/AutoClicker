# 🎯 通用连点器 (AutoClicker)

一个基于 Qt 6 开发的跨平台自动点击工具，支持多种点击模式和自定义配置。

![版本](https://img.shields.io/badge/version-2.1-blue)
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
| **点击方法** | 无干扰（PostMessage）/ 模拟鼠标（SendInput） |
| **点击间隔** | 固定间隔 + 可选随机扰动（±毫秒） |
| **点击次数** | 无限循环或指定次数 |
| **防检测** | 启用随机位置偏移，防止被检测 |

### 🆕 v2.0 新功能

| 功能 | 说明 |
|------|------|
| 🔴 **录制模式** | 实时录制鼠标点击，自动添加到序列 |
| 🎯 **窗口绑定** | 自动检测/选择目标窗口，窗口相对坐标点击 |
| 🔍 **元素定位** | 通过按钮文字查找UI元素，直接点击 |
| 🔝 **窗口置顶** | 可手动切换窗口保持在最上层 |
| ✏️ **手动输入** | 直接输入 X/Y 坐标添加到序列 |
| 🪄 **无干扰点击** | PostMessage 方式点击，不影响用户鼠标操作 |

### 热键控制

| 热键 | 功能 |
|------|------|
| **F6** | 开始/停止点击 |
| **F7** | 暂停/继续 |

---

## 📖 使用指南

### 界面说明

```
┌─────────────────────────────────────────────────────────────────┐
│  工具栏: [▶ 开始] [⏹ 停止] [⏸ 暂停]                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ 点击模式    │  │ 参数配置    │  │ 目标窗口    │             │
│  │ ○ 固定位置  │  │ 按键: 左键  │  │ 当前窗口... │             │
│  │ ○ 跟随鼠标  │  │ 方式: 单击  │  │ 指定窗口: ↻ │             │
│  │ ○ 序列点击  │  │ 方法: 无干扰│  │ 元素文本... │             │
│  │             │  │ 间隔: 200ms │  │ [查找元素]  │             │
│  │             │  │ ☑ 随机扰动 │  │             │             │
│  │             │  │ 次数: 无限  │  │             │             │
│  │             │  │ ☑ 窗口置顶 │  │             │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ 坐标选取                                                    ││
│  │ 当前坐标: (0, 0)      [选取坐标]                           ││
│  │ 手动输入: X: [0] Y: [0] [添加]                             ││
│  │ [🔴 开始录制] 已录制 0 个点                                ││
│  │ 序列列表: [左] (100, 200) [右] (300, 400)...               ││
│  └─────────────────────────────────────────────────────────────┘│
│                                                                 │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │ 状态: 就绪 / 运行中 / 已暂停 / 已停止                       ││
│  │ 点击次数: 0                                                 ││
│  └─────────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────────┤
│  状态栏: 就绪 | 热键: F6 开始/停止 | F7 暂停                   │
└─────────────────────────────────────────────────────────────────┘
```

### 基础使用步骤

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
2. 通过以下方式添加坐标点：
   - **手动输入**: 输入 X/Y 坐标，点击"添加"
   - **录制模式**: 点击"开始录制"，然后在屏幕上点击
3. 配置点击参数
4. 点击 "▶ 开始" 启动，程序会按序列循环点击

---

## 🆕 新功能详细说明

### 🔴 录制模式

录制模式可以自动捕获您的鼠标点击并添加到序列中：

1. 点击 "🔴 开始录制" 按钮（按钮变为红色）
2. 在屏幕任意位置进行鼠标点击（左键、右键、中键都会被记录）
3. 录制列表显示：`[左] (100, 200)`、`[右] (300, 400)` 等
4. 点击 "⏹ 停止录制" 结束录制

**技术原理**: 使用 Windows 低级鼠标钩子 (WH_MOUSE_LL) 实现全局点击捕获。

### 🎯 窗口绑定

窗口绑定功能让点击自动定位到目标窗口：

1. **自动检测模式**: 默认状态，实时显示当前最上层窗口
2. **指定窗口模式**: 
   - 点击 "🔄" 刷新窗口列表
   - 从下拉菜单选择目标窗口（如 "Chrome"、"记事本"）
   - 所有坐标自动转换为窗口相对坐标
3. **效果**: 即使窗口移动，点击位置仍然正确

**应用场景**: 
- 点击特定应用中的固定位置按钮
- 游戏辅助（窗口模式下坐标不受窗口移动影响）

### 🔍 元素定位

通过按钮文字直接查找并点击UI元素：

1. 确保目标窗口处于前台（或已绑定目标窗口）
2. 在 "元素文本" 输入框输入按钮文字（如 "确定"、"登录"）
3. 点击 "查找" 按钮
4. 找到后会显示元素位置信息：`找到: "登录" @ (320, 150) 80x30`
5. 开始点击后，程序会自动点击该元素

**技术原理**: 使用 EnumChildWindows 遍历窗口子控件，匹配文本内容。

### 🪄 无干扰点击

默认的点击方法，让您可以正常使用鼠标：

- **无干扰 (PostMessage)**: 直接发送点击消息到窗口，完全不影响您的鼠标操作
- **模拟鼠标 (SendInput)**: 传统方式，物理移动鼠标进行点击

**使用建议**: 
- 大多数场景使用 "无干扰" 即可
- 某些游戏可能需要 "模拟鼠标" 方式（如使用 DirectInput 的游戏）

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
# 1. 设置环境变量 (Windows CMD)
set PATH=D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.5.3\mingw_64\bin;D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;%PATH%

# 或 Git Bash
export PATH="/d/Qt/Tools/mingw1310_64/bin:/d/Qt/6.5.3/mingw_64/bin:/d/Qt/Tools/CMake_64/bin:/d/Qt/Tools/Ninja:$PATH"

# 2. 进入项目目录
cd d:\other\Tool_Collection\AutoClicker

# 3. 配置项目
cmake -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1310_64/bin/gcc.exe \
    -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe \
    -B build

# 4. 构建
cmake --build build

# 5. 部署Qt依赖（生成可独立运行的exe）
D:\Qt\6.5.3\mingw_64\bin\windeployqt.exe build\AutoClicker.exe

# 6. 运行
build\AutoClicker.exe
```

---

## 📁 项目结构

```
AutoClicker/
├── README.md                     # 项目说明文档
├── CMakeLists.txt                # CMake构建配置
├── configs/
│   └── default.json              # 默认配置文件
├── src/
│   ├── main.cpp                  # 程序入口
│   ├── MainWindow.h/cpp          # 主窗口
│   ├── core/
│   │   ├── PlatformAdapter.h         # 平台适配接口
│   │   ├── WindowsPlatformAdapter.h/cpp  # Windows实现
│   │   └── ClickEngine.h/cpp         # 点击引擎核心
│   └── utils/
│       └── ConfigManager.h/cpp       # 配置管理
├── build/                        # 构建输出目录
└── .gitignore                    # Git忽略规则
```

---

## 🗺 开发计划

### v2.1 规划

- [ ] 完善窗口绑定深层问题（某些应用不响应 PostMessage）
- [ ] 完善随机区域模式
- [ ] 实现拖拽模式
- [ ] 添加配置保存/加载菜单

### v2.2 规划

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

### v2.1 (2026-05-16)

**🐛 Bug修复**
- ✅ 无干扰模式 - 使用 simulateClickToWindow 直接发送消息到目标窗口
- ✅ 录制逻辑 - 使用窗口身份过滤而非矩形区域，窗口最小化时仍能正确过滤
- ✅ 点击次数 - 默认改为0（防止误触），逻辑修复确保 count=0 时立即停止
- ✅ 窗口置顶 - 默认开启，启动时自动置顶

**🔧 改进**
- ✅ 窗口绑定 - 录制时自动记录窗口ID，点击时追踪窗口位置
- ✅ 配置优化 - 默认配置更安全（clickCount: 0, stayOnTop: true）

### v2.0 (2026-05-16)

**🆕 新功能**
- ✅ 录制模式 - Windows 钩子录制所有鼠标点击（左/右/中键）
- ✅ 窗口绑定 - 自动检测/选择目标窗口，窗口相对坐标点击
- ✅ 元素定位 - 通过按钮文字查找UI元素并点击
- ✅ 无干扰点击 - PostMessage 方式，不影响用户鼠标操作
- ✅ 手动坐标输入 - 直接输入 X/Y 坐标添加到序列
- ✅ 窗口置顶 - 可手动切换窗口保持在最上层

**🔧 改进**
- ✅ 间隔延迟简化 - 固定间隔 + 可选随机扰动（取代 min-max 范围）

### v1.0 (2026-05-16)

- ✅ 项目初始化，基于 Qt 6 + CMake
- ✅ 实现主窗口UI框架
- ✅ Windows平台适配（鼠标模拟、热键）
- ✅ ClickEngine核心功能
- ✅ 配置管理系统
- ✅ 中文界面本地化
- ✅ GitHub仓库建立