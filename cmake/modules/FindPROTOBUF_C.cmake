# Find PROTOBUF_C
# ~~~~~~~~~~~~
# Copyright (c) 2024, dameng <yangzhenglong at dameng.com>
# Copyright (c) 2007, Martin Dobias <wonder.sk at gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for protobuf_c library by the given variable PROTOBUF_C_DIR
#
# If it's found it sets PROTOBUF_C_FOUND to TRUE
# and following variables are set:
#    PROTOBUF_C_INCLUDE_DIR
#    PROTOBUF_C_LIBRARY
#    PROTOBUF_C_LIBRARY_DIR
#    PROTOBUF_C_VERSION

# These variable were setted by find_package()
#    PROTOBUF_C_FIND_REQUIRED
#    PROTOBUF_C_FIND_QUIETLY


# find_path and find_library normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing.

# not support APPLE currently

if(NOT PROTOBUF_C_DIR)
  message(FATAL_ERROR "needs set PROTOBUF_C_DIR first")
endif()

find_path(PROTOBUF_C_INCLUDE_DIR NAMES protobuf-c.h PATHS
  "${PROTOBUF_C_DIR}/include"
  NO_DEFAULT_PATH
)

find_library(PROTOBUF_C_LIBRARY NAMES protobuf-c PATHS
  "${PROTOBUF_C_DIR}/lib"
  "${PROTOBUF_C_DIR}/lib64"
  NO_DEFAULT_PATH
)

get_filename_component(PROTOBUF_C_LIBRARY_DIR ${PROTOBUF_C_LIBRARY} DIRECTORY)

if(PROTOBUF_C_INCLUDE_DIR AND PROTOBUF_C_LIBRARY)
  set(PROTOBUF_C_FOUND TRUE)
endif()

# Extract version information from the header file
if(PROTOBUF_C_INCLUDE_DIR)
    file(STRINGS ${PROTOBUF_C_INCLUDE_DIR}/protobuf-c.h _ver_line
         REGEX "^#define PROTOBUF_C_VERSION[\t ]*\"[0-9]+\\.[0-9]+\\.[0-9]+\""
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
           PROTOBUF_C_VERSION "${_ver_line}")
    unset(_ver_line)
endif()

if(PROTOBUF_C_FOUND)
  if(NOT PROTOBUF_C_FIND_QUIETLY)
    message(STATUS "Found protobuf-c library: ${PROTOBUF_C_LIBRARY}")
    message(STATUS "Found protobuf-c headers: ${PROTOBUF_C_INCLUDE_DIR}")
    message(STATUS "protobuf-c version: ${PROTOBUF_C_VERSION}")
  endif()
else()

if(PROTOBUF_C_FIND_REQUIRED) # this variable will be set when 'REQUIRED' in find_package() function
  message(FATAL_ERROR "Could not find protobuf-c")
endif()

endif()