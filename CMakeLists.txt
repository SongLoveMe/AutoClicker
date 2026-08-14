cmake_minimum_required(VERSION 3.16)
project(AutoClicker VERSION 2.4.0 LANGUAGES CXX)

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
    src/core/SequencePointGeometry.cpp
    src/core/VisualDetector.cpp
    src/core/WorkflowStore.cpp
    src/utils/ConfigManager.cpp
    resources/resources.qrc
)

set(HEADERS
    src/MainWindow.h
    src/core/PlatformAdapter.h
    src/core/WindowsPlatformAdapter.h
    src/core/ClickEngine.h
    src/core/SequencePointGeometry.h
    src/core/VisualDetector.h
    src/core/Workflow.h
    src/core/WorkflowStore.h
    src/utils/ConfigManager.h
)

# Create a GUI executable on Windows so launching the .exe does not open a console.
if(WIN32)
    add_executable(AutoClicker WIN32 ${SOURCES} ${HEADERS} resources/autoclicker.rc)
else()
    add_executable(AutoClicker ${SOURCES} ${HEADERS})
endif()

# Link Qt libraries
target_link_libraries(AutoClicker PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

# The CMake project version is the canonical version for the executable and installer.
target_compile_definitions(AutoClicker PRIVATE
    AUTOCLICKER_VERSION="${PROJECT_VERSION}"
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

include(CTest)
if(BUILD_TESTING)
    add_executable(AutoClickerCoreTests
        tests/WorkflowCoreTests.cpp
        src/core/WindowsPlatformAdapter.cpp
        src/core/ClickEngine.cpp
        src/core/SequencePointGeometry.cpp
        src/core/VisualDetector.cpp
        src/core/WorkflowStore.cpp
    )
    target_link_libraries(AutoClickerCoreTests PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)
    if(WIN32)
        target_link_libraries(AutoClickerCoreTests PRIVATE user32 psapi)
        target_compile_definitions(AutoClickerCoreTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
    target_include_directories(AutoClickerCoreTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    )
    add_test(NAME AutoClickerCoreTests COMMAND AutoClickerCoreTests)
endif()

# Install rules
install(TARGETS AutoClicker RUNTIME DESTINATION .)
