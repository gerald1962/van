# SPDX-License-Identifier: GPL-2.0
#
# Telling GCC where to find the VAN OS interfaces and the VAN shared library.

# Define the root of the VAN platform.
GLOBALPATH := $(abspath ../../..)

# Define the path for the van OS interfaces.
INCPATH := -I${GLOBALPATH}/os

# Path for the van shared libray.
LIBVAN := ${GLOBALPATH}/lib/libvan.so

# Build the python test program.
all:
	gcc ${INCPATH} -g -Wall pot.c -o pot ${LIBVAN}
