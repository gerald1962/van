# SPDX-License-Identifier: GPL-2.0

# This makefile is used to build the zinc copper battery simulation.

# MODULE ist the name of the executable battery simulation.
MODULE := zinc


# Ensure that GLOBALPATH points to the project/view root!
GLOBALPATH := /home/gerald/van


# FEAT_XX
#
FEATURE =


# List of the van platform sources
include ${GLOBALPATH}/os/makefile


# Define additional files needed to build the battery simulation.
VAN_FILES := \
  ${${BUILD_LIB}_FILES} \
  zinc.c


# Define their pathes.
VAN_SRC := \
    ${GLOBALPATH}/tools/trap \
    ${GLOBALPATH}/os


# Define additional include pathes needed to build the battery simulation.
VAN_INC :=  $(addprefix -I, ${VAN_SRC})


# Define special flags needed to build the battery simulation.
#
VAN_CC_FLAGS  =
VAN_LD_FLAGS  =


# Uncommment this line to log all executed commands.
V = "command line"


# Include the framework makefile.
include ${GLOBALPATH}/tools/build/Makefile.include

# makefile ends here.
