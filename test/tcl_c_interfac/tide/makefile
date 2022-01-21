# SPDX-License-Identifier: GPL-2.0

# Name of the executable research programme.
MODULE := tide

# Define the root of the VAN platform.
GLOBALPATH := $(abspath ../../..)

# Include the makefile to define the pathes for the VAN platform files.
include ${GLOBALPATH}/tools/build/makefile_pathes

# Define additional files needed to build the research programme.
#
VAN_FILES := \
  ${${BUILD_LIB}_FILES} \
  tide.c

# Include the framework makefile with the integration operations.
include ${GLOBALPATH}/tools/build/makefile_operations
