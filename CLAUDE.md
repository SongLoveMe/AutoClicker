# AutoClicker 项目规则

## 技术栈位置

### Qt 安装位置
| 组件 | 路径 |
|------|------|
| **Qt 6.5.3 (MinGW)** | `D:\Qt\6.5.3\mingw_64` |
| **Qt 5.15.2 (MinGW)** | `D:\Qt\5.15.2\mingw81_64` |
| **Qt Creator** | `D:\Qt\Tools\QtCreator` |
| **qmake (Qt6)** | `D:\Qt\6.5.3\mingw_64\bin\qmake.exe` |

### 编译器位置
| 编译器 | 版本 | 路径 |
|--------|------|------|
| **Qt MinGW GCC** | 13.1.0 | `D:\Qt\Tools\mingw1310_64\bin\gcc.exe` |
| **Qt MinGW G++** | 13.1.0 | `D:\Qt\Tools\mingw1310_64\bin\g++.exe` |
| **MSYS2 MinGW GCC** | 13.2.0 | `D:\msys64\ucrt64\bin\gcc.exe` |
| **MSYS2 MinGW G++** | 13.2.0 | `D:\msys64\ucrt64\bin\g++.exe` |

### 构建工具位置
| 工具 | 路径 |
|------|------|
| **Qt CMake** | `D:\Qt\Tools\CMake_64\bin\cmake.exe` |
| **系统 CMake** | `C:\Program Files\CMake\bin\cmake.exe` |
| **Qt Ninja** | `D:\Qt\Tools\Ninja\ninja.exe` |
| **MinGW Make** | `D:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe` |

---

## 构建配置

### 环境变量设置
```bash
# 使用 Qt 6.5.3 + MinGW 13.1.0（推荐）
set PATH=D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.5.3\mingw_64\bin;D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;%PATH%
```

### CMake 构建命令
```bash
# 配置项目
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1310_64/bin/gcc.exe -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe -B build

# 构建
cmake --build build

# 运行
./build/AutoClicker.exe
```

---

## 开发规范

### 代码风格
- 使用 C++17 标准
- 遵循 Qt 编码规范
- 类名使用 PascalCase，函数名使用 camelCase
- 成员变量使用 m_ 前缀

### Qt 模块使用
- Qt6Widgets：GUI界面
- Qt6Core：核心功能
- Qt6Gui：图形相关
- Qt6Multimedia：多媒体支持（可选）

### 第三方库
- nlohmann/json：JSON解析
- OpenCV：图像识别
- spdlog：日志系统
- Catch2：单元测试

---

## 约束与禁止事项

### 编译器选择
- **必须使用 Qt MinGW 13.1.0**（`D:\Qt\Tools\mingw1310_64`）
- 不要使用 MSYS2 的 MinGW，因为 Qt 库是用 Qt MinGW 编译的，混用会导致链接问题

### 构建方式
- **必须使用 Ninja** 作为构建系统
- CMake 配置时必须明确指定编译器路径

### Qt 版本
- **优先使用 Qt 6.5.3**
- Qt 5.15.2 仅在需要兼容旧系统时使用

### 平台适配
- Windows 实现优先，使用 Win32 API
- Linux/macOS 适配在后期实现