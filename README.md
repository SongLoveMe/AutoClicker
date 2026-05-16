# AutoClicker

A cross-platform auto-clicker application built with C++ and Qt 6.

## Features

- Multiple click modes: fixed position, follow cursor, sequence, random, drag
- Global hotkey support
- Script recording and execution
- Anti-detection randomization
- Window binding
- Configuration profiles

## Build Requirements

- Qt 6.5.3+
- CMake 3.16+
- C++17 compiler (MinGW 13.1.0 recommended)

## Build Instructions

```bash
# Set environment
set PATH=D:\Qt\Tools\mingw1310_64\bin;D:\Qt\6.5.3\mingw_64\bin;D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;%PATH%

# Configure
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -B build

# Build
cmake --build build

# Run
./build/AutoClicker.exe
```

## License

MIT License