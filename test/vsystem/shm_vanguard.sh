#!/bin/bash

# Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>
#
# vanguard.sh - start the van system.

# 1 xfce4-terminal - A Terminal emulator for X
# 1.1 man xfce4-terminal
# 1.1.1 --geometry=geometry
# Sets the geometry of the last-specified window to geometry. Read X(7) for more
# information on how to specify window geometries:
# 
# 2 X - a portable, network-transparent window system
# 2.1 man X 7

# Start the van controller program.
xfce4-terminal --hold --geometry=120x25+25+1000   --title=vcontroller -e "vcontroller/out/vcontroller" &

# Wait for the generation of the shared memory resources.
sleep 0.500

# Start the van display program.
xfce4-terminal --hold --geometry=120x25+25+0      --title=vdisply     -e "vdisplay/vdisplay.tcl" &

# Start the van battery program.
xfce4-terminal --hold --geometry=120x25+1350+1000 --title=vbattery     -e "vbattery/out/vbattery" &

# shm_vanguard.sh
