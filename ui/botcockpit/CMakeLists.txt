cmake_minimum_required(VERSION 3.16)
project(botcockpit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Compatible with Ubuntu 22.04 Qt 6.2.x (no qt_standard_project_setup)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)

add_executable(botcockpit
    src/main.cpp
    src/frame_codec.cpp
    src/robotstate.cpp
    src/nodetablemodel.cpp
    src/connectioncontroller.cpp
    src/socketworker.cpp
    qml/qml.qrc
)

target_include_directories(botcockpit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(botcockpit PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

install(TARGETS botcockpit
    RUNTIME DESTINATION bin
)
