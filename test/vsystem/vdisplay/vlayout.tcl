#!/usr/bin/tclsh

# Evaluates the Tk script when this package is required and then ensures that
# the package is present.
package require Tk

# Pass the string to the window manager for use as the title for the display
# window.
wm title . "vLayout"

# If width and height are specified, they give the minimum permissible
# dimensions for window.
wm minsize . 1100  400

# If width and height are specified, they give the maximum permissible
# dimensions for window.
wm maxsize . 1100  400

# Create and manipulate the container widget for the switches.
frame .sf -background red   -relief ridge -borderwidth 8 -padx 10 -pady 10 -width 150 -height 400

# Create and manipulate the container widget for the graphs.
frame .gf -background green -relief ridge -borderwidth 8 -padx 10 -pady 10 -width 750 -height 400

# Create and manipulate the container widget for the boxes to display the
# current input from the controller.
frame .bf -background blue  -relief ridge -borderwidth 8 -padx 10 -pady 10 -width 200 -height 400

# -side side
# Specifies which side of the container the content will be packed against. Must
# be left, right, top, or bottom. Defaults to top.

# Geometry manager that packs around edges of cavity for the battery control.
pack .sf -side left

# Geometry manager that packs around edges of cavity for the display of graphs.
pack .gf -side left

# Geometry manager that packs around edges of cavity for the display of the controller input.
pack .bf -side right
