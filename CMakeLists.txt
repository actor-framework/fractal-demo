cmake_minimum_required(VERSION 2.8.11)
project(fractal_demo C CXX)

# Add caf submodule
set(CAF_BUILD_STATIC_ONLY yes)
set(CAF_NO_UNIT_TESTS yes)
set(CAF_NO_EXAMPLES yes)
add_subdirectory("actor-framework")

set(FRACTAL_DEMO_VERSION_MAJOR 0)
set(FRACTAL_DEMO_VERSION_MINOR 1)
set(FRACTAL_DEMO_VERSION_PATCH 0)

# Qt5 setup
if(QT_CMAKE_DIR_HINT)
  list(APPEND CMAKE_PREFIX_PATH "${QT_CMAKE_DIR_HINT}")
endif(QT_CMAKE_DIR_HINT)

# run MOC automatically if necessary
set(CMAKE_AUTOMOC on)

# MOC files are generated in the binary tree
set(CMAKE_INCLUDE_CURRENT_DIR on)

set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

find_package(OpenCL REQUIRED)
find_package(Qt5Widgets REQUIRED)

# prohibit in-source builds
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please use "
                        "./configure to choose a build directory and "
                        "initialize the build configuration.")
endif ()

# check if the user provided CXXFLAGS on the command line
if (CMAKE_CXX_FLAGS)
  set(CXXFLAGS_PROVIDED true)
endif ()
if (CXXFLAGS_PROVIDED)
  set(CMAKE_CXX_FLAGS_DEBUG          "")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "")
  set(CMAKE_CXX_FLAGS_RELEASE        "")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
else ()
  set(CMAKE_CXX_FLAGS                "-std=c++14 -Wextra -Wall -pedantic")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O4")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif ()

# set build type (evaluate ENABLE_DEBUG flag)
if (ENABLE_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCAF_DEBUG")
else ()
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

#add_definitions("-DENABLE_OPENCL")

# -pthread is ignored on MacOSX but required on other platforms
if(NOT APPLE AND NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
endif()

qt5_wrap_ui(UI_HDRS controller.ui main.ui)

# we need this to be able to include headers produced by uic in our code
# (CMAKE_BINARY_DIR holds a path to the build directory, while INCLUDE_DIRECTORIES() works just like INCLUDEPATH
# from qmake)
include_directories(. include ${CMAKE_BINARY_DIR} ${OPENCL_INCLUDE_DIRS} ${Qt5Widgets_INCLUDES} actor-framework/libcaf_core/ actor-framework/libcaf_io/)

set(DEMO_SOURCES
    src/cec.cpp
    src/config.cpp
    src/imagelabel.cpp
    src/mainwidget.cpp
    src/image_sink.cpp
    src/controllerwidget.cpp
    src/server.cpp
    src/calculate_fractal.cpp
    src/atoms.cpp
    src/fractal_request_stream.cpp)

file(GLOB DEMO_HEADERS "include/*.hpp" "caf/*.hpp")

add_executable(fractal-demo src/main.cpp ${DEMO_SOURCES} ${DEMO_HEADERS} ${UI_HDRS} ${CAF_HEADERS})
target_link_libraries(fractal-demo ${CMAKE_DL_LIBS} libcaf_core_static libcaf_io_static ${Qt5Widgets_LIBRARIES})
qt5_use_modules(fractal-demo Widgets)

add_executable(cafrun src/cafrun.cpp ${CAF_HEADERS})
target_link_libraries(cafrun ${CMAKE_DL_LIBS} libcaf_core_static libcaf_io_static)

add_executable(controller src/controller.cpp ${DEMO_SOURCES} ${DEMO_HEADERS} ${UI_HDRS} ${CAF_HEADERS})
target_link_libraries(controller ${CMAKE_DL_LIBS} libcaf_core_static libcaf_io_static ${Qt5Widgets_LIBRARIES})
qt5_use_modules(controller Widgets)
