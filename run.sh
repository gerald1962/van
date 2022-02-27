#!/bin/bash

# run.sh - prepare the execution of the van system.

# Go to the source code of the van OS system, compile the van OS und build the
# library for the GUI.
cd van/lib; make lib

# Compile the batter
simulation.
cd ../test/vsystem/vbattery; make all

# Compile the control logic.
cd ../vcontroller; make all

# Start the battery simulation.
cd ..; ./vanguard.sh
