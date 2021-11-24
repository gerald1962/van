# SPDX-License-Identifier: GPL-2.0

# This makefile is used to build the zinc copper battery simulation.

# MODULE ist the name of the executable battery simulation.
MODULE := zinc

# Define the location of the van system.
GLOBALPATH := /home/gerald/van

# Include the makefile to define the pathes for a van system, which is needed
# for a specific battery model
include ${GLOBALPATH}/tools/build/makefile_preamble

# Define additional files needed to build the battery simulation.
VAN_FILES := \
  ${${BUILD_LIB}_FILES} \
  zinc.c

# Include the framework makefile with integration operations.
include ${GLOBALPATH}/tools/build/makefile_appendix

# makefile ends here.
