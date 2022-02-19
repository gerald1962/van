#!/usr/bin/tclsh

# SPDX-License-Identifier: GPL-2.0

# Display component of the van system.
#
# Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>

# Evaluates the Tk script when this package is required and then ensures that
# the package is present.
package require Tk

# Plotchart is a Tcl-only package that focuses on the easy creation of xy-plots.
package require Plotchart

# Load dynmically the van OS into the statically-linked interpreter.
load ../../../lib/libvan[info sharedlibextension]

# The namespace command lets you create, access, and destroy separate contexts
# for commands and variables.
#
# vd - describes the display state in the van system.
#
# @p:      prompt of the display
# @ep_id:  display end point of the control display cable.
# @wm_w:   width of the main widget forwarded to the windows manager.
# @wm_h:   height of the main widget forwarded to the windows manager.
# @sw:     button widget for the actuators.
# @cw:     widget for the battery charge graph.
# @gw:     widget for any graph.
# @bw:     canvas coordinates for the sensor boxes.
# @bx:     canvas ids of the sensor boxes.
# @ci:     input from the battery controller.
# @co:     output to the battery controller.
namespace eval vd {
    variable  p      "D>"
    variable  ep_id
    variable  wm_w   1400
    variable  wm_h   800

    # sw - button widget for the actuators.
    #
    # @f:  frame widget id.
    # @c:  canvas id.
    # @w:  width of the frame widget and canvas.
    # @w:  height of the frrame widget and canvas.
    namespace eval sw {
	variable  f
	variable  c
	variable  w   250
	variable  h   800
    }

    # cw - capacity widget.
    #
    # @f:      frame widget id.
    # @c:      canvas id.
    # @w:      width of the frame widget and canvas.
    # @w:      height of the frame widget and canvas.
    # @p:      id of the coordinate system for the charge graph.
    # @s:      second counter
    # @x_min:  current start point on the x-time-axis.
    # @x_max:  current end point on the x-time-axis.
    # @x_l:    list of the x coordinates.
    # @y_l:    list of the x coordinates.
    # @x_len:  length of the x-axis in seconds.
    namespace eval cw {
	variable  f
	variable  c
	variable  w      900
	variable  h      400
	variable  p
	variable  x_min    0
	variable  x_max    5
	variable  x_l     ""
	variable  y_l     ""
	variable  x_len    5
	variable  rst      1
    }

    # gw - x graph widget.
    #
    # @f:  frame widget id.
    # @c:  canvas id.
    # @w:  width of the frame widget and canvas.
    # @w:  height of the frrame widget and canvas.
    namespace eval gw {
	variable  f
	variable  c
	variable  w   900
	variable  h   400
    }

    # bw - box widget for the sensors.
    #
    # @f:  frame widget id.
    # @c:  canvas id.
    # @w:  width of the frame widget and canvas.
    # @w:  height of the frrame widget and canvas.
    namespace eval bw {
	variable  f
	variable  c
	variable  w   250
	variable  h   800
    }

    variable  b_state  "B_OFF"

    # bx - box ids of the canvas items.
    #
    # @cyc:  id of the cycle box.
    # @vlt:  id of the voltage box.
    # @crt:  id of the current box.
    namespace eval bx {
	variable  cyc
	variable  vlt
	variable  crt
	variable  cha
    }
    
    # ci - input from the controller.
    #
    # @cyc:  cycle value or time stamp.
    # @vlt:  voltatage value
    # @crt:  current value.
    # @cha:  charging value.
    namespace eval ci {
	variable  cyc
	variable  vlt
	variable  crt
	variable  cha
    }

    # co - output to the controller.
    #
    # @b_ctrl:  current control settings for the battery customers: B_STOP:
    #           shutdown, B_ON: active battery, B_OFF: inactive battery,
    #           B_EMPTY: discharged battery.
    # @b_rech:  recharging state of the battery.
    namespace eval co {
	variable b_ctrl  "B_OFF"
	variable b_rech    
    }
}

#===============================================================================
# LOCAL FUNCTIONS
# ==============================================================================
 # disp_stop_exec{} - propagate the termination request to the controller and
# battery.
#
# code:  id of the exit code.
#
# Return:     None.
#
proc disp_stop_exec { code } {
    # Send the stop signal to the controller.
    puts $vd::ep_id "button=B_STOP:"
    
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

# disp_charge_update{} - update the battery charge graph.
#
# Return:     None.
#
proc disp_charge_update {} {
    # Calculate the number of seconds.
    set x  [expr $vd::ci::cyc / 1000.0 ]

    # Test the restart condition.
    if { $vd::cw::rst == 1 } {
	# Reset the restart condition.
	set vd::cw::rst  0
	
	# If arg is an integer value of the same width as the machine word, returns
	# arg, otherwise converts arg to an integer.
	set x_i  [expr { int($x)} ]

	# Test the current start of the x-axis.
	if { $x_i > $vd::cw::x_min } {
	    # Calculate the ininitial and final value of the x-axis.
	    set vd::cw::x_min  $x_i
	    set vd::cw::x_max  [expr { $vd::cw::x_min + $vd::cw::x_len }]
	}

	# Shift the x-axis.
	$vd::cw::p xconfig -scale [list $vd::cw::x_min $vd::cw::x_max 1]
	
	# Count the number of the list elements.
	set len [llength $vd::cw::x_l]
	
	# Test the length of the coordinate list.
	if { $len > 4 } {
	    # Draw again the deleted graph period.
	    $vd::cw::p plotlist charge $vd::cw::x_l $vd::cw::y_l
	}
    }
    
    # Test the current range of the x-time axis.
    if { $x >= $vd::cw::x_max } {
	# Calculate the ininitial and final value of the x-axis.
	incr vd::cw::x_min
	incr vd::cw::x_max

	# Set the scale parameter for the x-axis: new scale data for the axis, i.e.
	# a 3-element list containing minimum, maximum and stepsize. Beware: Setting
	# this option will clear all data from the plot.
	$vd::cw::p xconfig -scale [list $vd::cw::x_min $vd::cw::x_max 1]

	# Count the number of the list elements.
	set len [llength $vd::cw::x_l]

	# Test the length of the coordinate list.
	if { $len > 4 } {
	    # Draw again the deleted graph period.
	    $vd::cw::p plotlist charge $vd::cw::x_l $vd::cw::y_l
	
	    # Search for the new list start.
	    set i  0
	    foreach item $vd::cw::x_l {
		# Test the final condition.
		if { $item >= $vd::cw::x_min } {
		    break
		}

		# Count the number of the elements.
		incr i
	    }

	    # Make the coordinates lists smaller.
	    if { $i > 0 } {
		set vd::cw::x_l [lreplace $vd::cw::x_l 0 $i]
		set vd::cw::y_l [lreplace $vd::cw::y_l 0 $i]
	    }
	}
    }

    # Extend lists with the charge graph coordinates.
    lappend vd::cw::x_l  $x
    lappend vd::cw::y_l  $vd::ci::cha

    # Add a data point to the plot.
    # Name of the data series the new point belongs to: string series (in)
    # X-coordinate of the new point: float xcrd (in)
    # Y-coordinate of the new point: float ycrd (in)
    $vd::cw::p plot charge $x $vd::ci::cha
}

# disp_batt_exec() - update the display items and send the calculations results
# to the controller.
#
# @b_state:  battery state.
# @b_color:  color of the battery lamp.
#
# Return:     None.
#
proc disp_batt_exec { b_state b_color } {
    # Find all items with the "bc_light" tag.
    set list [$vd::sw::c find withtag bc_light]

    # Color all items yellow with the "b" tags.
    foreach item $list {
	# This command is similar to the configure widget command except that it
	# modifies item-specific options for the items given by tagOrId.
	$vd::sw::c itemconfigure $item -fill $b_color
    }

    # Test the current battery state.
    if { $vd::co::b_ctrl == "B_ON" && $b_state == "B_OFF" } {
	# The battery state changes from active to inactive.
	
	# Count the number of the list elements.
	set n [llength $vd::cw::x_l]
	incr n -1

	# Test the list size.
	if { $n > 0 } {
	    # Delete the charge list.
	    set vd::cw::x_l [lreplace $vd::cw::x_l 0 $n]
	    set vd::cw::y_l [lreplace $vd::cw::y_l 0 $n]
	}	
    }
    
    # Update the battery state.
    set vd::co::b_ctrl  $b_state

    # Test the battery state.
    if { $vd::co::b_ctrl == "B_EMPTY" } {
	return
    }
    
    # Inform the controller to activate the battery or to initiate the shutdown
    # procedure.
    puts $vd::ep_id "button=$vd::co::b_ctrl:"
    
    # Send the signal without buffering.
    flush $vd::ep_id
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
    
    # Retrive and test the charging element.
    if { [lindex $list 12] != "charging" } {
	error "missing \"charging\" in \"$buf\""
    }
    
    # Save the charging value.
    set vd::ci::cha  [lindex $list 14]
    
    return TCL_OK
}

# disp_input{} - get the input from the controller.
#
# Return:     None.
#
proc disp_input {} {
    # Read all signals from the display input queue of the display-controller cable.
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
	$vd::bw::c itemconfigure $vd::bx::cyc -text $vd::ci::cyc
	
	# Update the voltage value.
	$vd::bw::c itemconfigure $vd::bx::vlt -text $vd::ci::vlt
	
	# Update the current value.
	$vd::bw::c itemconfigure $vd::bx::crt -text $vd::ci::crt

	# Update the charging value.
	$vd::bw::c itemconfigure $vd::bx::cha -text $vd::ci::cha

	# Evaluate the battery state.
	switch $vd::co::b_ctrl {
	    "B_ON" {
		# Update the battery charge graph.
		if { $vd::ci::cha > 0 } {
		    disp_charge_update
		} else {
		    # The battery is empty.
		    disp_batt_exec "B_EMPTY" red
		}
	    }

	    "B_EMPTY" {
		# The battery button has been deactivated.
	    }
	    
	    default {
		# After shutdown there is nothing more to do.
	    }
	}
    }

    # Start the timer for the display input.
    after 100 disp_input
}

# disp_boxes{} - create the display input boxes.
#
# Return:     None.
#
proc disp_boxes {} {
    # Origin of the coordinates system for the boxes
    set ox  10
    set oy  10
    
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
    $vd::bw::c create rectangle $x1 $y1 $x2 $y2 -width 2 -fill snow -outline snow3

    # Print the cycle text.
    set dx  [expr $dx + 125 ]
    set dy  [expr $dy + 12]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    $vd::bw::c create text $x1 $y1 -justify left -text Cycles

    # Print the cycle counter value.
    set dx  [expr $ox + 45]
    set dy  [expr $dy + 0]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    set vd::bx::cyc  [$vd::bw::c create text $x1 $y1 -justify center -text 0]
    
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
    $vd::bw::c create rectangle $x1 $y1 $x2 $y2 -width 2 -fill snow -outline snow3
    
    # Print the voltage text.
    set dx  [expr $dx + 125 ]
    set dy  [expr $dy + 12]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    $vd::bw::c create text $x1 $y1 -justify left -text Voltage
    
    # Print the voltage value.
    set dx  [expr $ox + 45]
    set dy  [expr $dy + 0]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    set vd::bx::vlt  [$vd::bw::c create text $x1 $y1 -justify center -text 0]

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
    $vd::bw::c create rectangle $x1 $y1 $x2 $y2 -width 2 -fill snow -outline snow3
    
    # Print the current text.
    set dx  [expr $dx + 125 ]
    set dy  [expr $dy + 12]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    $vd::bw::c create text $x1 $y1 -justify left -text Current
    
    # Print the current value.
    set dx  [expr $ox + 45]
    set dy  [expr $dy + 0]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    set vd::bx::crt  [$vd::bw::c create text $x1 $y1 -justify center -text 0]

    #===========================================================================
    # Charge box
    # ==========================================================================
    
    # Create the display charge box.
    set dx  $ox
    set dy  [expr $oy + 150]
    set x1  [expr 20  + $dx]
    set y1  [expr 20  + $dy]
    set x2  [expr 110 + $dx]
    set y2  [expr 45  + $dy]
    $vd::bw::c create rectangle $x1 $y1 $x2 $y2 -width 2 -fill snow -outline snow3
    
    # Print the charging text.
    set dx  [expr $dx + 125 ]
    set dy  [expr $dy + 12]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    $vd::bw::c create text $x1 $y1 -justify left -text Charge
    
    # Print the charging value.
    set dx  [expr $ox + 45]
    set dy  [expr $dy + 0]
    set x1  [expr 20 + $dx]
    set y1  [expr 20 + $dy]
    set vd::bx::cha  [$vd::bw::c create text $x1 $y1 -justify center -text 0]
}

# disp_charge_draw{} - create the charge graph.
#
# Return:     None.
#
proc disp_charge_draw {} {
    # ::Plotchart::plotstyle subcmd style args
    #
    # The command plotstyle can be used to set all manner of options.
    #
    # Configure - this subcommand allows you to set the options per chart type.
    # scope is the name of the plot style to manipulate.
    ::Plotchart::plotstyle configure scope xyplot leftaxis   color      green
    ::Plotchart::plotstyle configure scope xyplot leftaxis   textcolor  green
    ::Plotchart::plotstyle configure scope xyplot bottomaxis color      green
    ::Plotchart::plotstyle configure scope xyplot bottomaxis textcolor  green
    ::Plotchart::plotstyle configure scope xyplot background outercolor black
    ::Plotchart::plotstyle configure scope xyplot background innercolor black
    ::Plotchart::plotstyle configure scope xyplot title      background black

    # load - this subcommand makes the given style the active style for
    # subsequent plots and charts.
    ::Plotchart::plotstyle load scope

    # Change the background colour of the charge canvas.
    $vd::cw::c configure -background black
    
    # You create the plot or chart with one single command and then fill the
    # plot with data:
    # ::Plotchart::createXYPlot w xaxis yaxis args
    #
    # widget w (in)   - name of the existing canvas widget to hold the plot.
    # list xaxis (in) - a 3-element list containing minimum, maximum and
    #                   stepsize for the x-axis, in this order.
    # list yaxis (in) - a 3-element list containing minimum, maximum and
    #                   stepsize for the y-axis, in this order.
    set vd::cw::p  [::Plotchart::createXYPlot $vd::cw::c [list $vd::cw::x_min $vd::cw::x_max 1] [list 0 12500 2500]]

    # Set the value for one or more options regarding the drawing of data of a
    # ä specific series with the colour to be used when drawing the data series.
    $vd::cw::p dataconfig charge -color red

    # Draw vertical ticklines at each tick location with the specified colour.
    $vd::cw::p xticklines green
    
    # Draw horizontal ticklines at each tick location with the specified colour.
    $vd::cw::p yticklines green

    # Specify the title of the (horizontal) x-axis.
    $vd::cw::p xtext seconds

    # Specify the title of the (horizontal) y-axis.
    $vd::cw::p ytext charge
}

# disp_brecharge_calc() - calculate the item display state of the battery
# recharge button and the output to the controller.
#
# Return:     None.
#
proc disp_brecharge_calc {} {
    puts "disp_brecharge_calc: ..."
}

# disp_brecharge_create{} - create the battery button for the recharging.
#
# Return:     None.
#
proc disp_brecharge_create {} {
    # Items of type oval appear as circular or oval regions on the display.
    # The arguments x1, y1, x2, and y2 or coordList give the coordinates of two
    # diagonally opposite corners of a rectangular region enclosing the oval.
    set dx   70
    set dy  300
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    set x2 [expr 90 + $dx]
    set y2 [expr 90 + $dy]
    $vd::sw::c create rectangle $x1 $y1 $x2 $y2 -width 0 -fill gray80 -tags [list br_button]
    
    # Create the body of the battery control button.
    set dx   70
    set dy  300
    set x1 [expr 40 + $dx]
    set y1 [expr 40 + $dy]
    set x2 [expr 70 + $dx]
    set y2 [expr 70 + $dy]
    $vd::sw::c create oval $x1 $y1 $x2 $y2 -width 2 -fill gray64 -tags [list br_button br_light]

    # Create the lamp symbol of the recharging button:
    # west-north - south-ost line.
    set x1 [expr 45 + $dx]
    set y1 [expr 45 + $dy]
    set x2 [expr 65 + $dx]
    set y2 [expr 65 + $dy]
    $vd::sw::c create line $x1 $y1 $x2 $y2 -width 2 -tags [list br_button]

    # west-south - ost-north line
    set x1 [expr 45 + $dx]
    set y1 [expr 65 + $dy]
    set x2 [expr 65 + $dx]
    set y2 [expr 45 + $dy]
    $vd::sw::c create line $x1 $y1 $x2 $y2 -width 2 -tags [list br_button]

    # A text item displays a string of characters on the screen in one or more
    # lines. 
    set dx 105
    set dy 310
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    $vd::sw::c create text $x1 $y1 -justify center -text Recharge -tags [list br_button]
    
    # Bind associates command with all the items given by tagOrId such that whenever
    # the event sequence given by sequence occurs for one of the items the command
    # will be invoked.
    # %x, %y
    # The x and y fields from the event. For ButtonPress, ButtonRelease, ...
    # %x and %y indicate the position of the mouse pointer relative to the receiving
    # window.
    $vd::sw::c bind br_button <ButtonPress-1> { disp_brecharge_calc }
}

# disp_bcontrol_calc() - calculate the item display state of the battery control
# button and the output to the controller.
#
# Return:     None.
#
proc disp_bcontrol_calc {} {
    # Evaluate the battery state.
    switch $vd::co::b_ctrl {
	"B_OFF" {
	    # Trigger for the restart actions.
	    set vd::cw::rst  1
	    
	    # Update the battery state.
	    disp_batt_exec "B_ON" yellow 
	}
	
	"B_ON" {
	    # Update the battery state.
	    disp_batt_exec "B_OFF" gray64
	}

	"B_EMPTY" {
	    # The battery button has been deactivated.
	}
	
	default {
	    # After shutdown there is nothing more to do.
	}
    }
}

# disp_bcontrol_create{} - create the battery control button.
#
# Return:     None.
#
proc disp_bcontrol_create {} {
    # Items of type oval appear as circular or oval regions on the display.
    # The arguments x1, y1, x2, and y2 or coordList give the coordinates of two
    # diagonally opposite corners of a rectangular region enclosing the oval.
    set dx   70
    set dy  170
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    set x2 [expr 90 + $dx]
    set y2 [expr 90 + $dy]
    $vd::sw::c create rectangle $x1 $y1 $x2 $y2 -width 0 -fill gray80 -tags [list bc_button]

    # Create the body of the battery control button.
    set dx   70
    set dy  170
    set x1 [expr 40 + $dx]
    set y1 [expr 40 + $dy]
    set x2 [expr 70 + $dx]
    set y2 [expr 70 + $dy]
    $vd::sw::c create oval $x1 $y1 $x2 $y2 -width 2 -fill gray64 -tags [list bc_button bc_light]

    # Create the lamp symbol of the battery control button:
    # west-north - south-ost line.
    set x1 [expr 45 + $dx]
    set y1 [expr 45 + $dy]
    set x2 [expr 65 + $dx]
    set y2 [expr 65 + $dy]
    $vd::sw::c create line $x1 $y1 $x2 $y2 -width 2 -tags [list bc_button]

    # west-south - ost-north line
    set x1 [expr 45 + $dx]
    set y1 [expr 65 + $dy]
    set x2 [expr 65 + $dx]
    set y2 [expr 45 + $dy]
    $vd::sw::c create line $x1 $y1 $x2 $y2 -width 2 -tags [list bc_button]

    # A text item displays a string of characters on the screen in one or more
    # lines. 
    set dx 105
    set dy 180
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    $vd::sw::c create text $x1 $y1 -justify center -text Battery -tags [list bc_button]

    # Bind associates command with all the items given by tagOrId such that whenever
    # the event sequence given by sequence occurs for one of the items the command
    # will be invoked.
    # %x, %y
    # The x and y fields from the event. For ButtonPress, ButtonRelease, ...
    # %x and %y indicate the position of the mouse pointer relative to the receiving
    # window.
    $vd::sw::c bind bc_button <ButtonPress-1> { disp_bcontrol_calc }
}

# disp_bstop_create{} - create the stop button.
#
# Return:     None.
#
proc disp_bstop_create {} {
    # Items of type rectangle appear as rectangular regions on the display. Each
    # rectangle may have an outline, a fill, or both. 
    set dx  70
    set dy  40
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    set x2 [expr 90 + $dx]
    set y2 [expr 90 + $dy]
    $vd::sw::c create rectangle $x1 $y1 $x2 $y2 -width 0 -fill gold -tags b_stop

    # Create the body of the system shutdown button.
    set dx  70
    set dy  40
    set x1 [expr 40 + $dx]
    set y1 [expr 40 + $dy]
    set x2 [expr 70 + $dx]
    set y2 [expr 70 + $dy]
    $vd::sw::c create oval $x1 $y1 $x2 $y2 -width 0 -fill red2 -tags b_stop

    # A text item displays a string of characters on the screen in one or more
    # lines. 
    set dx  105
    set dy   50
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    $vd::sw::c create text $x1 $y1 -justify center -text Stop
    
    # Bind associates command with all the items given by tagOrId such that whenever
    # the event sequence given by sequence occurs for one of the items the command
    # will be invoked.
    # %x, %y
    # The x and y fields from the event. For ButtonPress, ButtonRelease, ...
    # %x and %y indicate the position of the mouse pointer relative to the receiving
    # window.
    $vd::sw::c bind b_stop  <ButtonPress-1> { disp_stop_exec 0 }
}

# disp_frames{} - create and manipulate the container widgets.
#
# Return:     None.
#
proc disp_frames {} {
    #===========================================================================
    # Battery actuators widget
    # ==========================================================================
    # Create and manipulate the container widget for the battery actuators frame.
    set vd::sw::f  [frame .sf -background gray46 -relief ridge -borderwidth 4 -padx 2 -pady 2 -width $vd::sw::w -height $vd::sw::h]

    # Create and manipulate the display canvas widget.
    # All coordinates related to canvases are stored as floating-point numbers.
    set vd::sw::c  [canvas .sf.c -width $vd::sw::w -height $vd::sw::h -bg gray92]

    # Geometry manager that packs around edges of cavity battery actuators canvas.
    pack $vd::sw::c
    
    # Geometry manager that packs around edges of cavity for the battery actuators frame.
    pack $vd::sw::f -side left
    # pack configure $vd::sw::f -anchor w
    
    #===========================================================================
    # Capacity widget
    # ==========================================================================
    # Create and manipulate the container widget for the capacity frame.
    set vd::cw::f  [frame .cf -background gray46 -relief ridge -borderwidth 4 -padx 2 -pady 2 -width $vd::cw::w -height $vd::cw::h]

    # Create and manipulate the display canvas widget.
    # All coordinates related to canvases are stored as floating-point numbers.
    set vd::cw::c  [canvas .cf.c -width $vd::cw::w -height $vd::cw::h -xscrollincrement 1 -bg black]

    # Geometry manager that packs around edges of cavity for the capacity canvas.
    pack $vd::cw::c

    # Geometry manager that packs around edges of cavity for the capacity frame.
    # pack $vd::cw::f -side left
    pack configure $vd::cw::f -anchor nw
    
    #===========================================================================
    # x graph widget
    # ==========================================================================
    # Create and manipulate the container widget for the x graph frame.
    set vd::gw::f  [frame .gf -background gray46 -relief ridge -borderwidth 4 -padx 2 -pady 2 -width $vd::gw::w -height $vd::gw::h]

    # Create and manipulate the display canvas widget.
    # All coordinates related to canvases are stored as floating-point numbers.
    set vd::gw::c  [canvas .gf.c -width $vd::gw::w -height $vd::gw::h -xscrollincrement 1 -bg black]

    # Geometry manager that packs around edges of cavity for the graph canvas.
    pack $vd::gw::c

    # Geometry manager that packs around edges of cavity for the graph frame.
    # pack $vd::gw::f -side left
    pack configure $vd::gw::f -anchor sw
    
    #===========================================================================
    # Sensor widget
    # ==========================================================================
    # Create and manipulate the container widget for the box frame.
    set vd::bw::f  [frame .bf -background gray46 -relief ridge -borderwidth 4 -padx 2 -pady 2 -width $vd::bw::w -height $vd::bw::h]

    # Create and manipulate the display canvas widget.
    # All coordinates related to canvases are stored as floating-point numbers.
    set vd::bw::c  [canvas .bf.c -width $vd::bw::w -height $vd::bw::h -bg gray92]

    # Geometry manager that packs around edges of cavity for the box canvas.
    pack $vd::bw::c

    # The placer is a geometry manager for Tk. It provides simple fixed
    # placement of windows.
    set x  [expr $vd::wm_w - $vd::bw::w]
    place configure $vd::bw::f -x $x -y 0
}

# disp_wm{} - configure the root widget
#
# Return:     None.
#
proc disp_wm {} {
    # Communicate with the window manager:

    # Pass the string to the window manager for use as the title for the display
    # window.
    wm title . "vDisplay"

    # If width and height are specified, they give the minimum permissible
    # dimensions for window.
    wm minsize . $vd::wm_w $vd::wm_h

    # If width and height are specified, they give the maximum permissible
    # dimensions for window.
    wm maxsize . $vd::wm_w $vd::wm_h
    
    # Destroy the root window.
    bind . <Destroy> { disp_stop_exec 0 }
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

#===============================================================================
# EXPORTED FUNCTIONS
# ==============================================================================
# main{} - main process of the van display.
#
# Return:     None.
#
proc main {} {
    # Complete the display-controller cable.
    disp_connect

    # Configure the main widget.
    disp_wm

    # Create and manipulate the container widgets.
    disp_frames

    # Create the stop button.
    disp_bstop_create

    # Create the battery control button.
    disp_bcontrol_create

    # Create the battery button for the recharging.
    disp_brecharge_create
    
    # Create the battery charge graph.
    disp_charge_draw

    # Create the display input boxes.
    disp_boxes

    # Start the timer for the display input from the controller.
    disp_input
}

# Main process of the van display.
main
