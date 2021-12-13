#!/usr/bin/python3

import os
import time
from ctypes import *
lib_path = os.path.join(os.environ['HOME'],'van/lib/libvan.so')
van = CDLL(lib_path)
van.os_init.restype = None
van.os_trace_button.restype = None
van.os_open.restype = c_int
van.os_sync_read.restype = c_int
van.os_sync_read.argtypes = [c_int, c_char_p, c_int]
van.os_exit.restype = None

van.os_init()
van.os_trace_button(0)
id = van.os_open(b"/python")

buf = create_string_buffer(512)

start_time = time.time()

while 1:
    n = van.os_sync_read(id, buf, 512)
    print ('received: [b:{}, s:{}]' . format(buf.value, n))
    if buf.value == b'That\'s it.':
        end_time = time.time()
        print(end_time - start_time)
        break

van.os_close(id)
van.os_exit()
