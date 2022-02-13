#!/usr/bin/tclsh

package require Tk

# pl_tutorial.tcl --
#     Interactive tutorial for Plotchart
#
source "pl_tutorial_code.tcl"

manpageOnly {
[comment {-*- tcl -*- doctools manpage}]
[manpage_begin Plotchart n 1.5]
[copyright {2008 Arjen Markus <arjenmarkus@users.sourceforge.net>}]
[moddesc   Plotchart]
[titledesc {Tutorial to the Plotchart package}]
}

title {
Tutorial for the Plotchart package
}

plain {
Plotchart is Tcl-only package to create xy-plots, barcharts, piecharts
and many other types of graphical presentations. This tutorial is
intended to help you get started using it. It does not describe every
feature of Plotchart, you should consult the manual for that.

Rather, this tutorial will show:
}

bullets {
* How to use Plotchart to create several of the plots and charts
mentioned above
* How to customise various aspects
* Some of the gory details
}

plain {
Throughout this tutorial we will use a fixed set of data:
}

fixed {
chart       2008-05-18      17:25   1440    1081    27      94
axis        2008-05-18      17:26   940     712     20      86
pack        2008-02-24      08:22   378     258     7       37
priv        2008-05-18      17:18   1837    1238    40      167
annot       2008-02-22      13:03   376     275     7       22
config      2008-02-22      11:25   162     123     2       12
scaling     2008-02-22      10:23   148     109     4       15
plot3d      2007-09-07      11:47   300     203     5       34
contour     2005-04-15      13:42   1697    1131    19      151
gantt       2005-06-15      11:12   333     209     7       25
business    2007-09-07      12:15   382     246     7       30
}
hiddenCode {
set data {
{chart       2008-05-18      17:25   1440    1081    27      94}
{axis        2008-05-18      17:26   940     712     20      86}
{pack        2008-02-24      08:22   378     258     7       37}
{priv        2008-05-18      17:18   1837    1238    40      167}
{annot       2008-02-22      13:03   376     275     7       22}
{config      2008-02-22      11:25   162     123     2       12}
{scaling     2008-02-22      10:23   148     109     4       15}
{plot3d      2007-09-07      11:47   300     203     5       34}
{contour     2005-04-15      13:42   1697    1131    19      151}
{gantt       2005-06-15      11:12   333     209     7       25}
{business    2007-09-07      12:15   382     246     7       30}
}
}

plain {
These are simple software metrics from the Plotchart source files:
}

bullets {
* The abbreviated name of the file, date and time of the last modification
* Total number of lines and number of lines without comments
* Number of procedures
* Number of if and switch statements, for, foreach and while loops
}

plain {
Rather than use fictitious data, it seemed nice to use these data,
related in some way to Plotchart itself.

Now, let us plot the number of lines versus the number of procedures:
}

runnableCode {
    package require Plotchart

    #
    # Create the canvas that will hold the plot
    #
    canvas .c -width 400 -height 300 -background white
    pack .c -fill both

    #
    # Create the plot
    #
    set p [::Plotchart::createXYPlot .c {0 40 10} {0 1500 250}]

    #
    #
    # Get the data (via the convenience procedure "column") and plot the
    # data points
    #
    # ...
} 
