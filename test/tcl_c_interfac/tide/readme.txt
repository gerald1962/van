1 Tcl/Tk Extensions
-------------------
Reference  for development:
https://wiki.tcl-lang.org/page/Hello+World+as+a+C+extension
see

2 Makefile
----------
gcc -shared -o libhello.so -DUSE_TCL_STUBS -ICLINC hello.c -LCLLIB -ltclstub
hello.c:4:10: fatal error: tcl.h: Datei oder Verzeichnis nicht gefunden

3 Tcl-Van Extensions
--------------------
?sudo apt-get install tcl8.6-dev?
sudo apt-get install tcl-dev

3 Graphics with Tcl/Tk
----------------------
wish
% load ./libhello[info sharedlibextension]
% hello
Hello, World!
exit

4 Graphics with Tcl/Tk
----------------------
wish
% load ../../lib/libvan[info sharedlibextension]

5. Van OS extension
-------------------
os_tcl.c

6 Shared Libraray
-----------------
cd (van_develpment || github)/van/test/lib
make
/home/gerald/van_development/van/os/os_tcl.c:22: undefined reference to `Tcl_PkgInitStubsCheck'


7. Buch
-------
http://www.tcltk.info/einfuehrung_in_tcl_tk_buch.pdf
http://www.tcltk.info/buch.html

http://www.beedub.com/book/3rd/Cprogint.pdf
https://en.wikibooks.org/wiki/Tcl_Programming

The Tcl Programming Language: A Comprehensive Guide (2017), by Ashok Nadkarni, focuses on the Tcl language itself in great detail. Covers Tcl 8.6.

https://wiki.tcl-lang.org/


7 Tcl Man Page
--------------
* https://www.tcl.tk/man/tcl8.6/contents.html

http://tmml.sourceforge.net/doc/tcl/index.html
http://tmml.sourceforge.net/doc/tk/index.html

https://wiki.tcl-lang.org/page/Snit%27s+Not+Incr+Tcl

8 Tk Tutorial
-------------
* https://tkdocs.com/tutorial/index.html
https://www-user.tu-chemnitz.de/~hot/tcl_tk

os_socket.c    -> sod_xy
os_connector.c -> cod_xy

9 Tcl Script
------------
#!/usr/bin/tclsh

load ../../lib/libvan[info sharedlibextension]
puts "Hello World"

10. Wish Script
---------------
#!/usr/bin/wish
