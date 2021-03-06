#!/usr/bin/tclsh

package require Tk


package provide charts 0.3


#########################################################################
    
    #
    # This library provides routines used in creation of canvas-based
    # charts and graphs providing for the generation of linear axes,
    # value layouts, and legends.  The overall layout of the chart is
    # up to the caller.
    # 

    namespace eval charts {

        # webnice1 - A nice, web-friendly, palette.
        #
        #  51 153 204
        # 255 204 102
        # 153 153  51
        # 102 153 153
        # 153 204 255
        # 204 153  51
        # 204 204 153
        #   0 102 102
        # 153 153 204
        # 204 204 204
        # 153 153 153
        # 153 204 204
        #  51 102 153
        # 204 204  51
        # 204 255 153
        #  51 153 102
        # 153 153 255
        # 153 102  51
        # 153 204  51
        # 204 204 255
        # 204   0  51
        # 153   0   0

        variable palettes
        
        set palettes(webnice1) {
            #3399CC #FFCC66 #999933 #669999 #99CCFF #CC9933 #CCCC99 #006666 #9999CC #CCCCCC
            #999999 #99CCCC #336699 #CCCC33 #CCFF99 #339966 #9999FF #996633 #99CC33 #CCCCFF
            #CC0033 #990000
        }
    
    }


    #
    # nice_number
    #
    #   Reference: Paul Heckbert, "Nice Numbers for Graph Labels",
    #          Graphics Gems, pp 61-63.  
    #
    #   Finds a "nice" number approximately equal to x.
    #
    #   Args: x -- target number
    #         round -- If non-zero, round. Otherwise take ceiling of value.

    proc charts::nice_number {x round} {

        #   expt -- Exponent of x 
        #   frac -- Fractional part of x 
        #   nice -- Nice, rounded fraction 

        set expt [expr {floor(log10($x))}]
        set frac [expr {$x / pow(10.0, double($expt))}]
        if ($round) {
            if {$frac < 1.5} {
                set nice 1.0
            } elseif {$frac < 3.0} {
                set nice 2.0
            } elseif {$frac < 7.0} {
                set nice 5.0
            } else {
                set nice 10.0
            }
        } else {
            if {$frac <= 1.0} {
                set nice  1.0
            } elseif {$frac <= 2.0} {
                set nice  2.0
            } elseif {$frac <= 5.0} {
                set nice 5.0
            } else {
                set nice 10.0
            }
        }
        return [expr {$nice * pow(10.0, double($expt))}]
    }


    #
    # loose_label
    #
    #   Reference: Paul Heckbert, "Nice Numbers for Graph Labels",
    #          Graphics Gems
    #
    #   Returns a set of graph labelling attributes given a range of numbers
    #   that need to be graphed.
    #
    #   Args: min   -- Lower range boundary
    #         max   -- Upper range boundary
    #         steps -- Number of major tick marks desired
    #

    proc charts::loose_label {min max {steps 5}} {
        if {$steps < 2} { set steps 2 }
        # We expect min!=max 
        # Try and be nice by raising max by 1.  This obviously
        # fails miserably for small values.
        if {$min == $max} { set max [expr {$max + 1}] }

        set range [nice_number [expr {$max - $min}] 0]
        # tick mark spacing
        set d [nice_number [expr {$range / ($steps - 1)}] 1]
        set graphmin [expr {floor($min/$d) * $d}]
        set graphmax [expr {ceil($max/$d) * $d}]
        set nfx [expr {int(-floor(log10($d)))}]
        set nfrac [expr {($nfx > 0) ? $nfx : 0}]
        set stepfmt [format "%%.%df" $nfrac]

        set ticks [list]
        for {set x $graphmin} {$x < [expr {$graphmax + 0.5 * $d}]} {set x [expr {$x + $d}]} {
            lappend ticks $x
        }

        return [list graphmin $graphmin graphmax $graphmax step $d stepfmt $stepfmt ticks $ticks]
    }


    #
    # range_and_step
    #
    # Given a range (and a maximum number of steps), returns a 
    # "nice" range and "nice" step (tick) size.  This is useful
    # when the values being plotted aren't necessarily numbers
    # (e.g., time series or other alphanumeric series) where you
    # don't want to write out each value.
    #

    proc charts::range_and_step { range {steps 5}} {
        if {$steps < 2} { set steps 2 }
        set range [nice_number $range 0]
        set step  [nice_number [expr {$range / ($steps - 1)}] 1]
        return [list $range $step]
    }



    ##############################################################################################3


    #
    # draw_y_axis
    #
    # Draws a y-axis on a chart given the axis properties generated by
    # loose_label routine (or equivalent).   Assumes numeric data.
    #
    # Returns: y-coordinate of the zero crossing line (or 0.0 if no crossing).
    #
    #
    #   max -|<-height
    #        |
    #    nn -|
    #        |
    #    mm -|
    #        |
    #   min -|y
    #        x
    #  

    proc charts::draw_y_axis { canvas x y height font axisProps {fgcolor black} } {

        array set ap $axisProps
        set y1 [expr {$y - $height}]
        $canvas create line $x $y $x $y1
        set nTicks [llength $ap(ticks)]
        set tickIncr [expr {double($height) / ($nTicks - 1)}]
        set ty $y
        set zeroTickY 0.0
        foreach t $ap(ticks) {
            if {$t == 0} { set zeroTickY $ty }
            $canvas create line $x $ty [expr {$x - 5}] $ty
            $canvas create text [expr {$x - 7}] $ty -justify right -anchor e -text [format $ap(stepfmt) $t] -font $font
            set ty [expr {$ty - $tickIncr}]
        }
        return $zeroTickY
    }


    #
    # draw_x_axis
    #
    # Draws a y-axis on a chart given the axis properties generated by
    # loose_label routine (or equivalent).  Assumes numeric data (e.g., X/Y plots).
    #
    #   x                       v- width
    #  y+------------------------
    #   |    |    |    |    |   
    #  min   aa   bb   cc  max

    proc charts::draw_x_axis { canvas x y width font axisProps {fgcolor black} } {

        array set ap $axisProps
        set x1 [expr {$x + $width}]
        $canvas create line $x $y $x1 $y
        set nTicks [llength $ap(ticks)]
        set tickIncr [expr {$width / $nTicks}]
        set tx $x
        foreach t $ap(ticks) {
            $canvas create line $tx $y $tx [expr {$y + 5}]
            $canvas create text $tx [expr {$y + 7}]  -justify center -anchor n -text [format $ap(stepfmt) $t] -font $font
            set tx [expr {$tx + $tickIncr}]
        }
    }



    #
    # fit_x_axis_values
    #
    # Attempts to guide how to layout arbitrary x axis values. We cannot
    # rotate them, so we resort to multiple lines.  Crude heuristics:
    #   1. Try to fit in 1 line at the specified font size.
    #   2. Try to fit in 1 line at a reduced font size (two points lower)
    #   3. Try to fit in 2 lines at the specified font size.
    #   4. rinse and repeat...
    #
    # Currently it gives up at 3 lines because beyond that it'll look too 
    # ugly anyway. The two point reduction in size is obviously negotiable.
    # One point didn't seem to help a lot, and more than two delves into
    # readability issues.
    #
    # Returns a list consisting of the recommended number of lines for the
    # x-axis values and a recommended font size.
    #
    # This assumes that the values are all about the same size and makes
    # no attempt to optimize placement on a value-by-value basis (i.e., no
    # attempts to determine if overlaps will happen -- it just hopes for the
    # best).  In some respects, thinking about it too hard won't result in 
    # an any more attractive set of labels anyway.
    #

    proc charts::fit_x_axis_values { width font values } {
        set baseSize [font configure $font -size]
        set reducedSize [expr $baseSize - 2]
        # Create a temporary font for measurement purposes
        eval font create _fitx_reduced [font configure $font]
        font configure _fitx_reduced -size $reducedSize

        #
        # Add up all of the text so we can measure it
        #
        set t ""
        foreach v $values { append t $v " " }
        set bSize [font measure $font $t]
        set rSize [font measure _fitx_reduced $t]
        font delete _fitx_reduced

        # Our layout rules, such as they are...

        if {$bSize < $width} {
            return [list 1 $baseSize]
        } elseif {$rSize < $width} {
            return [list 1 $reducedSize]
        } elseif {[expr {round($bSize / 2.0)}] < $width} {
            return [list 2 $baseSize]
        } elseif {[expr {round($rSize / 2.0)}] < $width} {
            return [list 2 $reducedSize]
        } else {
            return [list 3 $reducedSize]
        }
    }


    #
    # layout_x_axis_values
    #
    # Draws the ticks and x-axis values on N lines with the specified font
    # and size.
    #
    # Returns a set of x coordinates where the ticks were drawn.  Presumably
    # you'll want to draw whatever somewhere over the value.
    #
    #   x                       v- width
    #  y+------------------------
    #   |    |    |    |    |   
    #   v1   v2   v3   v4   vN
    #
    #

    proc charts::layout_x_axis_values { canvas x y width font fSize lines values } {
        # We create a "working font" for the values so we can customize
        # the size of it, based on the font specified.  Only one per canvas!
        set xfont "$font$canvas"
        catch {font delete $xfont}
        eval font create $xfont [font configure $font]
        font configure $xfont -size $fSize

        # Pack in the lines as much as possible
        set linespace [expr {round([font metrics $xfont -linespace] * 0.8)}]

        set x1 [expr $x + $width]
        $canvas create line $x $y $x1 $y

        set nTicks [llength $values]
        set tickIncr [expr {$width / $nTicks}]
        set tx $x
        set line 0

        set xCoords [list]

        foreach t $values {
            $canvas create line $tx $y $tx [expr {$y + 5}]
            lappend xCoords $tx
            $canvas create text $tx [expr {$y + 3 + ($linespace * $line)}]  -justify center -anchor n -text $t -font $xfont
            set tx [expr {$tx + $tickIncr}]
            incr line
            if {$line >= $lines} { set line 0 }
        }

        return $xCoords

    }


    #
    # draw_legend
    #
    # Draws a legend box on the canvas starting at the x y coordinates
    # given.  Wraps values so they fit within maxWidth
    #
    # Layout of the legend area:
    #   x
    #  y+-----------------+
    #   | (rect) value1   |
    #   | (rect) value2   |
    #   +-----------------+
    #                     ^ resizes to text, no further than maxWidth
    #

    proc charts::draw_legend { canvas x y maxWidth font values colors {fgcolor black} } {
        
        # Remember how wide we've scribbled
        set width 0
        # Count the lines we've written
        set line 0
        # Pack in the lines as much as possible
        set lineSpace [expr {round([font metrics $font -linespace] * 0.8)}]
        
        set cx [expr {$x + 5}]
        set cy [expr {$y + 5}]

        set maxTextWidth [expr {$maxWidth - 15}]

        foreach v $values c $colors {
            $canvas create rectangle $cx $cy [expr {$cx + 10}] [expr {$cy + 7}] -fill $c
            set lWidth [expr {[font measure $font $v] + 13}]
            if {$lWidth < $maxWidth} {
                $canvas create text [expr {$cx + 13}] [expr {$cy + 4}] -text $v -font $font -anchor w -justify left
                if {$lWidth > $width} { set width $lWidth }
            } else {
                # Ugh. Text is too long.  Wrap it (character wrapping for now)
                set seg ""
                foreach w [split $v ""] {
                    if {[expr {[font measure $font $seg] + [font measure $font $w]}] > $maxTextWidth} {
                        $canvas create text [expr {$cx + 13}] [expr {$cy + 4}] -text $seg -font $font -anchor w -justify left
                        set cy [expr {$cy + $lineSpace}]
                        set width $maxWidth
                        set seg $w
                    } else {
                        append seg $w
                    }
                }
                $canvas create text [expr {$cx + 13}] [expr {$cy + 4}] -text $seg -font $font -anchor w -justify left
            }
            set cy [expr {$cy + $lineSpace}]
        }

        $canvas create rectangle $x $y [expr {$x + 10 + $width}] $cy
    }



    #
    # map_palette
    #
    # Given a list of values to chart, return a list of colors
    # to plot them in.  Built-in palette webnice 1 is used by
    # default.  Feel free to contribute your own.
    #
    # If the palette has fewer colors than there are values to
    # graph/plot, then we reuse the ones we have.  If the palette
    # is empty, we will return gray.
    #

    proc charts::map_palette { values {palette webnice1} } {
        variable palettes
        set p $palettes($palette)
        set colors [list]
        set x 0
        # Produce a list of colors for each value.  Wrap the palette
        # if we have too many.  If the palette is bogus, return gray.
        foreach v $values {
            set c [lindex $p $x]
            if {$c == ""} {
                set x 0
                set c [lindex $p $x]
                if {$c == ""} {
                    set c #808080
                }
            }
            lappend colors $c
            incr x
        }
        return $colors
    }


    #
    # allocate_space
    #
    # Given a width and a height, parcel out the space for the various chart
    # elements.  This is an EXTREMELY crude way of doing it and tends to be
    # ill-behaved when things get tight.
    #
    # Returns a set of suggested canvas coordinates and limits suitable for
    # tossing into an array.
    #

    proc charts::allocate_space { width height } {
    
        set chartBodyWidth  [expr {$width * 0.60}]
        set chartBodyHeight [expr {$height * 0.75}]
        set yMarginWidth    [expr {$width * 0.15}]
        set xMarginHeight   [expr {$height * 0.20}]

        set chartOriginX [expr {$yMarginWidth}]
        set chartOriginY [expr {$height - $xMarginHeight}]
        set xAxisX       [expr {$chartOriginX}]
        set xAxisY       [expr {$chartOriginY}]
        set yAxisX       [expr {$chartOriginX - 1}]
        set yAxisY       [expr {$chartOriginY}]

        set legendX      [expr {$chartOriginX + $chartBodyWidth}]
        set legendY      [expr {$height * 0.10}]
        set legendWidth  [expr {$width - $legendX - 2}]

        foreach v [info local] {
            lappend r $v [set $v]
        }

        return $r
    }


    ##############################

    #
    # The following is a very simple data file format for reading in chart data.
    # It would be better if it were XML based...
    #


    # 
    # parse_chart_file_data
    #
    # Function to parse the incoming chart data.  It's expected 
    # to be passed in in the following format: (each item delineated by a carriage return)
    #
    #       # comment
    #       Chart line|bar
    #       Width 400
    #       Height 300
    #       Labels date amount testingvalue
    #       Row Jan 55 4
    #       Row Feb 200 57
    #       Row Mar 17 5
    #       Row Sept 900 7
    #
    # Returns: an array with all the data
    #


    proc charts::parse_chart_file_data {datafile chartDatap} {
        upvar $chartDatap chartData

        set rowcount 0
        set chartData(Palette) webnice1

        # get each line
        while {[gets $datafile line] >= 0} {

            set data [split $line]
    
            set dataname [lindex $data 0]

            if {[string index $dataname 0] == "#"} continue

            # Process the commands we know about, ignore anything else
            switch -exact -- $dataname {
    
                "Type" -
                "Width" -
                "Height" -
                "Palette" {
                    set chartData($dataname) [lindex $data 1]
                }
                "Labels" { 
                    set chartData($dataname) [lrange $data 1 end]; 
                    set chartData(ColCount) [llength $chartData(Labels)]
                }
                "Row"    { 
                    set rowdata [lrange $data 1 end];
                    if {[llength $rowdata] == $chartData(ColCount)} {
                        set chartData(Row$rowcount) $rowdata;
                        incr rowcount 
                    }
                }
            }
        }

        set chartData(RowCount) $rowcount

        for {set c 0} {$c < $chartData(ColCount)} {incr c} {
            for {set r 0} {$r < $chartData(RowCount)} {incr r} {
                lappend chartData(col$c) [lindex $chartData(Row$r) $c]
            }
        }
    }


    proc charts::parse_chart_file { fileSpec chartDatap } {
        upvar $chartDatap chartData
        set readHandle [open $fileSpec r]
        parse_chart_file_data $readHandle chartData
        close $readHandle
    }


# Simple example of the axis drawing/layout routines:


    canvas .c -width 200 -height 150
    pack .c
    font create h10 -size 10
    # Set gwidth lower to see how x-axis layout adapts
    set gwidth 140
    set yap [charts::loose_label 0 857]
    charts::draw_y_axis .c  50 100 80 h10 $yap
    set xv {jan feb mar apr may}
    set fit [charts::fit_x_axis_values $gwidth h10 $xv]
charts::layout_x_axis_values .c 50 100 $gwidth h10 [lindex $fit 1] [lindex $fit 0] $xv
