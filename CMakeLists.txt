cmake_minimum_required(VERSION 2.8.11)
project(fractal_demo C CXX)

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
find_package(CAF COMPONENTS core io riac opencl REQUIRED)

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
  set(CMAKE_CXX_FLAGS                "-std=c++11 -Wextra -Wall -pedantic")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O4")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif ()

# check for g++ >= 4.8 or clang++ > = 3.2
try_run(ProgramResult
        CompilationSucceeded
        ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/src/get_compiler_version.cpp
        RUN_OUTPUT_VARIABLE CompilerVersion)
if (NOT CompilationSucceeded OR NOT ProgramResult EQUAL 0)
  message(FATAL_ERROR "Cannot determine compiler version")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  if (CompilerVersion VERSION_GREATER 4.7)
    message(STATUS "Found g++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "g++ >= 4.8 required (found: ${CompilerVersion}.")
  endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  if (CompilerVersion VERSION_GREATER 3.1)
    message(STATUS "Found clang++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "clang++ >= 3.2 required (found: ${CompilerVersion}.")
  endif ()
  if (NOT CXXFLAGS_PROVIDED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  endif ()
else ()
  message(FATAL_ERROR "Your C++ compiler does not support C++11.")
endif ()

# set build type (evaluate ENABLE_DEBUG flag)
if (ENABLE_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCAF_DEBUG")
else ()
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

add_definitions("-DENABLE_OPENCL")

qt5_wrap_ui(UI_HDRS controller.ui main.ui)

# we need this to be able to include headers produced by uic in our code
# (CMAKE_BINARY_DIR holds a path to the build directory, while INCLUDE_DIRECTORIES() works just like INCLUDEPATH
# from qmake)
include_directories(. include ${CMAKE_BINARY_DIR} ${CAF_INCLUDE_DIRS} ${OPENCL_INCLUDE_DIRS} ${Qt5Widgets_INCLUDES})

set(SOURCES
    src/config.cpp
    src/main.cpp
    src/buffer.cpp
    src/imagelabel.cpp
    src/mainwidget.cpp
    src/serialize_image.cpp
    src/controllerwidget.cpp
    src/server.cpp
    src/calculate_fractal.cpp
    src/atoms.cpp
    src/fractal_request_stream.cpp
    src/controller.cpp)

file(GLOB HEADERS "include/*.hpp")

add_executable(fractal-demo ${SOURCES} ${HEADERS} ${UI_HDRS})
target_link_libraries(fractal-demo ${CMAKE_DL_LIBS} ${CAF_LIBRARIES} ${Qt5Widgets_LIBRARIES} ${OPENCL_LIBRARIES})
qt5_use_modules(fractal-demo Widgets)

