# SPDX-License-Identifier: GPL-2.0

# MODULE ist the name of the executable battery simulation.
MODULE := zinc

# Define the root of the VAN platform.
GLOBALPATH := $(abspath ../../..)

# Include the makefile to define the pathes for the VAN platform files.
include ${GLOBALPATH}/tools/build/makefile_pathes

# Define additional files needed to build the battery simulation.
VAN_FILES := \
  ${${BUILD_LIB}_FILES} \
  zinc.c

# Include the framework makefile with integration operations.
include ${GLOBALPATH}/tools/build/makefile_operations
