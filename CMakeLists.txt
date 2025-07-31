cmake_minimum_required(VERSION 3.16)
project(untitled VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Concurrent)

set(PROJECT_SOURCES
    osn.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(untitled
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${PROJECT_HEADERS}
        ${PROJECT_RESOURCES}
    )
else()
    add_executable(untitled
        ${PROJECT_SOURCES}
        ${PROJECT_HEADERS}
        ${PROJECT_RESOURCES}
    )
endif()

target_link_libraries(untitled PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Concurrent
)

if(WIN32)
    set_target_properties(untitled PROPERTIES
        WIN32_EXECUTABLE TRUE
        SUFFIX ".exe"
    )
endif()

install(TARGETS untitled
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(untitled)
endif()
