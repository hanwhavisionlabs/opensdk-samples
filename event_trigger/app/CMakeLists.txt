cmake_minimum_required(VERSION 3.2.2)

set(CMAKE_CXX_STANDARD 17 )
set(CMAKE_CXX_STANDARD_REQUIRED True )

project(event_trigger)

if (DEFINED SDK_PATH)
    message("SDK_PATH: ${SDK_PATH}")
else ()
    set(SDK_PATH /opt/opensdk)
    message("SDK_PATH: default: ${SDK_PATH}")
endif ()

set(TARGET_EXE ${PROJECT_NAME})

message(STATUS "Compiler")
message(STATUS " - ID       \t: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS " - Version  \t: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " - Path     \t: ${CMAKE_CXX_COMPILER}")

set(COMMON ${SDK_PATH}/common)
set(WISENET_PLATFORM_RELEASE_PATH ${COMMON}/platform/${SOC})
set(WISENET_PRODUCT_RELEASE_PATH ${COMMON}/product/${SOC})
set(APP_SUPPORT_PACKAGE ${COMMON}/AppSupportPackage/${SOC})

message("COMMON: ${COMMON}")
message("WISENET_PLATFORM_RELEASE_PATH: ${WISENET_PLATFORM_RELEASE_PATH}")
message("WISENET_PRODUCT_RELEASE_PATH: ${WISENET_PRODUCT_RELEASE_PATH}")
message("APP_SUPPORT_PACKAGE: ${APP_SUPPORT_PACKAGE}")

include_directories(${WISENET_PLATFORM_RELEASE_PATH}/includes)
include_directories(${WISENET_PRODUCT_RELEASE_PATH}/includes)
include_directories(includes)

message("includes: ${CMAKE_CURRENT_SOURCE_DIR}/includes")

link_directories(${COMMON}/lib)
link_directories(${WISENET_PLATFORM_RELEASE_PATH}/lib)
link_directories(${WISENET_PRODUCT_RELEASE_PATH}/lib)
link_directories(${WISENET_PRODUCT_RELEASE_PATH}/3rd_party/lib)
link_directories(libs)

add_subdirectory(src)
