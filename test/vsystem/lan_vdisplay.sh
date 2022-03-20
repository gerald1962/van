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

# Start the van display program.
sleep 0.500
xfce4-terminal --hold --geometry=120x25+25+0      --title=vdisply     -e "vdisplay/vdisplay.tcl 10.0.2.15 192.168.178.96" &

# lan_vdisplay.sh
