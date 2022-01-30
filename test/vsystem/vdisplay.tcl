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

# The namespace command lets you create, access, and destroy separate contexts
# for commands and variables.
#
# ep_id:    id of the display end point.
# cv:       canvas id.
# c_x:      current x coordinate.
# c_y:      current y coordinatd.
# b_state:  if 1, the battery is active.
namespace eval d {
    variable ep_id
    variable cv
    variable c_x
    variable c_y
    variable b_state  0
}

# Connect the display-controller cable.
set d::ep_id [cable /van/display]

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
# fileevent $ep readable [list disp_read $ep]

# disp_b_control() - switch on or off the batter.
#
# Return:     None.
#
proc disp_b_control {} {
    # Find all items with the "b_button" tags.
    set list [$d::cv find withtag b_button]

    # Color all items yellow with the "b" tags.
    foreach item $list {
	# This command is similar to the configure widget command except that it
	# modifies item-specific options for the items given by tagOrId.
	$d::cv itemconfigure $item -fill yellow
    }

    # Inform the controller to activate the battery.
    puts $d::ep_id "battery=1"
    
    # Send the message without buffering.
    flush $d::ep_id
}

# Create and manipulate the display canvas widget.
# All coordinates related to canvases are stored as floating-point numbers.
canvas .cv -width 400 -height 600 -bg gray92

# Save the canvas id.
set d::cv .cv

# Geometry manager that packs around edges of cavity.
pack $d::cv

# Items of type oval appear as circular or oval regions on the display.
# The arguments x1, y1, x2, and y2 or coordList give the coordinates of two
# diagonally opposite corners of a rectangular region enclosing the oval.
$d::cv create oval 20 20 70 70 -width 2 -fill gray64 -tags [list b_button b_move]
$d::cv create oval 30 30 60 60 -width 2 -fill gray64 -tags [list b_button b_move]

$d::cv create line 34 34 57 57 -width 2 -tags [list b_lamp b_move]
$d::cv create line 34 57 57 34 -width 2 -tags [list b_lamp b_move]


# bind associates command with all the items given by tagOrId such that whenever
# the event sequence given by sequence occurs for one of the items the command
# will be invoked.
# %x, %y
# The x and y fields from the event. For ButtonPress, ButtonRelease, ...
# %x and %y indicate the position of the mouse pointer relative to the receiving
# window.
$d::cv bind b_button <ButtonPress-1> { disp_b_control }
$d::cv bind b_lamp   <ButtonPress-1> { disp_b_control }

# disp_b_grab() - grab the battery button.
#
# x:  current x coordinate.
# y:  current y coordinate.
#
# Return:     None.
#
proc disp_b_grab { x y } {
    # Save the current x and y coordinates.
    set d::c_x  $x
    set d::c_y  $y
}

# disp_b_drag() - drag the battery button.
#
# x:  current x coordinate.
# y:  current y coordinate.
#
# Return:     None.
#
proc disp_b_drag { x y } {
    # Calculate the distance.
    set dx [expr {$x - $d::c_x}]
    set dy [expr {$y - $d::c_y}]

    
    $d::cv move b_move $dx $dy
    $d::cv raise b_move
    
    # Save the current x and y coordinates.
    set d::c_x  $x
    set d::c_y  $y                
}

# Move the battery button.
$d::cv bind b_move <ButtonPress-3>   { disp_b_grab %x %y }
$d::cv bind b_move <ButtonRelease-3> { disp_b_drag %x %y }

# Communicate with the window manager: pass the string to the window manager for
# use as the title for the display window.
wm title . "vDisplay"
