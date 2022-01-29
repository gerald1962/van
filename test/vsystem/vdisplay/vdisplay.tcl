#!/usr/bin/tclsh

# SPDX-License-Identifier: GPL-2.0

# Display component of the van system.
#
# Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>

# Evaluates the Tk script when this package is required and then ensures that
# the package is present.
package require Tk

# Load dynmically the van OS into the statically-linked interpreter.
load ../../../lib/libvan[info sharedlibextension]

# Connect the display-controller cable.
set ep [cable /van/display]

# In this setup disp_read will be called with the cable end point as an argument
# whenever $ep becomes readable. 
proc disp_read {ep} {
    # set data [read $chan]
    # puts "[string length $data] $data"
    # if {[eof $chan]} {
        # fileevent $chan readable {}
    # }
    puts "disp_read: ..."
}

# Execute a script when the controller-display wire becomes readable.
fileevent $ep readable [list disp_read $ep]

# Create and manipulate the display canvas widget.
canvas .cv -width 200 -height 200 -bg bisque

# Geometry manager that packs around edges of cavity.
pack .cv

# Communicate with the window manager: pass the string to the window manager for
# use as the title for the display window.
wm title . "vDisplay"
