cmake_minimum_required(VERSION 3.16)
project(AutoClicker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/core/WindowsPlatformAdapter.cpp
    src/core/ClickEngine.cpp
)

set(HEADERS
    src/MainWindow.h
    src/core/PlatformAdapter.h
    src/core/WindowsPlatformAdapter.h
    src/core/ClickEngine.h
)

# Create executable
add_executable(AutoClicker ${SOURCES} ${HEADERS})

# Link Qt libraries
target_link_libraries(AutoClicker PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

# Windows specific libraries
if(WIN32)
    target_link_libraries(AutoClicker PRIVATE
        user32
        psapi
    )
    target_compile_definitions(AutoClicker PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endif()

# Include directories
target_include_directories(AutoClicker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)

# Output directory
set_target_properties(AutoClicker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Install rules (optional)
install(TARGETS AutoClicker DESTINATION bin)