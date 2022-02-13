#!/usr/bin/tclsh

package require Tk

# slipchart.tcl --
 #    Facilities to draw a slipchart in a dedicated canvas
 #

 # Slipchart --
 #    Namespace to hold the procedures and the private data
 #
 namespace eval ::Slipchart {
    variable scaling
    variable data_series

    namespace export worldCoordinates viewPort createSlipchart \
                     coordsToPixel
 }
 # viewPort --
 #    Set the pixel extremes for the graph
 # Arguments:
 #    w           Name of the canvas window
 #    pxmin       Minimum X-coordinate
 #    pxmax       Maximum X-coordinate
 #    pymin       Minimum Y-coordinate
 #    pymax       Maximum Y-coordinate
 # Result:
 #    None
 # Side effect:
 #    Array scaling filled
 #
 proc ::Slipchart::viewPort { w pxmin pxmax pymin pymax } {
    variable scaling

    set scaling($w,pxmin)    $pxmin
    set scaling($w,pymin)    $pymin
    set scaling($w,pxmax)    $pxmax
    set scaling($w,pymax)    $pymax
    set scaling($w,new)      1
 }

 # worldCoordinates --
 #    Set the extremes for the world coordinates
 # Arguments:
 #    w           Name of the canvas window
 #    width       Width of the canvas window
 #    height      Height of the canvas window
 #    xmin        Minimum X-coordinate
 #    xmax        Maximum X-coordinate
 #    ymin        Minimum Y-coordinate
 #    ymax        Maximum Y-coordinate
 # Result:
 #    None
 # Side effect:
 #    Array scaling filled
 #
 proc ::Slipchart::worldCoordinates { w xmin xmax ymin ymax } {
    variable scaling

    set scaling($w,xmin)    $xmin
    set scaling($w,ymin)    $ymin
    set scaling($w,xmax)    $xmax
    set scaling($w,ymax)    $ymax

    set scaling($w,new)     1
 }

 # coordsToPixel --
 #    Convert world coordinates to pixel coordinates
 # Arguments:
 #    w           Name of the canvas
 #    xcrd        X-coordinate
 #    ycrd        Y-coordinate
 # Result:
 #    List of two elements, x- and y-coordinates in pixels
 #
 proc ::Slipchart::coordsToPixel { w xcrd ycrd } {
    variable scaling

    if { $scaling($w,new) == 1 } {
       set scaling($w,new)     0
       set width               [expr {$scaling($w,pxmax)-$scaling($w,pxmin)+1}]
       set height              [expr {$scaling($w,pymax)-$scaling($w,pymin)+1}]

       set dx                  [expr {$scaling($w,xmax)-$scaling($w,xmin)}]
       set dy                  [expr {$scaling($w,ymax)-$scaling($w,ymin)}]
       set scaling($w,xfactor) [expr {$width/$dx}]
       set scaling($w,yfactor) [expr {$height/$dy}]
    }

    set xpix [expr {$scaling($w,pxmin)+($xcrd-$scaling($w,xmin))*$scaling($w,xfactor)}]
    set ypix [expr {$scaling($w,pymin)+($scaling($w,ymax)-$ycrd)*$scaling($w,yfactor)}]
    return [list $xpix $ypix]
 }

 # createSlipchart --
 #    Create a command for drawing a slipchart
 # Arguments:
 #    w           Name of the canvas
 #    xscale      Minimum, maximum and step for x-axis (initial)
 #    yscale      Minimum, maximum and step for y-axis
 # Result:
 #    Name of a new command
 # Note:
 #    The entire canvas will be dedicated to the slipchart.
 #    The slipchart will be drawn with axes
 #
 proc ::Slipchart::createSlipchart { w xscale yscale } {
    variable data_series

    foreach s [array names data_series "$w,*"] {
       unset data_series($s)
    }

    set newchart "slipchart_$w"
    interp alias {} $newchart {} ::Slipchart::DrawData $w

    set pxmin 80
    set pymin 20
    set pxmax [expr {[$w cget -width]  - 40}]
    set pymax [expr {[$w cget -height] - 20}]

    foreach {xmin xmax xdelt} $xscale {break}
    foreach {ymin ymax ydelt} $yscale {break}

    viewPort         $w $pxmin $pxmax $pymin $pymax
    worldCoordinates $w $xmin  $xmax  $ymin  $ymax

    DrawYaxis        $w $ymin  $ymax  $ydelt
    DrawXaxis        $w $xmin  $xmax  $xdelt
    DrawMask         $w

    return $newchart
 }

 # DrawYaxis --
 #    Draw the y-axis
 # Arguments:
 #    w           Name of the canvas
 #    ymin        Minimum y coordinate
 #    ymax        Maximum y coordinate
 #    ystep       Step size
 # Result:
 #    None
 # Side effects:
 #    Axis drawn in canvas
 #
 proc ::Slipchart::DrawYaxis { w ymin ymax ydelt } {
    variable scaling

    $w create line $scaling($w,pxmin) $scaling($w,pymin) \
                   $scaling($w,pxmin) $scaling($w,pymax) \
                   -fill black -tag yaxis

    set y $ymin
    while { $y < $ymax+0.5*$ydelt } {
       foreach {xcrd ycrd} [coordsToPixel $w $scaling($w,xmin) $y] {break}
       $w create text $xcrd $ycrd -text $y -tag yaxis -anchor e
       set y [expr {$y+$ydelt}]
    }
 }

 # DrawXaxis --
 #    Draw the x-axis
 # Arguments:
 #    w           Name of the canvas
 #    xmin        Minimum x coordinate
 #    xmax        Maximum x coordinate
 #    xstep       Step size
 # Result:
 #    None
 # Side effects:
 #    Axis drawn in canvas
 #
 proc ::Slipchart::DrawXaxis { w xmin xmax xdelt } {
    variable scaling

    $w delete xaxis

    $w create line $scaling($w,pxmin) $scaling($w,pymax) \
                   $scaling($w,pxmax) $scaling($w,pymax) \
                   -fill black -tag xaxis

    set x $xmin
    while { $x < $xmax+0.5*$xdelt } {
       foreach {xcrd ycrd} [coordsToPixel $w $x $scaling($w,ymin)] {break}
       $w create text $xcrd $ycrd -text $x -tag xaxis -anchor n
       set x [expr {$x+$xdelt}]
    }

    set scaling($w,xdelt) $xdelt
 }

 # DrawMask --
 #    Draw the stuff that masks the data lines outside the graph
 # Arguments:
 #    w           Name of the canvas
 # Result:
 #    None
 # Side effects:
 #    Several polygons drawn in the background colour
 #
 proc ::Slipchart::DrawMask { w } {
    variable scaling

    set width  [$w cget -width]
    set height [expr {[$w cget -height] + 1}]
    set colour [$w cget -background]
    set pxmin  $scaling($w,pxmin)
    set pxmax  $scaling($w,pxmax)
    set pymin  $scaling($w,pymin)
    set pymax  $scaling($w,pymax)
    $w create rectangle 0 0      $pxmin $height -fill $colour -outline $colour -tag mask
    $w create rectangle 0 0      $width $pymin  -fill $colour -outline $colour -tag mask
    $w create rectangle 0 $pymax $width $height -fill $colour -outline $colour -tag mask

    $w lower mask
 }

 # DrawData --
 #    Draw the x-axis
 # Arguments:
 #    w           Name of the canvas
 #    series      Data series
 #    xcrd        Next x coordinate
 #    ycrd        Next y coordinate
 # Result:
 #    None
 # Side effects:
 #    Axis drawn in canvas
 #
 proc ::Slipchart::DrawData { w series xcrd ycrd } {
    variable data_series
    variable scaling

    if { $xcrd > $scaling($w,xmax) } {
       set xdelt $scaling($w,xdelt)
       set xmin  $scaling($w,xmin)
       set xmax  $scaling($w,xmax)

       set xminorg $xmin
       while { $xmax < $xcrd } {
          set xmin [expr {$xmin+$xdelt}]
          set xmax [expr {$xmax+$xdelt}]
       }
       set ymin  $scaling($w,ymin)
       set ymax  $scaling($w,ymax)


       worldCoordinates $w $xmin $xmax $ymin $ymax
       DrawXaxis $w $xmin $xmax $xdelt

       foreach {pxminorg pyminorg} [coordsToPixel $w $xminorg $ymin] {break}
       foreach {pxmin pymin}       [coordsToPixel $w $xmin    $ymin] {break}
       $w move data [expr {$pxminorg-$pxmin+1}] 0
    }

    #
    # Draw the line piece
    #
    if { [info exists data_series($w,$series,x)] } {
       set xold $data_series($w,$series,x)
       set yold $data_series($w,$series,y)
       foreach {pxold pyold} [coordsToPixel $w $xold $yold] {break}
       foreach {pxcrd pycrd} [coordsToPixel $w $xcrd $ycrd] {break}
       $w create line $pxold $pyold $pxcrd $pycrd \
                      -fill black -tag data
       $w lower data
    }

    set data_series($w,$series,x) $xcrd
    set data_series($w,$series,y) $ycrd
 }

 #
 # Main code
 #
 canvas .c -background white -width 400 -height 200
 pack   .c -fill both

 set s [::Slipchart::createSlipchart .c {0.0 100.0 10.0} {0.0 100.0 20.0}]

 proc gendata {slipchart xold xd yold yd} {
    set xnew  [expr {$xold+$xd}]
    set ynew  [expr {$yold+(rand()-0.5)*$yd}]
    set ynew2 [expr {$yold+(rand()-0.5)*2.0*$yd}]
    $slipchart series1 $xnew $ynew
    $slipchart series2 $xnew $ynew2

    after 500 [list gendata $slipchart $xnew $xd $ynew $yd]
 }

after 100 [list gendata $s 0.0 15.0 50.0 30.0]
