#!/usr/bin/python3

# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2021 Gerald Schueller <gerald.schueller@web.de>

# ============================================================================
# IMPORTED INCLUDE REFERENCES
# ============================================================================
import os             # OS interfaces: os.path.join().
import time           # Access to timing: time.time().
from ctypes import *  # Allow calling C functions in shared libraries.

# ============================================================================
# LOCAL DATA
# ============================================================================
# char *lib_path:  path for the van DLL.
# struc van:       list of the van interfaces.
# int id:          device id of the py shm driver.
# char buf[512]:   buffer for the van DL data.
# int start_time:  beginning of the measurement.
# int end_time:    end of the measurement.
# int n:           size of the DL payload.

# ============================================================================
# VAN DLL FOR BATTERY DESIGN.
# ============================================================================
# Load the van shared library.
lib_path = os.path.join(os.environ['HOME'],'van_development/van/lib/libvan.so')
van = CDLL(lib_path)

# Declarations of the van interfaces.
van.os_init.argtypes = None
van.os_init.restype  = None
van.os_trace_button.argtypes = [c_int]
van.os_trace_button.restype  = None
van.os_open.argtypes = [c_char_p, c_int]
van.os_open.restype  = c_int
van.os_read.argtypes = [c_int, c_char_p, c_int]
van.os_read.restype  = c_int
van.os_write.argtypes = [c_int, c_char_p, c_int]
van.os_write.restype  = None
van.os_close.argtypes = [c_int]
van.os_close.restype  = None
van.os_exit.argtypes = None
van.os_exit.restype  = None

# ============================================================================
# LOCAL FUNCTIONS
# ============================================================================
# van_loop() - test the receiving of DL data from van and return them to the
# generator.
#
# dev_id: identity code of the python shared memory device.
#
# Return:	None.
#
def van_loop(dev_id):
    # Allocate the buffer for sync_read.
    buf = create_string_buffer(2048)
    
    # Beginning of the measurement.
    start_time = time.time()

    # Read and analyze all DL data from van.
    i = 0
    while 1:
        # Copy data from the receive channel.
        n = van.os_read(dev_id, buf, 2048)
        print ('python> received: [b:{}, s:{}]' . format(buf.value, n))

        # End condition for the read loop and calcuate the execution time.
        if buf.value == b'That\'s it.':
            end_time = time.time()
            print('elapsed timme: {}' . format(end_time - start_time))
            break

        # Convert and test the received counter.
        if i != int(buf.value):
            raise
            
        # Return the received counter.
        van.os_write(dev_id, buf, n)

        i += 1
        #        
    return
#

# van_exit() - destroy the python shared memoy device and shutdown the van OS.
#
# dev_id: identity code of the python shared memory device.
#
# Return:	None.
#
def van_exit(dev_id):
    van.os_close(dev_id)
    van.os_exit()
    return

# van_init() - install the python shared memory device.
#
# Return:	identity code of the python device.
#
def van_init():
    # Boot the van OS, switch off tracing and create the python shared memory device
    # for the communication with van.
    van.os_init(0)
    van.os_trace_button(0)
    id = van.os_open(b"/python", 0)
    return id;
#

# main() - test the communication with van.
#
# Return:	None.
#
def main():
    # Install the python shared memory device.
    id = van_init();

    # Test the receiving of DL data from van.
    van_loop(id);
    
    # Destroy the python shared memoy device and shutdown the van OS.
    van_exit(id);
    return
#

# Entry point for battery development.
#
main();
