# To substitute a variable’s value, write a dollar sign followed by the name of
# the variable in parentheses or braces: either ‘$(foo)’ or ‘${foo}’ is a valid
# reference to the variable foo.

# Change this to the name of the Main class file, without file extension
MAIN_FILE = string

# The shell function communicates with the world outside of make. It does
# command expansion:
# see https://www.gnu.org/software/make/manual

# Calculate the path of the current working directory.
CW := $(shell pwd)

# NUnit is a unit-testing framework for all .Net languages:
# see https://nunit.org

# DotNetwork unit test program path.
NU_EXE_P = $(CW)/NUnit.ConsoleRunner.3.12.0/tools

# DotNetwork unit test lib path.
NU_LIB_P = $(CW)/NUnit.3.13.3/lib/net45

# DotNetwork unit test program.
NU_EXE_N = $(NU_EXE_P)/nunit3-console.exe

# DotNetwork unit test lib name.
NU_LIB_N = $(NU_LIB_P)/nunit.framework.dll

# You can force make to export its value for SHELL by using the export
# directive.
export MONO_PATH=$(NU_EXE_P):$(NU_LIB_P)

# Change this to the depth of the project folders.
# If needed, add a prefix for a common project folder.
NU_SOURCE_FILES = \
	string.cs

# Add needed flags to the compiler.
NU_COMP_FLAGS = -debug -unsafe -target:library -r:$(NU_LIB_N) -out:$(NU_DLL)

# Change to the environment compiler.
NU_COMPILER = mcs

# If needed, change the executable file.
NU_DLL = $(MAIN_FILE).dll

# Add needed flags to the native code generaotr.
NU_CLI_FLAGS = --debug

# ECMA-CLI native code generator (Just-in-Time and Ahead-of-Time):
# Common Language Infrastructure:
NU_CLI_RUN = mono

# If needed, change the remove command according to your system.
RM_CMD = -rm -f $(NU_DLL)

all: $(NU_DLL)

$(NU_DLL): $(NU_SOURCE_FILES)
	@ $(NU_COMPILER) $(NU_SOURCE_FILES) $(NU_COMP_FLAGS)
	@ echo Compiling...

run: all
	@ $(NU_CLI_RUN) $(NU_CLI_FLAGS) $(NU_EXE_N) ./$(NU_DLL) -noresult

clean:
	@ $(RM_CMD)

remake: clean all

vars:
	@ echo "CW       =  " $(CW)
	@ echo "NU_EXE_P =  " $(NU_EXE_P)
	@ echo "NU_LIB_P =  " $(NU_LIB_P)
	@ echo "NU_EXE_N =  " $(NU_EXE_N)
	@ echo "NU_LIB_N =  " $(NU_LIB_N)
	@ echo "MONO_PATH = " $(MONO_PATH)
