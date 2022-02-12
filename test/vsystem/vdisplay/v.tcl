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
# @wm_w:     width of the main widget.
# @wm_h:     height of the main widget..
#
# @cv_id     canvas id.
# @cv_w:     width of the canvas.
# @cv_w:     height of the canvas.
# @b_state:  if 1, the battery is active.
namespace eval vd {
    variable  p        "D>"
    variable  ep_id
    variable  wm_w    1400
    variable  wm_h    800

    namespace eval sw {
	variable  f
	variable  c
	variable  w   250
	variable  h   800
    }

    namespace eval gw {
	variable  f
	variable  c
	variable  w   900
	variable  h   800
	
	variable  delay      50
	variable  x          { $t / 50. }
	variable  plot       { sin($x) }
	variable  t0         0
	variable  t1        -1
	variable  accuracy   1.e-2
    }

    namespace eval bw {
	variable  f
	variable  c
	variable  w   250
	variable  h   800
    }

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

#===============================================================================
# LOCAL FUNCTIONS
# ==============================================================================
# disp_charging{} - create the charging test graph.
#
# @t:  x or time axis.
#
# Return:     None.
#
proc disp_charging { t } {
    set h   $vd::gw::h
    set h1  [expr { int($h * 0.5) }]  ;# canvas mid-height
    set h2  [expr { $h1 + 1}]
    set h3  [expr { int($h * 0.4) }]  ;# graph mid-height

    if { $t != $vd::gw::t1 } {
	set x  [expr $vd::gw::x]
	set vv [expr $vd::gw::plot]
	set v  [expr { int($vv * $h3) + $h1 }]
	    
	if { abs($vv) < $vd::gw::accuracy } { 
	    #puts "$vv [expr {$params(accuracy)}]" ;#because I wanted to understand the points
	    $vd::gw::c create text $t 0 -anchor n -text [expr { $t / 50. }] -fill green
	    $vd::gw::c create line $t 0 $t $h -fill green
	}
	    
	$vd::gw::c create line $t $h1 $t $h2 -fill green
	$vd::gw::c create rectangle $t $v $t $v -outline green
	incr t
	    
	if { $t >  $vd::gw::w } {
	    $vd::gw::c  xview scroll 1 unit
	}
    }
	
    after $vd::gw::delay disp_charging $t
}

# disp_boxes{} - create the display input boxes.
#
# Return:     None.
#
proc disp_boxes {} {
    # Origin of the coordinates system for the boxes
    # set ox  0
    # set oy  100
    
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
}

# disp_batt{} - create the battery control button.
#
# Return:     None.
#
proc disp_batt {} {
    # Items of type oval appear as circular or oval regions on the display.
    # The arguments x1, y1, x2, and y2 or coordList give the coordinates of two
    # diagonally opposite corners of a rectangular region enclosing the oval.
    set dx   70
    set dy  170
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    set x2 [expr 90 + $dx]
    set y2 [expr 90 + $dy]
    $vd::sw::c create rectangle $x1 $y1 $x2 $y2 -width 0 -fill gray80 -tags [list b_button b_move]

    set dx   70
    set dy  170
    set x1 [expr 40 + $dx]
    set y1 [expr 40 + $dy]
    set x2 [expr 70 + $dx]
    set y2 [expr 70 + $dy]
    $vd::sw::c create oval $x1 $y1 $x2 $y2 -width 2 -fill gray64 -tags [list b_button b_light b_move]
    
    # west-north - south-ost line
    set x1 [expr 45 + $dx]
    set y1 [expr 45 + $dy]
    set x2 [expr 65 + $dx]
    set y2 [expr 65 + $dy]
    $vd::sw::c create line $x1 $y1 $x2 $y2 -width 2 -tags [list b_lamp b_move]

    # west-south - ost-north line
    set x1 [expr 45 + $dx]
    set y1 [expr 65 + $dy]
    set x2 [expr 65 + $dx]
    set y2 [expr 45 + $dy]
    $vd::sw::c create line $x1 $y1 $x2 $y2 -width 2 -tags [list b_lamp b_move]

    # A text item displays a string of characters on the screen in one or more
    # lines. 
    set dx 105
    set dy 180
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    $vd::sw::c create text $x1 $y1 -justify center -text Battery -tags [list b_move]

    # Bind associates command with all the items given by tagOrId such that whenever
    # the event sequence given by sequence occurs for one of the items the command
    # will be invoked.
    # %x, %y
    # The x and y fields from the event. For ButtonPress, ButtonRelease, ...
    # %x and %y indicate the position of the mouse pointer relative to the receiving
    # window.
    # $vd::sw::c bind b_button <ButtonPress-1> { disp_batt_calc }
    # $vd::sw::c bind b_lamp   <ButtonPress-1> { disp_batt_calc }
}

# disp_stop{} - create the stop button.
#
# Return:     None.
#
proc disp_stop {} {
    # Items of type rectangle appear as rectangular regions on the display. Each
    # rectangle may have an outline, a fill, or both. 
    set dx  70
    set dy  40
    set x1 [expr 20 + $dx]
    set y1 [expr 20 + $dy]
    set x2 [expr 90 + $dx]
    set y2 [expr 90 + $dy]
    $vd::sw::c create rectangle $x1 $y1 $x2 $y2 -width 0 -fill gold -tags b_stop

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
    # $vd::sw::c bind b_stop  <ButtonPress-1> { disp_stop_exec 0 }
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
    
    #===========================================================================
    # Graph widget
    # ==========================================================================
    # Create and manipulate the container widget for the graph frame.
    set vd::gw::f  [frame .gf -background gray46 -relief ridge -borderwidth 4 -padx 2 -pady 2 -width $vd::gw::w -height $vd::gw::h]

    # Create and manipulate the display canvas widget.
    # All coordinates related to canvases are stored as floating-point numbers.
    set vd::gw::c  [canvas .gf.c -width $vd::gw::w -height $vd::gw::h -xscrollincrement 1 -bg black]

    # xxx
    bind . <Destroy> { exit }

    # Geometry manager that packs around edges of cavity for the graph canvas.
    pack $vd::gw::c

    # Geometry manager that packs around edges of cavity for the battery graph frame.
    pack $vd::gw::f -side left
    
    # Shift the view in the window left or right according to number and what.
    # Number must be an integer. If what is units, the view adjusts left or
    # right in units of the xScrollIncrement option, if it is greater than zero,
    # or in units of one-tenth the window's width otherwise. 
    $vd::gw::c xview scroll $vd::gw::t0 unit

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

    # Geometry manager that packs around edges of cavity for the battery graph frame.
    pack $vd::bw::f -side right
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
    # disp_connect

    # Configure the main widget.
    disp_wm

    # Create and manipulate the container widgets.
    disp_frames

    # Create the stop button.
    disp_stop

    # Create the battery control button.
    disp_batt

    # Create the charging graph.
    disp_charging $vd::gw::t0

    # Create the display input boxes.
    disp_boxes

    # Start the timer for the display input from the controller.
    # disp_input
}

# Main process of the van display.
main
