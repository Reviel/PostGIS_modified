# Find JSON_C
# ~~~~~~~~~~~~
# Copyright (c) 2024, dameng <yangzhenglong at dameng.com>
# Copyright (c) 2007, Martin Dobias <wonder.sk at gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for json-c library by the given variable JSON_C_DIR
#
# If it's found it sets JSON_C_FOUND to TRUE
# and following variables are set:
#    JSON_C_INCLUDE_DIR
#    JSON_C_LIBRARY
#    JSON_C_LIBRARY_DIR
#    JSON_C_VERSION

# These variable were setted by find_package()
#    JSON_C_FIND_REQUIRED
#    JSON_C_FIND_QUIETLY


# find_path and find_library normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing.

# not support APPLE currently

if(NOT JSON_C_DIR)
  message(FATAL_ERROR "needs set JSON_C_DIR first")
endif()

find_path(JSON_C_INCLUDE_DIR NAMES json-c PATHS
  "${JSON_C_DIR}/include"
  NO_DEFAULT_PATH
)

find_library(JSON_C_LIBRARY NAMES json-c PATHS
  "${JSON_C_DIR}/lib"
  "${JSON_C_DIR}/lib64"
  NO_DEFAULT_PATH
)

get_filename_component(JSON_C_LIBRARY_DIR ${JSON_C_LIBRARY} DIRECTORY)

if(JSON_C_INCLUDE_DIR AND JSON_C_LIBRARY)
  set(JSON_C_FOUND TRUE)
endif()

# Extract version information from the header file
if(JSON_C_INCLUDE_DIR)
    file(STRINGS ${JSON_C_INCLUDE_DIR}/json-c/json_c_version.h _ver_line
	 REGEX "^#define JSON_C_VERSION  *\"[0-9]+\\.[0-9]+\""
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+\\.[0-9]+"
           JSON_C_VERSION "${_ver_line}")
    unset(_ver_line)
endif()

if(JSON_C_FOUND)
  if(NOT JSON_C_FIND_QUIETLY)
    message(STATUS "Found json-c library: ${JSON_C_LIBRARY}")
    message(STATUS "Found json-c headers: ${JSON_C_INCLUDE_DIR}")
    message(STATUS "json-c version: ${JSON_C_VERSION}")
  endif()
else()

if(JSON_C_FIND_REQUIRED) # this variable will be set when 'REQUIRED' in find_package() function
  message(FATAL_ERROR "Could not find json-c")
endif()

endif()