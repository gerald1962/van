#!/bin/bash

# Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
#
# run.sh - prepare the execution of the van system.

# Go to the source code of the van OS system, compile the van OS und build the
# library for the GUI.
(cd lib; make lib)

# Compile the battery
(cd test/vsystem/vbattery; make all)

# Compile the controller.
(cd test/vsystem/vcontroller; make all)

# Start the battery simulation.
(cd test/vsystem; ./vanguard.sh)
