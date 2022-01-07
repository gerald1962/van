#!/bin/bash

# Start the controller with wait condition, $1 produces cycles and cop trace.
xfce4-terminal --hold --title=controller -e "out/cop -s c -cw -cbc $1 -cdc $2 -t" &

# The controller needs some time to install the shared memory area.
sleep 0.250

# Start the battery with wait condition and cop trace.
xfce4-terminal --hold --title=battery    -e "out/cop -s b -bw -bcc $3         -t" &

# Start the display with wait condition and cop trace.
xfce4-terminal --hold --title=display    -e "out/cop -s d -dw -dcc $4         -t" &

# cop_prototype.sh
