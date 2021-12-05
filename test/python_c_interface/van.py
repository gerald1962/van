from ctypes import *
lib_path = "/home/gerald/van/test/python_c_interface/van.so"
van = CDLL(lib_path)
van.hello.restype = None

