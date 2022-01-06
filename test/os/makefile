# SPDX-License-Identifier: GPL-2.0

# Makefile for COST - coverage of the operating system test

# Define the root of the VAN platform.
GLOBALPATH := $(abspath ../..)

# Include the makefile to define the pathes for the VAN platform files.
include ${GLOBALPATH}/tools/build/makefile_pathes

# Name of the test program
TARGET     = vote

# Integration paths for the test system
OBJ_DIR    = ./obj
TEST_PATH  = $(shell pwd)
OBJ_PATH   = $(TEST_PATH)/obj
SRC_PATH  += $(TEST_PATH)

# Compiler settings:
# --coverage
#     Compiler and Linker instruments the code for coverage analysis.
# -g
#     Produce debugging information.
# -m32
# -m64
#     Generate code for 32-bit or 64-bit ABI.
#
# Wall
#     Enable all compiler warnings.
#
# CFLAGS += --coverage -g -Wall
CFLAGS += --coverage -g -Wall -DUSE_TCL_STUBS -ICLINC

# Libraries for the linker:
# rt
#     Real-time extension.
#
# pthread
#     The POSIX thread library
#
# LIBS = -lrt -lpthread
LIBS = -lrt -lpthread -LCLLIB -ltclstub

# List of all sources files, like VAN OS files
ALL_FILES =  \
	${${BUILD_LIB}_FILES} \
	vote.c \
	vote_but.c \
	vote_cob.c

# List of all includes paths, like VAN OS directory
ALL_INCS += \
    $(SRC_PATH)

# Search paths for gcc
vpath %.c  ${SRC_PATH}

# Create the object and dependency list
ALL_OBJS := $(addprefix ${OBJ_DIR}/, $(notdir ${ALL_FILES:.c=.o}))
ALL_DEPS := $(addprefix ${OBJ_DIR}/, $(notdir ${ALL_FILES:.c=.d}))

CFLAGS += $(addprefix -I, ${ALL_INCS})

# Call of the gcc linker
$(TARGET): $(OBJ_DIR) $(ALL_OBJS)
	@echo Linking
	@gcc $(ALL_OBJS) ${CFLAGS} -o ${TARGET} ${LIBS}

# Include all dependency files
-include $(ALL_DEPS)

# Generate the object and dependency files
$(OBJ_DIR)/%o: %c
	@echo Compiling ${notdir $<}
	@gcc -c ${CFLAGS} -o $@ $<
	@gcc -MM ${CFLAGS} $< | sed -e 's!.*:!$(OBJ_DIR)/&!' > $(@:.o=.d)

# Directory for the generated files
$(OBJ_DIR) :
	if [ ! -d $(OBJ_DIR) ] ; then mkdir $(OBJ_DIR) ; fi

# Remove all generated files including the obj subdirectory
clean: rm_obj_dir
	rm -f $(TARGET) core error.txt gcov.txt summary.txt test.txt

rm_obj_dir:
	rm -rf $(OBJ_DIR)

# Extract information from the code coverage with the script can.pl
can: clean $(TARGET)
	./$(TARGET) | tee error.txt | (grep -E 'MISMATCH|FAILURE'; exit 0)
	./$(TARGET) | tee test.txt | (grep -E 'FAILURE|TEST'; exit 0)
	(cd $(OBJ_DIR)/; gcov -b $(ALL_FILES)) > gcov.txt
	./can.pl $(OBJ_DIR)/*.c.gcov > summary.txt
	cat summary.txt

# Debug makefile variables
echovars:
	@echo GLOBALPATH: ${GLOBALPATH}
	@echo "ALL_FILES: ${ALL_FILES}"
	@echo "CFLAGS: ${CFLAGS}"
