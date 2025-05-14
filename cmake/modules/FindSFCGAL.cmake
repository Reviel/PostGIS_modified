# Find SFCGAL
# ~~~~~~~~~~~~
# Copyright (c) 2024, dameng <yangzhenglong at dameng.com>
# Copyright (c) 2007, Martin Dobias <wonder.sk at gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for SFCGAL library by the given variable SFCGAL_DIR
#
# If it's found it sets SFCGAL_FOUND to TRUE
# and following variables are set:
#    SFCGAL_INCLUDE_DIR
#    SFCGAL_LIBRARY
#    SFCGAL_LIBRARY_DIR
#    SFCGAL_VERSION

# These variable were setted by find_package()
#    SFCGAL_FIND_REQUIRED
#    SFCGAL_FIND_QUIETLY


# find_path and find_library normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing.

# not support APPLE currently

if(NOT SFCGAL_DIR)
  message(FATAL_ERROR "needs set SFCGAL_DIR first")
endif()

find_path(SFCGAL_INCLUDE_DIR NAMES SFCGAL PATHS
  "${SFCGAL_DIR}/include"
  NO_DEFAULT_PATH
)

find_library(SFCGAL_LIBRARY NAMES SFCGAL SFCGALd PATHS
  "${SFCGAL_DIR}/lib"
  "${SFCGAL_DIR}/lib64"
  NO_DEFAULT_PATH
)

get_filename_component(SFCGAL_LIBRARY_DIR ${SFCGAL_LIBRARY} DIRECTORY)

if(SFCGAL_INCLUDE_DIR AND SFCGAL_LIBRARY)
  set(SFCGAL_FOUND TRUE)
endif()

# Extract version information from the header file
if(SFCGAL_INCLUDE_DIR)
    file(STRINGS ${SFCGAL_INCLUDE_DIR}/SFCGAL/version.h _ver_line
         REGEX "^#define SFCGAL_VERSION *\"[0-9]+\\.[0-9]+\\.[0-9]+\""
         LIMIT_COUNT 1)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
           SFCGAL_VERSION "${_ver_line}")
    unset(_ver_line)
endif()

if(SFCGAL_FOUND)
  if(NOT SFCGAL_FIND_QUIETLY)
    message(STATUS "Found SFCGAL library: ${SFCGAL_LIBRARY}")
    message(STATUS "Found SFCGAL headers: ${SFCGAL_INCLUDE_DIR}")
    message(STATUS "SFCGAL version: ${SFCGAL_VERSION}")
  endif()
else()

if(SFCGAL_FIND_REQUIRED) # this variable will be set when 'REQUIRED' in find_package() function
  message(FATAL_ERROR "Could not find SFCGAL")
endif()

endif()