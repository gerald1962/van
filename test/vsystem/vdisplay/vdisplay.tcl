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
# d - describes the display state in the van system.
#
# ep_id:    display end point of the control display cable.
# cv:       canvas id.
# c_x:      current x coordinate.
# c_y:      current y coordinatd.
# b_state:  if 1, the battery is active.
namespace eval d {
    variable  p        "vD>"
    variable  ep_id
    variable  cv
    variable  c_x
    variable  c_y
    variable  b_state  0
    variable  cycle
}

# Connect the display-controller cable.
set d::ep_id [vcable /van/display]

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


# disp_out_exec() - update the display items and send the calculations results
# to the controller.
#
# Return:     None.
#
proc disp_batt_exec { b_state b_color } {
    # Find all items with the "b_button" tags.
    set list [$d::cv find withtag b_light]

    # Color all items yellow with the "b" tags.
    foreach item $list {
	# This command is similar to the configure widget command except that it
	# modifies item-specific options for the items given by tagOrId.
	$d::cv itemconfigure $item -fill $b_color
    }

    # Update the battery state.
    set d::b_state  $b_state
    
    # Inform the controller to activate the battery or to initiate the shutdown
    # procedure.
    puts $d::ep_id "button=$d::b_state:"
    
    # Send the signal without buffering.
    flush $d::ep_id
}

# disp_out_calc() - calculate the item display state and the output to the
# controller.
#
# Return:     None.
#
proc disp_batt_calc {} {
    # Evaluate the battery state.
    switch $d::b_state {
	0        {
	    # Update the battery state.
	    disp_batt_exec 1 yellow 
	}
	
	1        {
	    # Update the battery state.
	    disp_batt_exec 0 gray64
	}
	
	default  {
	    # After shutdown there is nothing more to do.
	}
    }
    
}

# disp_stop_calc() - terminate the van system.
#
# Return:     None.
#
proc disp_stop_calc {} {
    # Send the stop signal to the controller.
    puts $d::ep_id "button=2:"
    
    # Send the signal without buffering.
    flush $d::ep_id
    
    # Wait until the output queue is empty
    set n 1
    while { $n > 0 } {
	# Get the number of the pending output bytes.
	set n [fconfigure $d::ep_id -sync]

 	# Wait a few milliseconds
	after 1
    }

    # Print the goodby notificaton.
    puts "vdisplay done"
    
    # Pull out the display plug.
    close $d::ep_id

    # This command deletes the windows given by the window arguments, plus all
    # of their descendants. If a window “.” is deleted then all windows will be
    # destroyed and the application will (normally) exit. 
    destroy .
    
    # End the van system display application.
    exit 0
}

# Create and manipulate the display canvas widget.
# All coordinates related to canvases are stored as floating-point numbers.
canvas .cv -width 600 -height 400 -bg gray92

# Save the canvas id.
set d::cv .cv

# Geometry manager that packs around edges of cavity.
pack $d::cv

# Items of type rectangle appear as rectangular regions on the display. Each
# rectangle may have an outline, a fill, or both. 
set dx  0
set dy  0
set x1 [expr 20 + $dx]
set y1 [expr 20 + $dy]
set x2 [expr 90 + $dx]
set y2 [expr 90 + $dy]
$d::cv create rectangle $x1 $y1 $x2 $y2 -width 0 -fill gold -tags b_stop

set dx  10
set dy  10
set x1 [expr 30 + $dx]
set y1 [expr 30 + $dy]
set x2 [expr 60 + $dx]
set y2 [expr 60 + $dy]
$d::cv create oval $x1 $y1 $x2 $y2 -width 0 -fill red2 -tags b_stop

# A text item displays a string of characters on the screen in one or more
# lines. 
set dx  35
set dy  10
set x1 [expr 20 + $dx]
set y1 [expr 20 + $dy]
set x2 [expr 90 + $dx]
set y2 [expr 90 + $dy]
$d::cv create text $x1 $y1 -justify center -text Stop


# Items of type oval appear as circular or oval regions on the display.
# The arguments x1, y1, x2, and y2 or coordList give the coordinates of two
# diagonally opposite corners of a rectangular region enclosing the oval.
set dx  100
set dy  0
set x1 [expr 20 + $dx]
set y1 [expr 20 + $dy]
set x2 [expr 90 + $dx]
set y2 [expr 90 + $dy]
$d::cv create rectangle $x1 $y1 $x2 $y2 -width 0 -fill gray80 -tags [list b_button b_move]

set dx  110
set dy   10
set x1 [expr 30 + $dx]
set y1 [expr 30 + $dy]
set x2 [expr 60 + $dx]
set y2 [expr 60 + $dy]
$d::cv create oval $x1 $y1 $x2 $y2 -width 2 -fill gray64 -tags [list b_button b_light b_move]

set x1 [expr 34 + $dx]
set y1 [expr 34 + $dy]
set x2 [expr 57 + $dx]
set y2 [expr 57 + $dy]
$d::cv create line $x1 $y1 $x2 $y2 -width 2 -tags [list b_lamp b_move]

set x1 [expr 34 + $dx]
set y1 [expr 57 + $dy]
set x2 [expr 57 + $dx]
set y2 [expr 34 + $dy]
$d::cv create line $x1 $y1 $x2 $y2 -width 2 -tags [list b_lamp b_move]

# A text item displays a string of characters on the screen in one or more
# lines. 
set dx 135
set dy  10
set x1 [expr 20 + $dx]
set y1 [expr 20 + $dy]
$d::cv create text $x1 $y1 -justify center -text Battery -tags [list b_move]


# bind associates command with all the items given by tagOrId such that whenever
# the event sequence given by sequence occurs for one of the items the command
# will be invoked.
# %x, %y
# The x and y fields from the event. For ButtonPress, ButtonRelease, ...
# %x and %y indicate the position of the mouse pointer relative to the receiving
# window.
$d::cv bind b_button <ButtonPress-1> { disp_batt_calc }
$d::cv bind b_lamp   <ButtonPress-1> { disp_batt_calc }
$d::cv bind b_stop   <ButtonPress-1> { disp_stop_calc }


# Create the display boxes for cycle, voltage and current.
set dx  0
set dy  100
set x1 [expr 20  + $dx]
set y1 [expr 20  + $dy]
set x2 [expr 110 + $dx]
set y2 [expr 45  + $dy]
$d::cv create rectangle $x1 $y1 $x2 $y2 -width 2 -fill snow -outline snow3

# Print the time stamp text.
set dx  125
set dy  112
set x1 [expr 20 + $dx]
set y1 [expr 20 + $dy]
$d::cv create text $x1 $y1 -justify left -text Cycles

# Print the cycle counter.
set dx  45
set dy  112
set x1 [expr 20 + $dx]
set y1 [expr 20 + $dy]
set d::cycle  [$d::cv create text $x1 $y1 -justify center -text 0]

# Get the input from the controller.
proc disp_input {} {
    # Read all signal from the display input queue of the display-controller cable.
    while { 1 } {
	set n [gets $d::ep_id buf]

	# Test the length of the input signal.
	if { $n < 1 } {
	    break
	}
	
	# Trace the input from the controller.
	puts "D> INPUT $buf"
	
	# Search string2 for a sequence of characters that exactly match the
	# characters in string1. If found, return the index of the first
	# character in the first such match within string2. If not found,
	# return -1. 
	set n [string first "cycle=" $buf]

	# Test the search result.
	if { $n < 0 } {
	    puts "$d::p error: vcontroller does not send any cycle values."
	}

	# Update the controller time stamp.
	$d::cv itemconfigure $d::cycle -text 1234567

    }
    
    # Start the timer for the display input.
    after 1000 disp_input
}

# Start the timer for the display input.
disp_input

# Communicate with the window manager:

# Pass the string to the window manager for use as the title for the display
# window.
wm title . "vDisplay"

# If width and height are specified, they give the minimum permissible
# dimensions for window.
wm minsize . 600 400

# If width and height are specified, they give the maximum permissible
# dimensions for window.
wm maxsize . 600 400
