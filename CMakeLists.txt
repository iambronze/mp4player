cmake_minimum_required(VERSION 3.8)
project(mp4player)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 11)

set(HOME $ENV{HOME})
message(STATUS ${HOME})

set(CMAKE_PREFIX_PATH "${HOME}/rv1109/buildroot/output/rockchip_rv1126_rv1109_facial_gate/host/arm-buildroot-linux-gnueabihf/sysroot/usr/lib/cmake/Qt5")
set(QML2_IMPORT_PATH "${HOME}/rv1109/buildroot/output/rockchip_rv1126_rv1109_facial_gate/host/arm-buildroot-linux-gnueabihf/sysroot/usr/qml")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(QT_VERSION 5)
set(REQUIRED_LIBS Core Gui Widgets Multimedia)
set(REQUIRED_LIBS_QUALIFIED Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Multimedia)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g  -fPIC -Wl,--enable-new-dtags -Wextra -Wall")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -g  -fPIC -Wl,--enable-new-dtags -Wextra -Wall")

set(EXECUTABLE_OUTPUT_PATH ${HOME}/project/linux/build)

set(THIRD_PARTY_INCLUDE_DIR ${HOME}/jingxi/build)
set(THIRD_PARTY_LIB_DIR ${HOME}/jingxi/build)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}  -fvisibility=hidden -static-libstdc++")

set(RV1109_ROOT_DIR ${HOME}/rv1109/buildroot/output/rockchip_rv1126_rv1109_facial_gate/host/arm-buildroot-linux-gnueabihf/sysroot/usr)

set(SDK_ROOT_DIR ${CMAKE_SOURCE_DIR})

include_directories(
        ${RV1109_ROOT_DIR}
        ${SDK_ROOT_DIR}
        ${THIRD_PARTY_INCLUDE_DIR}
        ${THIRD_PARTY_INCLUDE_DIR}/libevent/include
        ${THIRD_PARTY_INCLUDE_DIR}/openssl/include
        ${THIRD_PARTY_INCLUDE_DIR}/ffmpeg/include
)

set(SRC_MAIN ${SDK_ROOT_DIR}/main.cc
        ${SDK_ROOT_DIR}/main_app.cc)

file(GLOB_RECURSE SRC_BASE ${SDK_ROOT_DIR}/base/*.c*)

file(GLOB_RECURSE SRC_PLAYER ${SDK_ROOT_DIR}/media/*.c*)

file(GLOB_RECURSE SRC_UI ${SDK_ROOT_DIR}/ui/*.c*)

link_directories(
        ${EXECUTABLE_OUTPUT_PATH}
        ${THIRD_PARTY_LIB_DIR}/libevent/lib
        ${THIRD_PARTY_LIB_DIR}/openssl/lib
        ${THIRD_PARTY_LIB_DIR}/ffmpeg/lib
        ${THIRD_PARTY_LIB_DIR}/x264/lib
        ${RV1109_ROOT_DIR}/lib
)

if (NOT CMAKE_PREFIX_PATH)
    message(WARNING "CMAKE_PREFIX_PATH is not defined, you may need to set it "
            "(-DCMAKE_PREFIX_PATH=\"path/to/Qt/lib/cmake\" or -DCMAKE_PREFIX_PATH=/usr/include/{host}/qt{version}/ on Ubuntu)")
endif ()


find_package(Qt${QT_VERSION} COMPONENTS ${REQUIRED_LIBS} REQUIRED)

add_executable(${PROJECT_NAME} ${SRC_MAIN} ${SRC_BASE} ${SRC_PLAYER} ${SRC_UI} )

target_link_libraries(${PROJECT_NAME} ${REQUIRED_LIBS_QUALIFIED}
        -levent
        -levent_openssl
        -lasound
        -lx264
        -lavcodec
        -lavfilter
        -lavformat
        -lavutil
        -lpostproc
        -lswresample
        -lswscale
        -lcrypto
        -lssl
        -lrga
        -leasymedia
        -lrockchip_mpp
        -lrkaiq
        -lpthread)
