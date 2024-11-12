cmake_minimum_required(VERSION 3.12)
project(wgleng)
set(CMAKE_CXX_STANDARD 23)

set(SOURCE_LOC src)
set(DEPS_LOC dependencies)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "${SOURCE_LOC}/*.cpp")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_library(${PROJECT_NAME} STATIC ${SRC_FILES})

set(WGLENG_COMP_OPT
    -O3
    -flto=full
    -fno-exceptions
    --std=c++23
    -sUSE_SDL=2 	
	-DGLM_FORCE_PURE
	-DGLM_ENABLE_EXPERIMENTAL
)
set(WGLENG_LINK_OPT
    -O3
    -flto=full
    --closure 1
    --emit-tsd wasmInterface.d.ts
    -lembind
    -lSDL2
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sENVIRONMENT="web"
    -sMIN_WEBGL_VERSION=2
    -sMAX_WEBGL_VERSION=2
    -sALLOW_MEMORY_GROWTH=1
    -sFILESYSTEM=0
)

# deps
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "Build Bullet2 demos")
set(BUILD_EXTRAS OFF CACHE BOOL "Build Bullet Extras")
set(BUILD_UNIT_TESTS OFF CACHE BOOL "Build Bullet Unit Tests")
set(BUILD_CPU_DEMOS OFF CACHE BOOL "Build Bullet CPU Demos")
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "Build OpenGL3 Demos")
set(BUILD_ROBOTICS OFF CACHE BOOL "Build Bullet Robotics")
add_subdirectory(${DEPS_LOC}/bullet3)
target_include_directories(${PROJECT_NAME} PUBLIC ${DEPS_LOC}/bullet3/src)

target_include_directories(${PROJECT_NAME} PUBLIC ${DEPS_LOC}/glm/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${DEPS_LOC}/entt/include)
target_link_libraries(${PROJECT_NAME} PUBLIC BulletDynamics BulletCollision LinearMath)

file(GLOB_RECURSE DEPS_FILES_FOR_WARNING_DISABLE CONFIGURE_DEPENDS "${DEPS_LOC}/*" "${SOURCE_LOC}/vendor/*")
set_source_files_properties(
  ${DEPS_FILES_FOR_WARNING_DISABLE}
  PROPERTIES
  COMPILE_FLAGS "..."
)

# Options
OPTION(WGLENG_SHADER_HOT_RELOAD "use WGLENG_SHADER_HOT_RELOAD" OFF)
if (WGLENG_SHADER_HOT_RELOAD)
    message(STATUS "Using WGLENG_SHADER_HOT_RELOAD")
    set(WGLENG_COMP_OPT ${WGLENG_COMP_OPT} -DSHADER_HOT_RELOAD)
    set(WGLENG_LINK_OPT ${WGLENG_LINK_OPT} -sFETCH)
endif ()

OPTION(WGLENG_PROFILING "use WGLENG_PROFILING" OFF)
if (WGLENG_PROFILING)
    message(STATUS "Using WGLENG_PROFILING")
    set(WGLENG_COMP_OPT ${WGLENG_COMP_OPT} --profiling)
    set(WGLENG_LINK_OPT ${WGLENG_LINK_OPT} --profiling-funcs -sASSERTIONS)
endif ()

target_compile_options(${PROJECT_NAME} PRIVATE ${WGLENG_COMP_OPT})
target_link_options(${PROJECT_NAME} PRIVATE ${WGLENG_LINK_OPT})

set(WGLENG_COMP_OPT ${WGLENG_COMP_OPT} PARENT_SCOPE)
set(WGLENG_LINK_OPT ${WGLENG_LINK_OPT} PARENT_SCOPE)