#!/usr/bin/tclsh

package require Tk

proc sleep {time} {
   after $time set end 1
   vwait end
}
# --------------------------
#   params
# --------------------------
# title
# canvas width & height
# delay between plots
# x = f(t)
# plot expression
# initial & final times
# accuracy
array set params {
    title     {sin(x)}
    width     400
    height    240
    delay     50
    x         {$t / 50.}
    plot      {sin($x)}
    t0        0
    #         {t1 < t0: non-stop scrolling}
    t1        -1
    accuracy  1.e-2
}

# --------------------------
#   plotting
# --------------------------
# computed heights
set h $params(height)
set h1 [expr {int($h * 0.5)}]  ;# canvas mid-height
set h2 [expr {$h1 + 1}]
set h3 [expr {int($h * 0.4)}]  ;# graph mid-height
# canvas & bindings
canvas .c -width $params(width) -height $h -xscrollincrement 1 -bg black
bind . <Destroy> { exit }
# plotting
wm title . $params(title)
pack .c
.c xview scroll $params(t0) unit
set t $params(t0)

while TRUE {
        if {$t != $params(t1)} {
                set x [expr $params(x)]
                set vv [expr $params(plot)]
                set v [expr {int($vv * $h3) + $h1}]
                if {abs($vv) < $params(accuracy)} \
                { 
                                #puts "$vv [expr {$params(accuracy)}]" ;#because I wanted to understand the points
                                .c create text $t 0 -anchor n -text [expr { $t / 50. }] -fill green
                                .c create line $t 0 $t $h -fill green
                }
                .c create line $t $h1 $t $h2 -fill green
                .c create rectangle $t $v $t $v -outline green
                incr t
                if {$t > $params(width)} { .c xview scroll 1 unit }
        }
        sleep $params(delay)
}
