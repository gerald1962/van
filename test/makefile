# SPDX-License-Identifier: GPL-2.0

# Define the root of the VAN platform.
GLOBALPATH := $(abspath ..)

# Include the makefile to define the pathes for the VAN platform files.
include ${GLOBALPATH}/tools/build/makefile_pathes

TARGET     = vat
OBJ_DIR    = ./obj
TEST_PATH  = $(shell pwd)
OBJ_PATH   = $(TEST_PATH)/obj
SRC_PATH  += $(TEST_PATH)

CFLAGS += --coverage -g -m32 -Wall

# List of all sources files, like VAN OS files
ALL_FILES =  \
	${${BUILD_LIB}_FILES} \
	vat.c

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
	@gcc $(ALL_OBJS) ${CFLAGS} -o ${TARGET} -lrt -lpthread

# Include all dependency files
-include $(ALL_DEPS)

# Generation the object and dependency files
$(OBJ_DIR)/%o: %c
	@echo Compiling ${notdir $<}
	@gcc -c ${CFLAGS} -o $@ $<
	@gcc -MM ${CFLAGS} $< | sed -e 's!.*:!$(OBJ_DIR)/&!' > $(@:.o=.d)

$(OBJ_DIR) :
	if [ ! -d $(OBJ_DIR) ] ; then mkdir $(OBJ_DIR) ; fi

# Remove all generated files
clean: rm_obj_dir
	rm -f $(TARGET) *gcov* summary.csv

rm_obj_dir:
	rm -rf $(OBJ_DIR)

# Analyze the code coverage
gcov: clean $(TARGET)
	./$(TARGET) | tee run_gcov.log | (grep -E 'CLAIM|HOSTTEST'; exit 0)
	(cp *.c $(OBJ_DIR); cd $(OBJ_DIR)/; gcov -b $(ALL_FILES)) > gcov.log
	./can.pl $(OBJ_DIR)/*.c.gcov > summary.csv
	cat summary.csv

echovars:
	@echo GLOBALPATH: ${GLOBALPATH}
	@echo "ALL_FILES: ${ALL_FILES}"
	@echo "VPATH: ${VPATH}"
	@echo OBJ_PATH: ${OBJ_PATH}
	@echo "ALL_OBJS: ${ALL_OBJS}"
	@echo "CFLAGS: ${CFLAGS}"
