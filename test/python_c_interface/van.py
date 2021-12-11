from ctypes import *
lib_path = "/home/gerald/van/test/python_c_interface/van.so"
van = CDLL(lib_path)
van.s_write.restype = c_int
van.s_read.restype = c_int

s = "Hello, van"
c_s = c_char_p(s)
print(c_s.value)

n = len(s)
c_n = c_int(n)
print(c_n.value)

c_w = van.s_write(c_s, c_n)

c_r = van.s_read(c_s, c_n)
print(c_s.value)
