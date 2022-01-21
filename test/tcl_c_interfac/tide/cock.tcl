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
while { 1 } {
    # Read a message from the display input queue of the display-controller cable.
    set n [gets $c msg]

    # Test the message length.
    if { $n < 1} {
	continue
    }

    # Trace the input message.
    puts "D> rcvd: \[m=\"$msg\", l=$n\]"

    # Analyze the message contents.
    if { $msg == "That's it." } {
	break
    }

    # Test the input message.
    if { $msg != $i } {
	error "input error: see trace"
    }
    
    # Get back the message to the controller.
    puts $c $msg

    # Send the message without buffering.
    flush $c

    # Get the status of the cable output queue.
    set ostat [fconfigure $c -writable]
    puts "D> o-status=$ostat"

    # Test the status of the cable output queue.
    if { $ostat != 1 } {
	error "output error: see trace"
    }
    
    # Trace the output message.
    puts "D> sent: \[m=\"$msg\", l=$n\]"

    # Increment the message counter.
    incr i
}

# Pull out the display plug.
close $c
