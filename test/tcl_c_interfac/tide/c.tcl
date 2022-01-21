#!/usr/bin/tclsh

# cock - Cable Controller Knob

# # Load dynmically the van OS into the statically-linked interpreter.
# load ../../../lib/libvan[info sharedlibextension]

# Welcoming.
puts "Cable Controller Knob"

# Connect the display-controller cable.
set c [cable display]

# Trace the displace entry point name.
puts "Cable access name: $c"

# Read all controller messages.
while { 1 } {
    # Read a message from the display input queue of the display-controller cable.
    set msg [read $c]

    # Trace the received message.
    puts $msg

    # Analyze the message contents.
    if { $msg == "That's it." } {
	break
    }
    
    # Get back the message to the controller.
#    puts $c $msg
}

# Pull out the display plug.
close $c
