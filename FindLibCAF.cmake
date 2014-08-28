# - Try to find libcaf
# Once done this will define
#
#  CAF_FOUND    - system has libcaf
#  CAF_INCLUDE  - libcaf include dir
#  CAF_LIBRARY  - link againgst libcaf
#

if (CAF_LIBRARY AND CAF_INCLUDE)
  set(CAF_FOUND TRUE)
else (CAF_LIBRARY AND CAF_INCLUDE)
  find_path(CAF_INCLUDE
    NAMES
      caf/all.hpp
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
      ${CAF_INCLUDE_PATH}
      ${CAF_LIBRARY_PATH}
      ${CMAKE_INCLUDE_PATH}
      ${CMAKE_INSTALL_PREFIX}/include
      ${CAF_ROOT}/include
      ${CAF_ROOT}/libcaf
      ../libcaf
      ../../libcaf
      ../../../libcaf)
  
  if (CAF_INCLUDE) 
    message (STATUS "Header files found ...")
  else (CAF_INCLUDE)
    message (SEND_ERROR "Header files NOT found. Provide absolute path with -DCAF_INCLUDE_PATH=<path-to-header>.")
  endif (CAF_INCLUDE)

  if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    set(CPPA_BUILD_DIR build-gcc)
  elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CPPA_BUILD_DIR build-clang)
  endif ()

  find_library(CAF_LIBRARY
    NAMES
      libcaf_core.so
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      ${CAF_INCLUDE_PATH}
      ${CAF_LIBRARY_PATH}
      ${CMAKE_LIBRARY_PATH}
      ${CMAKE_INSTALL_PREFIX}/lib
      ${LIBRARY_OUTPUT_PATH}
      ${CAF_ROOT}/lib
      ${CAF_ROOT}/build/lib
      ${CAF_ROOT}/libcaf/build/lib
      ${CAF_ROOT}/${CAF_BUILD_DIR}/lib
      ${CAF_ROOT}/libcaf/${CAF_BUILD_DIR}/lib
      ../libcaf/build/lib
      ../../libcaf/build/lib
      ../../../libcaf/build/lib)

  find_library(CAF_LIBRARY_IO
    NAMES
      libcaf_io.so
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      ${CAF_INCLUDE_PATH}
      ${CAF_LIBRARY_PATH}
      ${CMAKE_LIBRARY_PATH}
      ${CMAKE_INSTALL_PREFIX}/lib
      ${LIBRARY_OUTPUT_PATH}
      ${CAF_ROOT}/lib
      ${CAF_ROOT}/build/lib
      ${CAF_ROOT}/libcaf/build/lib
      ${CAF_ROOT}/${CAF_BUILD_DIR}/lib
      ${CAF_ROOT}/libcaf/${CAF_BUILD_DIR}/lib
      ../libcaf/build/lib
      ../../libcaf/build/lib
      ../../../libcaf/build/lib)

  find_library(CAF_LIBRARY_OPENCL
    NAMES
      libcaf_opencl.so
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      ${CAF_INCLUDE_PATH}
      ${CAF_LIBRARY_PATH}
      ${CMAKE_LIBRARY_PATH}
      ${CMAKE_INSTALL_PREFIX}/lib
      ${LIBRARY_OUTPUT_PATH}
      ${CAF_ROOT}/lib
      ${CAF_ROOT}/build/lib
      ${CAF_ROOT}/libcaf/build/lib
      ${CAF_ROOT}/${CAF_BUILD_DIR}/lib
      ${CAF_ROOT}/libcaf/${CAF_BUILD_DIR}/lib
      ../libcaf/build/lib
      ../../libcaf/build/lib
      ../../../libcaf/build/lib)

  if (CAF_LIBRARY AND CAF_LIBRARY_IO)
    message (STATUS "Library found ...")
  else ()
    message (SEND_ERROR "Library NOT found. Provide absolute path with -DCAF_LIBRARY_PATH=<path-to-library>.")
  endif ()

  if (CAF_INCLUDE AND CAF_LIBRARY)
    set(CAF_FOUND TRUE)
    set(CAF_INCLUDE ${CAF_INCLUDE})
    set(CAF_LIBRARY ${CAF_LIBRARY})
  else (CAF_INCLUDE AND CAF_LIBRARY)
    message (FATAL_ERROR "CAF LIBRARY AND/OR HEADER NOT FOUND!")
  endif (CAF_INCLUDE AND CAF_LIBRARY)
endif (CAF_LIBRARY AND CAF_INCLUDE)

