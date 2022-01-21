#!/usr/bin/tclsh

# cock - Cable Controller Knob

# Load dynmically the van OS into the statically-linked interpreter.
load ../../../lib/libvan[info sharedlibextension]

# Welcoming.
puts "Cable Controller Knob"

# Connect the display-controller cable.
set c_id [cable display]

# Trace the displace entry point name.
puts "Cable access name: $c_id"

proc msg_get {chan} {
#    if {![eof $chan]} {
        puts "msg_get ..."
#    }
}

proc msg_put {chan} {
#    if {![eof $chan]} {
        gets "msg_put ..."
#    }
}

fileevent $c_id readable [list msg_get $c_id]
fileevent $c_id writable [list msg_put $c_id]

# milliseconds
proc sleep {n} {
    after [expr {int($n * 100)}]
}

# Read or write controller messages.
while { 1 } {
    # XXX
    # update
    sleep 10
}

# Pull out the display plug.
close $c_id
