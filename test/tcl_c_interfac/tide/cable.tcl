#!/usr/bin/tclsh

# Load dynmically the van OS into the statically-linked interpreter.
load ../../../lib/libvan[info sharedlibextension]

# Welcoming.
puts "Van Cable Test"

# Connect the cable with the display entry point.
set c [cable display]

# Trace the displace entry point name.
puts "Cable access name: $c"

# Read a message from the display input queue of cable display-controller.
set data [read $c]

# Test the cable output wire.
puts $c "b-on"

# Reconfigure a cable.
fconfigure $c -xxx 0

# Configures the behavior of a cable end point.
# If a single parameter is given on the command line, the value of that
# parameter is returned.
fconfigure $c -xxx

# Pull out the display plug.
close $c
