# Find PROJ
# ~~~~~~~~~~~~
# Copyright (c) 2024, dameng <yangzhenglong at dameng.com>
# Copyright (c) 2007, Martin Dobias <wonder.sk at gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for proj library by the given variable PROJ_DIR
#
# If it's found it sets PROJ_FOUND to TRUE
# and following variables are set:
#    PROJ_INCLUDE_DIR
#    PROJ_LIBRARY
#    PROJ_LIBRARY_DIR
#    PROJ_VERSION

# These variable were setted by find_package()
#    PROJ_FIND_REQUIRED
#    PROJ_FIND_QUIETLY


# find_path and find_library normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing.

# not support APPLE currently

if(NOT PROJ_DIR)
  message(FATAL_ERROR "needs set PROJ_DIR first")
endif()

find_path(PROJ_INCLUDE_DIR NAMES proj.h PATHS
  "${PROJ_DIR}/include"
  NO_DEFAULT_PATH
)

find_library(PROJ_LIBRARY NAMES proj proj_d PATHS
  "${PROJ_DIR}/lib"
  "${PROJ_DIR}/lib64"
  "${PROJ_DIR}/bin" # for windows
  NO_DEFAULT_PATH
)

get_filename_component(PROJ_LIBRARY_DIR ${PROJ_LIBRARY} DIRECTORY)

if(PROJ_INCLUDE_DIR AND PROJ_LIBRARY)
  set(PROJ_FOUND TRUE)
endif()

# Extract version information from the header file
if(PROJ_INCLUDE_DIR)
    file(STRINGS ${PROJ_INCLUDE_DIR}/proj.h _ver_line_major
         REGEX "^#define PROJ_VERSION_MAJOR *[0-9]+"
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+"
           PROJ_VERSION_MAJOR "${_ver_line_major}")
    unset(_ver_line_major)
    file(STRINGS ${PROJ_INCLUDE_DIR}/proj.h _ver_line_minor
         REGEX "^#define PROJ_VERSION_MINOR *[0-9]+"
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+"
           PROJ_VERSION_MINOR "${_ver_line_minor}")
    unset(_ver_line_minor)
    file(STRINGS ${PROJ_INCLUDE_DIR}/proj.h _ver_line_patch
         REGEX "^#define PROJ_VERSION_PATCH *[0-9]+"
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+"
           PROJ_VERSION_PATCH "${_ver_line_patch}")
    unset(_ver_line_patch)
    set(PROJ_VERSION "${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR}.${PROJ_VERSION_PATCH}")
endif()

if(PROJ_FOUND)
  if(NOT PROJ_FIND_QUIETLY)
    message(STATUS "Found proj library: ${PROJ_LIBRARY}")
    message(STATUS "Found proj headers: ${PROJ_INCLUDE_DIR}")
    message(STATUS "proj version: ${PROJ_VERSION}")
  endif()
else()

if(PROJ_FIND_REQUIRED) # this variable will be set when 'REQUIRED' in find_package() function
  message(FATAL_ERROR "Could not find proj")
endif()

endif()