#!/usr/bin/tclsh

# SPDX-License-Identifier: GPL-2.0

# van display
#
# Copyright (C) 2022 Gerald Schueller <gerald.schueller@web.de>

# Load dynmically the van OS into the statically-linked interpreter.
# load ../../../lib/libvan[info sharedlibextension]

# Welcoming.
puts "van display"

# Connect the display-controller cable.
set c [cable /van/display]

# Trace the displace entry point name.
puts "Cable access name: $c"

# Test loop.
while { 1 } {
    # Send a test message.
    puts $c "test"

    # Send the message without buffering.
    flush $c

    # Sleep 1 second.
    after 1000
}

# Pull out the display plug.
close $c
