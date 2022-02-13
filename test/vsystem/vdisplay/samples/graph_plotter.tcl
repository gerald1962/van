#!/usr/bin/tclsh

package require Tk
package require math

proc graphscale {v vmin vmax pixels} {
    return [expr {$pixels*($v-$vmin)/($vmax-$vmin)}]
}

proc plot {res labels} {
    # adapted the simple graph plotter, https://wiki.tcl-lang.org/8552
    upvar [set state [namespace current]::plot_[clock clicks]] plot
    array set plot [list busy 0 colors {ff 00 00} res $res labels $labels]
    set plot(newcolors) $plot(colors)
    set width [expr {[winfo screenwidth .] / 2}]
    set height [expr {[winfo screenheight .] / 2}]
    # canvas
    if [winfo exists .c] { ;# delete it. Same as clear.
        destroy .c
    }
    canvas .c -width $width -height $height \
        -xscrollincrement 1 -bg beige
    pack .c -expand 1 -fill both
    wm title . Graph
    bind .c <Configure> [list draw $state]
    event generate .c <Configure>
    return $state
}

proc draw {plot} {
    upvar [namespace current]::$plot plot[unset plot]
    if {$plot(busy)} return
    set plot(busy) 0
    .c addtag all all
    .c delete all
    set plot(newcolors) $plot(colors)
    set res [lsort -real -index 0 $plot(res)]
    # get range of x & y
    set width [winfo width .c]
    set height [winfo height .c]
    variable xmin {} xmax {} ymin {} ymax {}
    set nvars [expr {[llength [lindex $res 0]]-1}]
    foreach item $plot(res) {
        set y [lassign $item r]
        set xmin [::math::min {*}$xmin $r]
        set xmax [::math::max {*}$xmax $r]
        foreach yv $y { ;# get range of Y coordinate
            set ymin [::math::min {*}$ymin $yv]
            set ymax [::math::max {*}$ymax $yv]
        }
    }

    #how often tick marks
    set tspace [expr {($xmax-$xmin)/8.0}] ;# how often to draw grid in X
    set nexttic [expr {int($xmin/$tspace)*$tspace}]
    while {$nexttic < $width} { ;# draw grid
        set xpix [graphscale $nexttic $xmin $xmax $width]
        set nexttic [expr {$nexttic+$tspace}]
        .c create text [expr {$xpix-10}] 0 -anchor n -text [format %.2f $nexttic] \
            -fill gray
        .c create line $xpix 0 $xpix $height -fill gray
    }

    #how often Ytick marks
    set tspace [expr {($ymax-$ymin)/8.0}] ;# how often to draw grid in X
    set nexttic [expr {int($ymin/$tspace)*$tspace}]
    while {$nexttic < $ymax} { ;# draw grid
        set ypix [graphscale $nexttic $ymax $ymin $height]
        set nexttic [expr {$nexttic+$tspace}]
        .c create text 20 [expr {$ypix-10}] -anchor n -text [format %.2f $nexttic] \
            -fill gray
        .c create line 0 $ypix $width $ypix -fill gray
    }

    set lastheight $height
    for {set iy 0} {$iy<$nvars} {incr iy} {
        lassign $plot(newcolors) red green blue
        lappend plot(newcolors) $red 
        set plot(newcolors) [lreplace $plot(newcolors) 0 0]

        #label graph
        set label [lindex $plot(labels) $iy] 
        .c create text -1000 0 -anchor n -text $label \
            -fill #$red$green$blue -tag $label
        lassign [.c bbox $label] textx1 texty1 textx2 texty2
        set textwidth [expr {abs($textx2-$textx1)}]
        set textheight [expr {abs($texty2-$texty1)}]
        .c coords $label [expr {$width-$textwidth}] \
            [set lastheight [expr {$lastheight-$textheight}]]

        set coordlist {}
        foreach item $res {
                set y [lassign $item r]
                set xpix [graphscale $r $xmin $xmax $width]
                set vv [lindex $y $iy]
                set v [graphscale $vv $ymax $ymin $height]
                lappend coordlist $xpix $v
           set rold $xpix
           set yold $v         ;# vector of y values
         }
        .c create line $coordlist -fill #$red$green$blue
    }
    set plot(busy) 0
}

proc examples {} {
    ##Exercise the graph thus:
    #set plot [list] 
    #for {set i 0} {$i<20} {incr i 1} {
    #    set plot [linsert $plot 0 [list [expr {$i/3.0}] [expr {sin($i/3.0)}]]]
    #}
    #set names Sine

    # Or 2 graphs (values of sin(x) and cos(x) are placed in a list of results at x):

    set plot [list] 
    for {set i 0} {$i<20} {incr i} {
        lappend plot [list [expr {$i/3.0}] [expr {sin($i/3.0)}] \
            [expr {cos($i/3.0)}]]
    }

    ##set data [list 52 97 113 33 189 17 118 22 68 163]
    #set data [list 40 20 180 80 100 120 140 160 60 200]
    #foreach item $data {
    #    lappend plot [list [expr {$item/3.0}] [expr {sin($item/3.0)}] [expr {cos($item/3.0)}]]
    #}

    set names {Sine Cos}
    plot $plot $names
}

examples
