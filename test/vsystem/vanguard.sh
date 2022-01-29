#!/bin/bash

# vanguard.sh - start the van system.

# Start the van controller program.
xfce4-terminal --hold --title=vcontroller -e "vcontroller/out/vcontroller" &

# Start the van display program.
sleep 0.1
cd vdisplay; ./vdisplay.tcl &

# Start the van battery program.
# cd ../vbattery; ./vbattery.tcl &
cd ..; xfce4-terminal --hold --title=vbattery -e "vbattery/out/vbattery" &

# vanguard.sh
