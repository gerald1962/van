# SPDX-License-Identifier: GPL-2.0

# Name of the shared libray.
MODULE := libvan.so

# Define the root of the VAN platform.
GLOBALPATH := $(abspath ..)

# Include the makefile to define the pathes for the VAN platform files.
include ${GLOBALPATH}/tools/build/makefile_pathes

# We need to compile our library sources into position-independent code (PIC).
VAN_CC_FLAGS := -fpic

# Define additional files needed to build the research programme.
#
VAN_FILES := \
  ${${BUILD_LIB}_FILES}


# Include the framework makefile with the integration operations.
include ${GLOBALPATH}/tools/build/makefile_operations
