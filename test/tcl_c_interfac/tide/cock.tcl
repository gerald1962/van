#!/usr/bin/tclsh

# SPDX-License-Identifier: GPL-2.0

# cock - Cable Controller Knob
#
# Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>

# Load dynmically the van OS into the statically-linked interpreter.
load ../../../lib/libvan[info sharedlibextension]

# Welcoming.
puts "Cable Controller Knob"

# Connect the display-controller cable.
set c [cable display]

# Trace the displace entry point name.
puts "Cable access name: $c"

# Initialize the message counter.
set i 0

# Receive or send controller messages.
set busy 1
while { $busy == 1 } {
    # Read a message from the display input queue of the display-controller cable.
    set n [gets $c msg]

    # Test the message length.
    if { $n < 1} {
	continue
    }

    # Trace the input message.
    puts "D> rcvd: \[m=\"$msg\", l=$n\]"

    # Analyze the message contents.
    if { $msg == "DONE" } {
	set busy 0
    } else {
	# Test the input message.
	if { $msg != $i } {
	    error "input error: see trace"
	}
    }
    
    # Wait for the free message buffer.
    set free 0
    while { $n > $free } {
	# Get the size of the next free message buffer.
	set free [fconfigure $c -writable]
    }
    
    # Get back the message to the controller.
    puts $c $msg

    # Send the message without buffering.
    flush $c
    
    # Trace the output message.
    puts "D> sent: \[m=\"$msg\", l=$n, f=$free\]"

    # Increment the message counter.
    incr i
}

# Pull out the display plug.
close $c
