#!/usr/bin/tclsh

package require Tk


proc grapharea {w args} { ;# a graph plotter canvas derived thing.
        set dx 400
        set dy 400
        global $w.props ;# list of specialised options
        array set $w.props { -pencolour black -xmin -1 -xmax 1 -ymin -1 -ymax 1}
        array set options {}
        # split off the custom options:
        set generalopts [list]
        foreach {opt val} $args {
                if {[array names $w.props $opt]!=""} {set options($opt) $val
                } else { lappend generalopts $opt $val }
        }
        eval canvas $w $generalopts ;# generalopts are the options inherited from canvas.

        interp hide {} $w
        # Install the alias:
        interp alias {} $w {} graphareaCmd $w
        foreach opt [array names options] {
                $w configure $opt $options($opt)
        }
          return $w ;# the original canvas
 }
  proc graphareaCmd {self cmd args} {
        switch -- $cmd {
                configure {eval graphareaConfigure $self $cmd $args}
                cget {eval graphareaCget $self $args}
                showgrid { ;# draw a grid of lines on the graph
                        set x0 [$self cget -xmin];                set x1 [$self cget -xmax]
                        set y0 [$self cget -ymin];                set y1 [$self cget -ymax]
                        set dx [lindex $args 0] ;#number of grids in X direction
                        set x [expr int($x0/$dx)*$dx] ;# choose lines s.t. zero is a line.
                        while {$x<$x1} {
                                $self create line $x $y0 $x $y1
                                set x [expr $x+$dx]
                        }
                        set dy [lindex $args 1] ;# No grids in Y direction
                        set y [expr int($y0/$dy)*$dy]
                        while {$y<$y1} {
                                $self create line $x0 $y $x1 $y
                                set y [expr $y+$dy]
                        }
                }
                  create  { ;# replaces the create default of a canvas which works in pixels
                        #- adds text, rectangle, oval, polygon... (eg) positioned at scaled position
                        set args [eval concat $args] ;# this removes a level of curlies if necessary from the list.
                        set penc [$self cget -pencolour];# get "local" pen colour name
                        # scale factor for draw space to pixels
                        set x0 [$self cget -xmin];                set x1 [$self cget -xmax]
                        set y0 [$self cget -ymin];                set y1 [$self cget -ymax]
                        set wid [$self cget -width]
                        set ht [$self cget -height]
                        set idx 1 ;# where to start in args for coordinates
                        while {[string is double [lindex $args 1]] && [llength $args]>1} {
                                if {$idx%2==1} {
                                        lappend xylist [expr {int($wid*double([lindex $args 1]-$x0)/($x1-$x0))}]
                                } else { ;# a y coordinate
                                        lappend xylist [expr {int($ht*double([lindex $args 1]-$y1)/($y0-$y1))}]
                                }
                                set args [lreplace $args 1 1]
                                incr idx
                        }
                        switch [lindex $args 0] {
                                {line}  -
                                {text} {
                                        lappend command [lindex $args 0] $xylist -fill $penc
                                        if {[llength $args]>1} { set command [concat $command [lrange $args 1 end]] }
                                        eval interp invokehidden {{}} $self create $command
                                }
                                {default} {
                                        lappend command [lindex $args 0] $xylist -outline $penc
                                        if {[llength $args]>1} { set command [concat $command [lrange $args 1 end]] }
                                        eval interp invokehidden {{}} $self create $command
                                }
                        }
                }
                  default  { ;# pass to default of a canvas which works in pixels
                        #puts "Action $cmd $args"
                        eval interp invokehidden {{}} $self $cmd $args
                }
        }
  }
  proc graphareaConfigure {self cmd args} {
        # 3 scenarios:
        #
        # $args is empty       -> return all options with their values
        # $args is one element -> return current values
        # $args is 2+ elements -> configure the options
        global $self.props
        switch [llength $args] {
                0 { ;# return argument values
                        lappend result [array names $self.props]

                        # default options:
                        lappend result [interp invokehidden {} $self configure ]
                        #        lappend result [uplevel 1  $self cconfigure]
                        return $result
                }
                1 {
                        if {[array names $self.props $args ] != ""} {
                                 lappend opts [$self cget $args ]
                        } else {
                                set opts [uplevel 1 interp invokehidden {} $self configure $args]
                        }
                        return $opts
                        }
                default { ;# >1 arg - an option and its value
                                # go through each option:
                                foreach {option value} $args {
                                        #puts "setting $option to $value [array names $self.props]"
                                        if {[array names $self.props $option ]!=""} {
                                                set $self.props($option) $value
                                        } else {
                                                puts " default $option, $value for $self";$self configure $option $value
                                        }
                                }
                        
                        return {}
                }
        }
  }
  proc graphareaCget {self args} {
        # cget defaults done by the canvas cget command
        #puts "In graphareaCget option $self $args"
        upvar #0 $self.props props
        if {[array names props $args]!=""} { return $props($args) }
        return [uplevel 1 [list interp invokehidden {} $self cget $args]]
  }

  proc testplotc {} { ;#Now exercise this 'new' widget 
        catch {destroy .fib}
        # create a grapharea widget.
         set dr2 [grapharea .fib -width 500 -height 400 -pencolour red -xmin -1 -xmax 20  -ymin -1.02 -ymax 1.02]
         # points are drawn with the canvas area scaled to these values:
  #        $dr2 configure -xmin -2 -xmax 20  -ymin -1.02 -ymax 1.02
        pack $dr2 -expand true -fill both
            $dr2 showgrid 5 1

        $dr2 configure -pencolour blue ;# this value persist for all lines from here
        set xold 0
        set yold [expr sin(0)]
        for {set x 0} {$x<20} {set x [expr $x+.25]} {
                set y [expr sin($x)*exp(-0.1*$x)]
                $dr2 create line $xold $yold $x $y
                set xold $x
                set yold $y
        }
        $dr2 configure -pencolour orange
        set xold 0
        set yold [expr cos(0)]
        for {set x 0} {$x<20} {set x [expr $x+.25]} {
                set y [expr cos($x)*exp(-0.1*$x)]
                $dr2 create line $xold $yold $x $y
                set xold $x
                set yold $y
        }
        $dr2 configure -pencolour green
        # a simple rectangle drawn as a line with 5 coordinates (10 values, last repeats first)
        $dr2 create line  1 .8 19 .8 19 -.8 1 -.8 1 .8
        $dr2 configure -pencolour yellow
        #$dr2  create polygon 0  -1 1.0 -.9 2 -.7 3 -.2 4 0.5 2 .8 -smooth true
        $dr2  create rectangle 0  -1 10.0 1.0
        $dr2  create oval 0  -1 10.0 1.0
        # you can also add 'pure' canvas options to the grapharea using pixel coordinates:
        interp invokehidden {} $dr2  create text 120 30  -fill black -text "Graph Area"
        $dr2 configure -pencolour purple
      # or you can use the graph space to define the text position
        $dr2 create text 10.0 0.9 -text {{Text Placed using Graph Space}}
        $dr2 create line 4.0 0.86 14.0 .86
        # creating a long line of coordinates
        set xys ""
        for {set x 0} {$x<20} {set x [expr $x+.25]} {
                set y [expr cos(2*$x)*sin(0.3*$x)]
                lappend xys  $x $y
        }
        puts "Length [llength $xys] [$dr2 cget -pencolour] [$dr2 cget -xmin] [$dr2 cget -xmax]"
        puts "[$dr2 configure]"
        puts "[$dr2 configure -pencolour]"
        $dr2 create line  $xys
 }

 puts "Call testplotc to test the plot canvas"
 set entrypts {}
 lappend entrypts {testplotc "Call testplotc to test the plot canvas"}
 catch {destroy .testplotcanvas}
 set fex [frame .testplotcanvas]
 foreach ep $entrypts { ;# create a button to launch an example procedure
        set choice [lindex $ep 0]
        button $fex.$choice -text "[lindex $ep 1]" -command [lindex $ep 0]
        pack $fex.$choice -side left
 }
pack $fex -side top
