#!/usr/bin/tclsh

# tcl_wiki_moving_text_egg.tcl           
# program moving with text tagged to object  
# pretty print from autoindent and ased editor
# moving with text tagged to object
# working under TCL version 8.5.6 and eTCL 1.0.1
# program written on Windows XP on eTCL
# gold on TCL WIKI, 24Mar2017
package require Tk
package require math::numtheory

namespace path {::tcl::mathop ::tcl::mathfunc math::numtheory }

set tcl_precision 17

proc grab { xx yy } {
    global currentx currenty
    set currentx $xx
    set currenty $yy
}

proc drag {w xx yy } {
    global currentx currenty
    set dx [expr {$xx - $currentx}]
    set dy [expr {$yy - $currenty}]
    .cv move first $dx $dy
    $w raise first
    set currentx $xx
    set currenty $yy                
}

canvas .cv -width 200 -height 200 -bg bisque
pack .cv
.cv create oval 10 10 30 30 -fill red -tag first
.cv create text 20 20   -text @ -fill blue -tag first
.cv create rect 110 10 130 30 -fill green -tag second
.cv create rect 10 110 30 130 -fill yellow -tag second
.cv bind first <Button-1> {grab %x %y }
.cv bind first <B1-Motion> {drag .cv %x %y }
.cv bind drag <B1-Motion> {drag .cv %x %y}
wm title . "Canvas Demo  Moving Text & Egg"
