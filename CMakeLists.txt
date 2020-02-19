# version required
cmake_minimum_required(VERSION 2.8)

# set default values, it will be replace by cmake command options:-D <var>:<type>=<value>
set(TOOLCHAINS_PREFIX "/opt/hisi-linux/x86-arm/arm-hisiv500-linux/target/bin/arm-hisiv500-linux-")
# set the root directions
set(LV_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LV_BUILD_ROOT ${CMAKE_BINARY_DIR})

# set the make flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -std=c++11 -Wall -Os -Wsign-compare -fno-rtti -fno-exceptions")

# toolchain
set(CMAKE_CXX_COMPILER ${TOOLCHAINS_PREFIX}g++)
set(CMAKE_C_COMPILER ${TOOLCHAINS_PREFIX}gcc)
message(STATUS "this CMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
message(STATUS "this CMAKE_C_COMPILER=${CMAKE_C_COMPILER}")

# set the source files
file(GLOB_RECURSE LV_CODE_SRC  ${LV_SDK_ROOT}/sample/*.cpp ${LV_SDK_ROOT}/sample/*.c)

# set the include files
set(LV_CODE_INC
        ${LV_SDK_ROOT}/sample
        ${LV_SDK_ROOT}/sample/ipc_operation
        ${LV_SDK_ROOT}/sample/ipc_operation/media_parser
        ${LV_SDK_ROOT}/sample/sdk_api_operation
        ${LV_SDK_ROOT}/linkvisual
        ${LV_SDK_ROOT}/third_party/cJSON-1.7.7
        ${LV_SDK_ROOT}/third_party/linkkit-sdk-c
        ${LV_SDK_ROOT}/third_party/linkkit-sdk-c/include
        ${LV_SDK_ROOT}/third_party/linkkit-sdk-c/include/exports
        ${LV_SDK_ROOT}/third_party/linkkit-sdk-c/include/imports
        )
include_directories(${LV_CODE_INC})

#---------------- ADD LIBS ---------------#
# cjson
add_library(cjson SHARED IMPORTED)
set_target_properties(cjson PROPERTIES IMPORTED_LOCATION ${LV_SDK_ROOT}/third_party/cJSON-1.7.7/build/libcjson.so)
# linkkit
add_library(iot_hal SHARED IMPORTED)
add_library(iot_sdk SHARED IMPORTED)
add_library(iot_tls SHARED IMPORTED)
set_target_properties(iot_hal PROPERTIES IMPORTED_LOCATION ${LV_SDK_ROOT}/third_party/linkkit-sdk-c/out/libiot_hal.so)
set_target_properties(iot_sdk PROPERTIES IMPORTED_LOCATION ${LV_SDK_ROOT}/third_party/linkkit-sdk-c/out/libiot_sdk.so)
set_target_properties(iot_tls PROPERTIES IMPORTED_LOCATION ${LV_SDK_ROOT}/third_party/linkkit-sdk-c/out/libiot_tls.so)
# linkvisual
add_library(link_visual_ipc_static STATIC IMPORTED)
set_target_properties(link_visual_ipc_static PROPERTIES IMPORTED_LOCATION ${LV_SDK_ROOT}/linkvisual/liblink_visual_ipc.a)

# build exe
add_executable(link_visual_demo ${LV_CODE_SRC})
target_link_libraries(link_visual_demo link_visual_ipc_static iot_sdk cjson iot_hal iot_tls pthread rt)

# prepare files
FILE(GLOB LV_SDK_MEDIA_FILES ${LV_SDK_ROOT}/sample/ipc_operation/media_source/*)
install(FILES ${LV_SDK_MEDIA_FILES} DESTINATION ${LV_SDK_ROOT}/build)
