# Change this to the name of the Main class file, without file extension
MAIN_FILE = vos_test

# Change this to the depth of the project folders.
# If needed, add a prefix for a common project folder.
CSHARP_SOURCE_FILES = \
	vos_test.cs \
	../os/van_thread.cs

# Add needed flags to the compiler.
CSHARP_COMP_FLAGS = -debug -out:$(EXECUTABLE)

# Change to the environment compiler.
CSHARP_COMPILER = mcs

# If needed, change the executable file.
EXECUTABLE = $(MAIN_FILE).exe

# Add needed flags to the native code generaotr.
CSHARP_CLI_FLAGS = --debug

# ECMA-CLI native code generator (Just-in-Time and Ahead-of-Time):
# Common Language Infrastructure:
CSHARP_CLI_RUN = mono

# If needed, change the remove command according to your system.
RM_CMD = -rm -f $(EXECUTABLE)

all: $(EXECUTABLE)

$(EXECUTABLE): $(CSHARP_SOURCE_FILES)
	@ $(CSHARP_COMPILER) $(CSHARP_SOURCE_FILES) $(CSHARP_COMP_FLAGS)
	@ echo Compiling...

run: all
	@ $(CSHARP_CLI_RUN) $(CSHARP_CLI_FLAGS) ./$(EXECUTABLE)

clean:
	@ $(RM_CMD)

remake: clean all
