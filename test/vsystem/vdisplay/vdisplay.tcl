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
# load ../../../lib/libvan[info sharedlibextension]
load ../../lib/libvan[info sharedlibextension]

# The namespace command lets you create, access, and destroy separate contexts
# for commands and variables.
#
# vd - describes the display state in the van system.
#
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
    variable  wm_w   1400
    variable  wm_h   800

    # sw - switch or button widget for the actuators.
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
    # @rst:    1: restart the battery activation.
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

    # bx - box ids of the canvas sensor items.
    #
    # @cyc:  id of the cycle box.
    # @vlt:  id of the voltage box.
    # @crt:  id of the current box.
    # @cha:  id of the charge quantity box.
    # @lev:  id of the fill level box.
    # @dtc:  id of the disply box for the transfer counters.
    # @ctc:  id of the controll box for the transfer counters.
    namespace eval bx {
	variable  cyc
	variable  vlt
	variable  crt
	variable  cha
	variable  lev
	variable  dtc
	variable  ctc
    }
    
    # ci - input from the controller.
    #
    # @rx_n:    number of the received display messages from the controller
    # @tx_n:    number of the sent display messages from the controller
    # @cyc:     cycle value or time stamp.
    # @vlt:     voltatage value
    # @crt:     current value.
    # @cha:     charge quantity.
    # @lev:     fill level of the battery in percent.
    namespace eval ci {
	variable  rx_n
	variable  tx_n
	variable  cyc
	variable  vlt
	variable  crt
	variable  cha
	variable  lev
    }

    # co - output to the controller.
    #
    # @rx_n:    number of the received controller messages
    # @tx_n:    number of the sent controller messages
    # @ctrl_b:  current state of the battery control button: B_STOP:
    #           shutdown, B_ON: active battery, B_OFF: inactive battery,
    #           B_EMPTY: discharged battery, nevertheless the battery is active
    #           and can be charged at any time.
    # @rech_b:  recharging state of the battery: 0: off, 1: on
    namespace eval co {
	variable rx_n
	variable tx_n
	variable ctrl_b
	variable rech_b
    }

    # ds - battery display state
    #
    # @p:       display prompt.
    # @ep_id:   display end point of the controller display cable.
    # @ctrl_b:  current state of the battery control button: B_STOP:
    #           shutdown, B_ON: active battery, B_OFF: inactive battery,
    #           B_EMPTY: discharged battery, therefor the control button has
    #           been blocked. Nevertheless the battery is active
    # @rech_b:  recharging state of the battery: 0: off, 1: on
    #           and can be charged at any time.
    # @cyc:     cycle value or time stamp.
    # @vlt:     voltatage value
    # @crt:     current value.
    # @cha:     charge quantity.
    # @lev:     fill level of the battery in percent.
    # @d_rx_n:  number of the received messages by the display
    # @d_tx_n:  number of the sent messages by the display
    # @c_rx_n:  number of the received display messages by the controller
    # @c_tx_n:  number of the sent display messages by the controller
    namespace eval ds {
        variable  p       "D>"
        variable  ep_id   -1
	variable  ctrl_b  "B_OFF"
	variable  rech_b  0  
	variable  cyc     0
	variable  vlt     0
	variable  crt     0
	variable  cha     0
	variable  lev     0
	variable  d_rx_n    0
	variable  d_tx_n    0
	variable  c_rx_n    0
	variable  c_tx_n    0
    }
}

#===============================================================================
# LOCAL FUNCTIONS
# ==============================================================================
# disp_ctrl_write{} - send the display signal to the controller.
#
# Return:     None.
#
proc disp_ctrl_write {} {
    # Update the send counter.
    incr vd::ds::d_tx_n
    
    # Make a copy of the display actuators: controll and recharge button.
    set vd::co::rx_n    $vd::ds::d_rx_n
    set vd::co::tx_n    $vd::ds::d_tx_n
    set vd::co::ctrl_b  $vd::ds::ctrl_b
    set vd::co::rech_b  $vd::ds::rech_b

    # Concatenate the signal elements to the controller.
    set buf  "rxno=$vd::co::rx_n\:\:txno=$vd::co::tx_n\:\:control_b=$vd::co::ctrl_b\:\:recharge_b=$vd::co::rech_b\:\:"
    
    # Send the signal to the controller.
    puts $vd::ds::ep_id  $buf

    # Prevent the buffering of the output signal to the controller.
    flush $vd::ds::ep_id
}

# disp_bstop_cb{} - propagate the termination request to the controller and
# battery.
#
# Return:     None.
#
proc disp_bstop_cb {} {
    # Send the stop signal to the controller.
    set vd::ds::ctrl_b  "B_STOP"
    set vd::ds::rech_b  0
    disp_ctrl_write
    
    # Wait until the output queue is empty
    set n 1
    while { $n > 0 } {
	# Get the number of the pending output bytes.
	set n [fconfigure $vd::ds::ep_id -sync]

 	# Wait a few milliseconds
	after 1
    }

    # Print the goodby notificaton.
    puts "vdisplay done"
    
    # Pull out the display plug.
    close $vd::ds::ep_id

    # This command deletes the windows given by the window arguments, plus all
    # of their descendants. If a window “.” is deleted then all windows will be
    # destroyed and the application will (normally) exit. 
    destroy .
    
    # End the van system display application.
    exit 0
}

# disp_charge_xylist{} - test and delete coordinates, which are outside range.
#
# Return:     None.
#
proc disp_charge_xylist {} {
    # Count the number of the list elements.
    set len [llength $vd::cw::x_l]

    # Test the number of the list elements
    if { $len < 1 } {
	return
    }
    
    # Search for the new list start.
    set i  0
    foreach item $vd::cw::x_l {
	# Test the final condition.
	if { $item >= $vd::cw::x_min } {
	    break
	}

	# Count the number of the elements.
	incr i -1
    }

    # Test the number of coordinates, which are outside range.
    if { $i < 1 } {
	return
    }

    # Make the coordinates lists smaller.
    set vd::cw::x_l [lreplace $vd::cw::x_l 0 $i]
    set vd::cw::y_l [lreplace $vd::cw::y_l 0 $i]
}

# disp_charge_update{} - update the battery charge graph.
#
# Return:     None.
#
proc disp_charge_update {} {
    # Initialize the redraw state.
    set redraw  0
    
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
	    # Remove the lines, symbols and other graphical object associated
	    # with the actual data from the plot.
	    $vd::cw::p deletedata
	
	    # Calculate the ininitial and final value of the x-axis.
	    set vd::cw::x_min  $x_i
	    set vd::cw::x_max  [expr { $vd::cw::x_min + $vd::cw::x_len }]
	    
	    # Shift the x-axis.
	    $vd::cw::p xconfig -scale [list $vd::cw::x_min $vd::cw::x_max 1]
	    
	    # Test and delete coordinates, which are outside range.
	    disp_charge_xylist
	
	    # Redraw the charge graph.
	    set redraw  1
	}
    }
    
    # Test the current range of the x-time axis.
    if { $x >= $vd::cw::x_max } {
	# Remove the lines, symbols and other graphical object associated
	# with the actual data from the plot.
	$vd::cw::p deletedata
	
	# Calculate the ininitial and final value of the x-axis.
	incr vd::cw::x_min
	incr vd::cw::x_max

	# Set the scale parameter for the x-axis: new scale data for the axis, i.e.
	# a 3-element list containing minimum, maximum and stepsize. Beware: Setting
	# this option will clear all data from the plot.
	$vd::cw::p xconfig -scale [list $vd::cw::x_min $vd::cw::x_max 1]

	# Test and delete coordinates, which are outside range.
	disp_charge_xylist

	# Redraw the charge graph.
	set redraw  1
    }

    # Extend the lists with the charge graph coordinates.
    lappend vd::cw::x_l  $x
    lappend vd::cw::y_l  $vd::ci::cha

    # Test the redraw state.
    if { $redraw } {
	# Count the number of the list elements.
	set len [llength $vd::cw::x_l]

	# Test the list size.
	if { $len > 2 } {
	    # Draw again the deleted graph period.
	    $vd::cw::p plotlist charge $vd::cw::x_l $vd::cw::y_l
	}
    } else {
	# Add a data point to the plot.
	# Name of the data series the new point belongs to: string series (in)
	# X-coordinate of the new point: float xcrd (in)
	# Y-coordinate of the new point: float ycrd (in)	
	$vd::cw::p plot charge $x $vd::ci::cha
    }
}

# disp_bcontrol_exec() - update the display items of the battery control button
# and send the calculations results to the controller.
#
# @b_state:  battery control state.
# @b_color:  color of the battery control lamp.
#
# Return:     None.
#
proc disp_bcontrol_exec { b_state b_color } {
    # Find all items with the "bctrl_light" tag.
    set list [$vd::sw::c find withtag bctrl_light]

    # Color all items with the "bc_ligh" tags.
    foreach item $list {
	# This command is similar to the configure widget command except that it
	# modifies item-specific options for the items given by tagOrId.
	$vd::sw::c itemconfigure $item -fill $b_color
    }

    # Test the current battery control state.
    if { $vd::ds::ctrl_b == "B_ON" && $b_state == "B_OFF" } {
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
    
    # Update the battery control state.
    set vd::ds::ctrl_b  $b_state

    # Test the battery state.
    if { $vd::ds::ctrl_b == "B_EMPTY" } {
	return
    }
    
    # Instruct the controller to activate or to deactivate the battery.
    disp_ctrl_write
}

# disp_type_value{} - parse the type and value of a message parameter from the
# controller.
#
# @buf:   unstructed input from the controller.
# @msg:   structed input from the controller.
# @i:     type index and value index.
# @type:  type of the message parameter.
#
# Return:     the value of the message parameter type.
#
proc disp_type_value { buf msg &i type } {
    # upvar arranges for one or more local variables in the current procedure to
    # refer to variables in an enclosing procedure call or to global variables.
    # Level may have any of the forms permitted for the uplevel command, and may
    # be omitted (it defaults to 1).    
    upvar 1 ${&i} i
    
    # Test the type of the message parameter.
    if { [lindex $msg $i] != $type } {
	error "missing \"$type\" in \"$buf\""
    }
    
    # Update the message index.
    incr i +2

    # Return the value of the message parameter type.
    set n  [lindex $msg $i]

    # Update the message index.
    incr i +2
    
    return $n
}

# disp_input_parse{} - parse the input from the controller.
#
# @buf:  input signal from the controller.
#
# Return:     the trap information or TCL_OK.
#
proc disp_input_parse { buf } {
    # Intialize the index counter.
    set i  0

    # Update the receive counter.
    incr vd::ds::d_rx_n
    
    # Match the regular expression against the controller signal.
    set msg [regexp -inline -all -- {[a-z]+|=|[0-9]+|::}  $buf]

    # Count the elements of the controller signal.
    set ll [llength $msg]
    if { $ll != 27 } {
	# Generate an error.
	error "wrong format \"$buf\""
    }

    # Get the rx counter value.
    set vd::ci::rx_n [disp_type_value $buf $msg i "rxno"]
    
    # Get the tx counter value.
    set vd::ci::tx_n [disp_type_value $buf $msg i "txno"]
    
    # Get the cycle value.
    set vd::ci::cyc  [disp_type_value $buf $msg i "cycle"]
    
    # Get the voltage value.
    set vd::ci::vlt  [disp_type_value $buf $msg i "voltage"]

    # Get the current value.
    set vd::ci::crt  [disp_type_value $buf $msg i "current"]

    # Get the charge value.
    set vd::ci::cha  [disp_type_value $buf $msg i "charge"]

    # Get the fill level value.
    set vd::ci::lev  [disp_type_value $buf $msg i "level"]

    # Refresh the display state.
    set vd::ds::cyc     $vd::ci::cyc
    set vd::ds::vlt     $vd::ci::vlt
    set vd::ds::crt     $vd::ci::crt
    set vd::ds::cha     $vd::ci::cha
    set vd::ds::lev     $vd::ci::lev
    set vd::ds::c_rx_n  $vd::ci::rx_n
    set vd::ds::c_tx_n  $vd::ci::tx_n
    
    return TCL_OK
}

# disp_input_wait{} - get the input from the controller.
#
# Return:     None.
#
proc disp_input_wait {} {
    # Read all signals from the display input queue of the display-controller cable.
    while { 1 } {
	set n  [gets $vd::ds::ep_id buf]

	# Test the length of the input signal.
	if { $n < 1 } {
	    break
	}
	
	# Trace the input from the controller.
	puts "$vd::ds::p INPUT $buf"

	# Evaluate the input from the controller and trap exceptional returns
	if { [ catch { disp_input_parse $buf } ] } {
	    set info $::errorInfo
	    puts "Invalid input from the controller: $info"

	    # Propagate the termination request to the controller and battery.
	    disp_bstop_cb 1
	}

	# Update the cycle counter.
	$vd::bw::c itemconfigure $vd::bx::cyc -text $vd::ds::cyc
	
	# Update the voltage value.
	$vd::bw::c itemconfigure $vd::bx::vlt -text $vd::ds::vlt
	
	# Update the current value.
	$vd::bw::c itemconfigure $vd::bx::crt -text $vd::ds::crt

	# Update the charging value.
	$vd::bw::c itemconfigure $vd::bx::cha -text $vd::ds::cha

	# Update the fill evel value.
	$vd::bw::c itemconfigure $vd::bx::lev -text $vd::ds::lev

	# Update the display transfer counters..
	set rx_n  $vd::ds::d_rx_n
	set tx_n  $vd::ds::d_tx_n
	$vd::bw::c itemconfigure $vd::bx::dtc -text "$rx_n / $tx_n"
	
	# Update the controller transfer counters..
	set rx_n  $vd::ds::c_rx_n
	set tx_n  $vd::ds::c_tx_n
	$vd::bw::c itemconfigure $vd::bx::ctc -text "$tx_n / $rx_n"

	# Evaluate the battery state.
	switch $vd::ds::ctrl_b {
	    "B_ON" {
		# Update the battery charge graph.
		disp_charge_update
		
		# Test the charge quantity of the battery.
		if { $vd::ds::cha > 0 } {
		    break
		}
		
		# The battery is empty.
		disp_bcontrol_exec "B_EMPTY" red
	    }

	    "B_EMPTY" {
		# Update the battery charge graph.
		disp_charge_update
		
		# The battery is active, but it is flat.
		# Test the charge quantity of the battery.
		if { $vd::ds::cha < 1 } {
		    break
		}
		
		# Change the state of the battery control button.
		disp_bcontrol_exec "B_ON" yellow 	    
	    }
	    
	    default {
		# After shutdown there is nothing more to do.
	    }
	}
    }

    # Start the timer for the display input with 100 milliseconds.
    # The keyword after ms schedules that script to be evaluated at least 'ms'
    # millisecondss in the future.
    after 100 disp_input_wait
}

# disp_sensor_create{} - create a sensor display box.
#
# @y1_r:  
#
# Return:     the canvas item id.
#
proc disp_sensor_create { y1_r y2_r label } {
    # All sensor boxes are located on the same hoizontal line or x-axis.
    # Here is the list of the reference points on the x-axis.
    set x1_r   30
    set x2_r  120

    # Define the start of any box text  on the vertical or y-axis.
    set y1  [expr $y1_r +  12]
    
    # Create the sensor box.
    $vd::bw::c create rectangle $x1_r $y1_r $x2_r $y2_r -width 2 -fill snow -outline snow3

    # Print the sensor text.
    set x1  [expr $x1_r + 125]
    $vd::bw::c create text $x1 $y1 -justify left -text $label

    # Print the initial value of the sensor.
    set x1  [expr $x1_r + 45]
    
    # Return the box item id.
    return [$vd::bw::c create text $x1 $y1 -justify center -text "0 / 0"]
}

# disp_sensors{} - create the display sensors or input boxes.
#
# Return:     None.
#
proc disp_sensors {} {
    # Define the origion points of the sensor boxes on the vertical or y-axis.

    # Top left y-coordinate.
    set y1_o    0

    # Bottom right y-coordinate.
    set y2_o   25

    # Distance between sensor boxes.
    set dy     50

    # Initialize the multiplier for shift on the y-axis.
    set m  0
    
    # Create the cycle box.
    incr  m
    set y1  [expr $y1_o + $dy * $m]
    set y2  [expr $y2_o + $dy * $m]
    set vd::bx::cyc  [disp_sensor_create $y1 $y2 "Cycles"]

    # Create the voltage box.
    incr  m
    set y1  [expr $y1_o + $dy * $m]
    set y2  [expr $y2_o + $dy * $m]
    set vd::bx::vlt  [disp_sensor_create $y1 $y2 "Voltage"]

    # Create the current box.
    incr  m
    set y1  [expr $y1_o + $dy * $m]
    set y2  [expr $y2_o + $dy * $m]
    set vd::bx::crt  [disp_sensor_create $y1 $y2 "Current"]

    # Create the charge box.
    incr  m
    set y1  [expr $y1_o + $dy * $m]
    set y2  [expr $y2_o + $dy * $m]
    set vd::bx::cha  [disp_sensor_create $y1 $y2 "Charge"]

    # Create the fill level box.
    incr  m
    set y1  [expr $y1_o + $dy * $m]
    set y2  [expr $y2_o + $dy * $m]
    set vd::bx::lev  [disp_sensor_create $y1 $y2 "Level"]
    
    # Create the display box for the transfer counters.
    incr  m
    set y1  [expr $y1_o + $dy * $m]
    set y2  [expr $y2_o + $dy * $m]
    set vd::bx::dtc  [disp_sensor_create $y1 $y2 "D Rx/Tx"]
    
    # Create the controller box for the transfer counters.
    incr  m
    set y1  [expr $y1_o + $dy * $m]
    set y2  [expr $y2_o + $dy * $m]
    set vd::bx::ctc  [disp_sensor_create $y1 $y2 "C Tx/Rx"]
}

# disp_charge_graph{} - create the charge graph.
#
# Return:     None.
#
proc disp_charge_graph {} {
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

    # Draw vertical ticklines at each tick location with the specified colour.
    # $vd::cw::p xticklines green
    $vd::cw::p xticklines green
    
    # Draw horizontal ticklines at each tick location with the specified colour.
    # $vd::cw::p yticklines green
    $vd::cw::p yticklines green

    # Set the value for one or more options regarding the drawing of data of a
    # ä specific series with the colour to be used when drawing the data series.
    $vd::cw::p dataconfig charge -color red -width 2

    # Specify the title of the (horizontal) x-axis.
    $vd::cw::p xtext seconds

    # Specify the title of the (vertical) y-axis.
    $vd::cw::p ytext charge
}

# disp_brecharge_exec() - update the display items of the battery recharge
# button and send the calculations results to the controller.
#
# @b_state:  battery recharge state.
# @b_color:  color of the battery control lamp.
#
# Return:     None.
#
proc disp_brecharge_exec { b_state b_color } {
    # Find all items with the "br_light" tag.
    set list [$vd::sw::c find withtag bcha_light]

    # Color all items with the "br_light" tags.
    foreach item $list {
	# This command is similar to the configure widget command except that it
	# modifies item-specific options for the items given by tagOrId.
	$vd::sw::c itemconfigure $item -fill $b_color
    }

    # Update the battery recharge state.
    set vd::ds::rech_b  $b_state
    
    # Inform the controller to activate or to deactivate the battery recharging.
    disp_ctrl_write
}

# disp_brecharge_cb() - calculate the item display state of the battery
# recharge button and the output to the controller.
#
# Return:     None.
#
proc disp_brecharge_cb {} {
    # Test the battery control button.
    if { $vd::ds::ctrl_b == "B_OFF" } {
	return;
    }
    
    # Evaluate the battery control state.
    switch $vd::ds::rech_b {
	0 {
	    # Update the battery recharge state.
	    disp_brecharge_exec 1 green2 
	}
	
	1 {
	    # Update the battery recharge state.
	    disp_brecharge_exec 0 gray64
	}

	default {
	    # After shutdown there is nothing more to do.
	}
    }
}

# disp_bcontrol_cb() - calculate the item display state of the battery control
# button and the output to the controller.
#
# Return:     None.
#
proc disp_bcontrol_cb {} {
    # Evaluate the battery control state.
    switch -regexp $vd::ds::ctrl_b {
	B_OFF {
	    # Trigger for the restart actions.
	    set vd::cw::rst  1
	    
	    # Update the battery control state.
	    disp_bcontrol_exec "B_ON" yellow 
	}
	
	(B_ON)|(B_EMPTY) {
	    # Update the battery recharge state.
	    disp_brecharge_exec 0 gray64
	    
	    # Update the battery control state.
	    disp_bcontrol_exec "B_OFF" gray64
	}

	default {
	    # After shutdown there is nothing more to do.
	}
    }
}

# disp_actuator_create{} - create an  actuator or control button.
#
# @dx:        top left x coordinate.
# @dy:        top left y coordinate.
# @l_symbol:  if 1, create the lamp symbol.
# @b_tag:     button tag.
# @b_colour:  colour of the button.
# @l_tag:     lamp tag.
# @l_colour:  colour of the lamp.
# @l_width:   outline width of the lamp circle.
# @b_text:    title of the button.
# @cb:        button call back.
#
# Return:     None.
#
proc disp_actuator_create { dx dy l_symbol b_tag b_colour l_tag l_colour l_width b_text cb } {
    # Items of type oval appear as circular or oval regions on the display.
    # The arguments x1, y1, x2, and y2 or coordList give the coordinates of two
    # diagonally opposite corners of a rectangular region enclosing the oval.
    set x1 [expr $dx +  0]
    set y1 [expr $dy +  0]
    set x2 [expr $dx + 70]
    set y2 [expr $dy + 70]
    $vd::sw::c create rectangle $x1 $y1 $x2 $y2 -width 0 -fill $b_colour -tags [list $b_tag]

    # Create the body of the battery control button.
    set x1 [expr $dx + 20]
    set y1 [expr $dy + 20]
    set x2 [expr $dx + 50]
    set y2 [expr $dy + 50]
    $vd::sw::c create oval $x1 $y1 $x2 $y2 -width $l_width -fill $l_colour -tags [list $b_tag $l_tag]

    # Test the state of the lamp sysmbol.
    if { $l_symbol } {
	# Create the lamp symbol of the battery control button:
	# west-north - south-ost line.
	set x1 [expr $dx + 25]
	set y1 [expr $dy + 25]
	set x2 [expr $dx + 45]
	set y2 [expr $dy + 45]
	$vd::sw::c create line $x1 $y1 $x2 $y2 -width 2 -tags [list $b_tag]

	# west-south - ost-north line
	set x1 [expr $dx + 25]
	set y1 [expr $dy + 45]
	set x2 [expr $dx + 45]
	set y2 [expr $dy + 25]
	$vd::sw::c create line $x1 $y1 $x2 $y2 -width 2 -tags [list $b_tag]
    }
    
    # A text item displays a string of characters on the screen in one or more
    # lines. 
    set x1 [expr $dx + 35]
    set y1 [expr $dy + 10]
    $vd::sw::c create text $x1 $y1 -justify center -text $b_text -tags [list $b_tag]

    # Bind associates command with all the items given by tagOrId such that whenever
    # the event sequence given by sequence occurs for one of the items the command
    # will be invoked.
    # %x, %y
    # The x and y fields from the event. For ButtonPress, ButtonRelease, ...
    # %x and %y indicate the position of the mouse pointer relative to the receiving
    # window.
    $vd::sw::c bind $b_tag <ButtonPress-1> $cb
}

# disp_actuators{} - create the display actuators or control buttions.
#
# Return:     None.
#
proc disp_actuators {} {
    # Define the horizontal inital coordinate of the actuators buttons.
    set dx  90
    
    # Define the vertical initial coordinate of the actuators buttons.
    set dy  60

    # Create the button to shut down the battery system.
    set func [list disp_bstop_cb]
    disp_actuator_create $dx $dy 0 bstop_press gold bstop_light red2 0 Stop $func
    
    # Define the vertical orientation of the battery on and off switch.
    set dy  [expr $dy + 130]
    
    # Create the button to activate or deactivate the battery.
    set func [list disp_bcontrol_cb]
    disp_actuator_create $dx $dy 1 bctr_press gray80 bctrl_light gray64 2 Battery $func

    # Define the vertical orientation of the recharging button.
    set dy  [expr $dy + 130]
    
    # Create the button to recharge the battery.
    set func [list disp_brecharge_cb]
    disp_actuator_create $dx $dy 1 bcha_press gray80 bcha_light gray64 2 Charge $func
}

# disp_container_frames{} - create and manipulate the container widgets.
#
# Return:     None.
#
proc disp_container_frames {} {
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

# disp_main_widget{} - configure the root widget
#
# Return:     None.
#
proc disp_main_widget {} {
    # Communicate with the window manager:
    
    # wm geometry window ?newGeometry?
    # NewGeometry has the form =widthxheight rootflagx x rootflagy y, where any
    # of =, widthxheight, or +x+y'' may be omitted.
    wm geometry . $vd::wm_w\x$vd::wm_h+1350+0
    
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
    bind . <Destroy> { disp_bstop_cb 0 }
}

# disp_cable{} - insert the display-controller cable.
#
# Return:     None.
#
proc disp_cable_insert {} {
    # The controller has to be started before, otherwise the display plug cannot
    # be inserted, to provide the controller display cable for the bidirectional
    # message or signal tranfer between controller and display.
    set vd::ds::ep_id [vcable /van/display]
}

# disp_specify{} - specify the looks of the GUI.
#
# Return:     None.
#
proc disp_looks_specify {} {
    # Configure the main widget.
    disp_main_widget

    # Create and manipulate the container widgets.
    disp_container_frames

    # Create the display actuators or control buttions.
    disp_actuators
    
    # Create the battery charge graph.
    disp_charge_graph

    # Create the display sensors or notification boxes.
    disp_sensors
}

# disp_argv{} - analyze the vdisplay arguments.
#
# Return:     None.
#
proc disp_argv {} {
    # $argc  - number items of arguments passed to a script.
    # $argv  - list of the arguments.
    # $argv0 - name of the script.
    if { $::argc > 0 } {
	set i 1
	foreach arg $::argv {
	    puts "argument $i is $arg"
	    incr i
	}
    } else {
	puts "no command line argument passed"
    }    
}

#===============================================================================
# EXPORTED FUNCTIONS
# ==============================================================================
# main{} - main process of the van display.
#
# Return:     None.
#
proc main {} {
    # Analyze the vdisplay arguments.
    # disp_argv

    # Insert the display-controller cable.
    disp_cable_insert

    # Specify the looks of the graphical user interface.
    disp_looks_specify
    
    # Start the timer for the evalution of the display input from the
    # controller.
    disp_input_wait
}

# Main process of the van display.
main
