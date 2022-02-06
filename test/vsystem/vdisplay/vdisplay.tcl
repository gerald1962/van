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
# vd - describes the display state in the van system.
#
# @p:        prompt of the display
# @ep_id:    display end point of the control display cable.
# @cv_id     canvas id.
# @cv_w:     width of the canvas.
# @cv_w:     height of the canvas.
# @b_state:  if 1, the battery is active.
namespace eval vd {
    variable  p        "D>"
    variable  ep_id
    variable  cv_id
    variable  cv_w     600
    variable  cv_h     400
    variable  b_state  0

    # bx - box ids of the canvas items.
    #
    # @cyc:  id of the cycle box.
    # @vlt:  id of the voltage box.
    # @crt:  id of the current box.
    namespace eval bx {
	variable  cyc
	variable  vlt
	variable  crt
    }
    
    # ci - input from the controller.
    #
    # cyc:  cycle value or time stamp.
    # vlt:  voltatage value
    # crt:  current value.
    namespace eval ci {
	variable  cyc
	variable  vlt
	variable  crt
    }
}

#============================================================================
# LOCAL FUNCTIONS
# ============================================================================
# disp_stop_exec{} - propagate the termination request to the controller and
# battery.
#
# code:  id of the exit code.
#
# Return:     None.
#
proc disp_stop_exec { code } {
    # Send the stop signal to the controller.
    puts $vd::ep_id "button=2:"
    
    # Send the signal without buffering.
    flush $vd::ep_id
    
    # Wait until the output queue is empty
    set n 1
    while { $n > 0 } {
	# Get the number of the pending output bytes.
	set n [fconfigure $vd::ep_id -sync]

 	# Wait a few milliseconds
	after 1
    }

    # Print the goodby notificaton.
    puts "vdisplay done"
    
    # Pull out the display plug.
    close $vd::ep_id

    # This command deletes the windows given by the window arguments, plus all
    # of their descendants. If a window “.” is deleted then all windows will be
    # destroyed and the application will (normally) exit. 
    destroy .
    
    # End the van system display application.
    exit $code
}

# disp_input_parse{} - parse the input from the controller.
#
# @buf:  input signal from the controller.
#
# Return:     the trap information or TCL_OK.
#
proc disp_input_parse { buf } {
    # Match the regular expression against the controller signal.
    set list [regexp -inline -all -- {[a-z]+|=|[0-9]+|::}  $buf]

    # Count the elements of the controller signal.
    set ll [llength $list]
    if { $ll != 15 } {
	# Generate an error.
	error "wrong format \"$buf\""
    }
    
    # Retrive and test the cycle element.
    if { [lindex $list 0] != "cycle" } {
	error "missing \"cycle\" in \"$buf\""
    }

    # Save the counter value.
    set vd::ci::cyc  [lindex $list 2]
    
    # Retrive and test the voltage element.
    if { [lindex $list 4] != "voltage" } {
	error "missing \"voltage\" in \"$buf\""
    }
    
    # Save the voltage value.
    set vd::ci::vlt  [lindex $list 6]
    
    # Retrive and test the current element.
    if { [lindex $list 8] != "current" } {
	error "missing \"current\" in \"$buf\""
    }
    
    # Save the current value.
    set vd::ci::crt  [lindex $list 10]
    
    return TCL_OK
}

# disp_input{} - get the input from the controller.
#
# Return:     None.
#
proc disp_input {} {
    # Read all signal from the display input queue of the display-controller cable.
    while { 1 } {
	set n [gets $vd::ep_id buf]

	# Test the length of the input signal.
	if { $n < 1 } {
	    break
	}
	
	# Trace the input from the controller.
	puts "$vd::p INPUT $buf"

	# Evaluate the input from the controller and trap exceptional returns
	if { [ catch { disp_input_parse $buf } ] } {
	    set info $::errorInfo
	    puts "Invalid input from the controller: $info"

	    # Propagate the termination request to the controller and battery.
	    disp_stop_exec 1
	}

	# Update the cycle counter.
	$vd::cv_id itemconfigure $vd::bx::cyc -text $vd::ci::cyc
	
	# Update the voltage value.
	$vd::cv_id itemconfigure $vd::bx::vlt -text $vd::ci::vlt
	
	# Update the current value.
	$vd::cv_id itemconfigure $vd::bx::crt -text $vd::ci::crt
    }
    
    # Start the timer for the display input.
    after 1000 disp_input
}

# disp_inp_boxes{} - create the display input boxes.
#
# Return:     None.
#
proc disp_inp_boxes {} {
    # Origin of the coordinates system for the boxes
    set ox  0
    set oy  100
    
    #===========================================================================
    # Cycle box
    # ==========================================================================
    
    # Create the display cycle box.
    set dx  $ox
    set dy  $oy
    set x1  [expr 20  + $dx]
    set y1  [expr 20  + $dy]
    set x2  [expr 110 + $dx]
    set y2  [expr 45  + $dy]
    $vd::cv_id create rectangle $x1 $y1 $x2 $y2 -width 2 -fill snow -outline snow3

    # Print the cycle text.
    set dx  [expr $dx + 125 ]
    set dy  [expr $dy + 12]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    $vd::cv_id create text $x1 $y1 -justify left -text Cycles

    # Print the cycle counter value.
    set dx  [expr $ox + 45]
    set dy  [expr $dy + 0]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    set vd::bx::cyc  [$vd::cv_id create text $x1 $y1 -justify center -text 0]
    
    #===========================================================================
    # Voltage box
    # ==========================================================================
    
    # Create the display voltage box.
    set dx  $ox
    set dy  [expr $oy + 50]
    set x1  [expr 20  + $dx]
    set y1  [expr 20  + $dy]
    set x2  [expr 110 + $dx]
    set y2  [expr 45  + $dy]
    $vd::cv_id create rectangle $x1 $y1 $x2 $y2 -width 2 -fill snow -outline snow3
    
    # Print the voltage text.
    set dx  [expr $dx + 125 ]
    set dy  [expr $dy + 12]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    $vd::cv_id create text $x1 $y1 -justify left -text Voltage
    
    # Print the voltage value.
    set dx  [expr $ox + 45]
    set dy  [expr $dy + 0]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    set vd::bx::vlt  [$vd::cv_id create text $x1 $y1 -justify center -text 0]

    #===========================================================================
    # Current box
    # ==========================================================================
    
    # Create the display current box.
    set dx  $ox
    set dy  [expr $oy + 100]
    set x1  [expr 20  + $dx]
    set y1  [expr 20  + $dy]
    set x2  [expr 110 + $dx]
    set y2  [expr 45  + $dy]
    $vd::cv_id create rectangle $x1 $y1 $x2 $y2 -width 2 -fill snow -outline snow3
    
    # Print the current text.
    set dx  [expr $dx + 125 ]
    set dy  [expr $dy + 12]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    $vd::cv_id create text $x1 $y1 -justify left -text Current
    
    # Print the current value.
    set dx  [expr $ox + 45]
    set dy  [expr $dy + 0]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    set vd::bx::crt  [$vd::cv_id create text $x1 $y1 -justify center -text 0]
}

# disp_out_exec() - update the display items and send the calculations results
# to the controller.
#
# @b_state:  battery state.
# @b_color:  color of the battery lamp.
#
# Return:     None.
#
proc disp_batt_exec { b_state b_color } {
    # Find all items with the "b_button" tags.
    set list [$vd::cv_id find withtag b_light]

    # Color all items yellow with the "b" tags.
    foreach item $list {
	# This command is similar to the configure widget command except that it
	# modifies item-specific options for the items given by tagOrId.
	$vd::cv_id itemconfigure $item -fill $b_color
    }

    # Update the battery state.
    set vd::b_state  $b_state
    
    # Inform the controller to activate the battery or to initiate the shutdown
    # procedure.
    puts $vd::ep_id "button=$vd::b_state:"
    
    # Send the signal without buffering.
    flush $vd::ep_id
}

# disp_out_calc() - calculate the item display state and the output to the
# controller.
#
# Return:     None.
#
proc disp_batt_calc {} {
    # Evaluate the battery state.
    switch $vd::b_state {
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

# disp_batt{} - create the battery control button.
#
# Return:     None.
#
proc disp_batt {} {
    # Items of type oval appear as circular or oval regions on the display.
    # The arguments x1, y1, x2, and y2 or coordList give the coordinates of two
    # diagonally opposite corners of a rectangular region enclosing the oval.
    set dx  100
    set dy  0
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    set x2 [expr 90 + $dx]
    set y2 [expr 90 + $dy]
    $vd::cv_id create rectangle $x1 $y1 $x2 $y2 -width 0 -fill gray80 -tags [list b_button b_move]
    
    set dx  110
    set dy   10
    set x1 [expr 30 + $dx]
    set y1 [expr 30 + $dy]
    set x2 [expr 60 + $dx]
    set y2 [expr 60 + $dy]
    $vd::cv_id create oval $x1 $y1 $x2 $y2 -width 2 -fill gray64 -tags [list b_button b_light b_move]
    
    set x1 [expr 34 + $dx]
    set y1 [expr 34 + $dy]
    set x2 [expr 57 + $dx]
    set y2 [expr 57 + $dy]
    $vd::cv_id create line $x1 $y1 $x2 $y2 -width 2 -tags [list b_lamp b_move]

    set x1 [expr 34 + $dx]
    set y1 [expr 57 + $dy]
    set x2 [expr 57 + $dx]
    set y2 [expr 34 + $dy]
    $vd::cv_id create line $x1 $y1 $x2 $y2 -width 2 -tags [list b_lamp b_move]

    # A text item displays a string of characters on the screen in one or more
    # lines. 
    set dx 135
    set dy  10
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    $vd::cv_id create text $x1 $y1 -justify center -text Battery -tags [list b_move]

    # Bind associates command with all the items given by tagOrId such that whenever
    # the event sequence given by sequence occurs for one of the items the command
    # will be invoked.
    # %x, %y
    # The x and y fields from the event. For ButtonPress, ButtonRelease, ...
    # %x and %y indicate the position of the mouse pointer relative to the receiving
    # window.
    $vd::cv_id bind b_button <ButtonPress-1> { disp_batt_calc }
    $vd::cv_id bind b_lamp   <ButtonPress-1> { disp_batt_calc }
}

# disp_stop{} - create the stop button.
#
# Return:     None.
#
proc disp_stop {} {
    # Items of type rectangle appear as rectangular regions on the display. Each
    # rectangle may have an outline, a fill, or both. 
    set dx  0
    set dy  0
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    set x2 [expr 90 + $dx]
    set y2 [expr 90 + $dy]
    $vd::cv_id create rectangle $x1 $y1 $x2 $y2 -width 0 -fill gold -tags b_stop

    set dx  10
    set dy  10
    set x1 [expr 30 + $dx]
    set y1 [expr 30 + $dy]
    set x2 [expr 60 + $dx]
    set y2 [expr 60 + $dy]
    $vd::cv_id create oval $x1 $y1 $x2 $y2 -width 0 -fill red2 -tags b_stop

    # A text item displays a string of characters on the screen in one or more
    # lines. 
    set dx  35
    set dy  10
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    set x2 [expr 90 + $dx]
    set y2 [expr 90 + $dy]
    $vd::cv_id create text $x1 $y1 -justify center -text Stop
    
    # Bind associates command with all the items given by tagOrId such that whenever
    # the event sequence given by sequence occurs for one of the items the command
    # will be invoked.
    # %x, %y
    # The x and y fields from the event. For ButtonPress, ButtonRelease, ...
    # %x and %y indicate the position of the mouse pointer relative to the receiving
    # window.
    $vd::cv_id bind b_stop  <ButtonPress-1> { disp_stop_exec 0 }
}

# disp_housing{} - create the housing of the display device.
#
# Return:     None.
#
proc disp_housing {} {
    # Create and manipulate the display canvas widget.
    # All coordinates related to canvases are stored as floating-point numbers.
    canvas .cv -width $vd::cv_w -height $vd::cv_h -bg gray92

    # Save the canvas id.
    set vd::cv_id .cv

    # Geometry manager that packs around edges of cavity.
    pack $vd::cv_id
}

# disp_frame{} - configure the housing frame.
#
# Return:     None.
#
proc disp_frame {} {
    # Communicate with the window manager:

    # Pass the string to the window manager for use as the title for the display
    # window.
    wm title . "vDisplay"

    # If width and height are specified, they give the minimum permissible
    # dimensions for window.
    wm minsize . $vd::cv_w $vd::cv_h

    # If width and height are specified, they give the maximum permissible
    # dimensions for window.
    wm maxsize . $vd::cv_w $vd::cv_h
}

# disp_connect{} - complete the display-controller cable.
#
# Return:     None.
#
proc disp_connect {} {
    # The controller has to be started before, otherwise the display plug cannot
    # be inserted.
    set vd::ep_id [vcable /van/display]
}

#============================================================================
# EXPORTED FUNCTIONS
# ============================================================================
# main{} - main process of the van display.
#
# Return:     None.
#
proc main {} {
    # Complete the display-controller cable.
    disp_connect

    # Configure the housing frame.
    disp_frame
    
    # Create the housing of the display device.
    disp_housing
    
    # Create the stop button.
    disp_stop

    # Create the battery control button.
    disp_batt

    # Create the display input boxes.
    disp_inp_boxes

    # Start the timer for the display input from the controller.
    disp_input

}

# Main process of the van display.
main
