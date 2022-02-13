# tutorial_code.tcl --
#     Procedures to create an interactive tutorial or a man page
#     in doctools format
#

# manpageOnly --
#     Text to be written to the man page file only
#
# Arguments:
#     text        Text to be written
#
# Result:
#     None
#
# Side effects:
#     The text is copied verbatim
#
proc manpageOnly {text} {
    global outfile
    global mode

    if { $mode == "manpage" } {
        puts $outfile $text
    }
}

# plain --
#     Text to be written in plain form to the text widget or to the file
#
# Arguments:
#     text        Text to be written
#
# Result:
#     None
#
# Side effects:
#     The text is copied verbatim or displayed in the text widget
#
proc plain {text} {
    global outfile
    global mode

    if { $mode == "manpage" } {
        puts $outfile $text
    } else {
        .textw.t insert end [string map {\n\n \n\n \n " "} $text] plain
    }
}

# title --
#     Text to be written as a title
#
# Arguments:
#     text        Text to be written
#
# Result:
#     None
#
# Side effects:
#     The text is copied to the file or the text widget
#
proc title {text} {
    global outfile
    global mode

    if { $mode == "manpage" } {
        puts $outfile "\[section \{$text\}\]"
    } else {
        .textw.t insert end $text title
    }
}

# fixed --
#     Text to be written in non-proportional font
#
# Arguments:
#     text        Text to be written
#
# Result:
#     None
#
# Side effects:
#     The text is copied as an example or displayed in Courier
#
proc fixed {text} {
    global outfile
    global mode

    if { $mode == "manpage" } {
        puts $outfile "\[example \{$text\n\}\]"
    } else {
        .textw.t insert end $text fixed
    }
}

# bullets --
#     Text to be written in the form of a bullet list
#
# Arguments:
#     text        Text to be written
#
# Result:
#     None
#
# Side effects:
#     The text is copied to the output file or displayed in the text
#     widget with the correct formatting for a bullet list
#
proc bullets {text} {
    global outfile
    global mode

    if { $mode == "manpage" } {
        puts $outfile $text
    } else {
        .textw.t insert end $text bullets
    }
}

# hiddenCode --
#     Code to be run once, not visible in the text widget or otherwise
#
# Arguments:
#     code        Code to be run immediately
#
# Result:
#     None
#
# Side effects:
#     The code is run in the global namespace
#
proc hiddenCode {code} {

    uplevel #0 $code

}

# runnableCode --
#     Code to be written to the output file as an example or shown in a
#     embedded text widget. Can be edited and run by pressing the
#     associated push button
#
# Arguments:
#     code        Code to be written or run
#
# Result:
#     None
#
# Side effects:
#     The code is copied to the output file or displayed in an
#     embedded text widget. A push button is added to run the code
#
proc runnableCode {code} {
    global outfile
    global count
    global mode

    if { $mode == "manpage" } {
        puts $outfile "\example \{$text\n\}\]"
    } else {
        incr count
        set frame [frame  .textw.t.frame$count]
        set tcode [text   $frame.t -background lightblue \
                       -wrap none \
                       -xscrollcommand [list $frame.xscroll set] \
                       -yscrollcommand [list $frame.yscroll set]]
        set brun  [button .textw.b$count -text "Run" -command $code -font "Helvetica, 12"]

        set xbar [scrollbar $frame.xscroll -orient horizontal -command [list $tcode xview]]
        set ybar [scrollbar $frame.yscroll -orient vertical   -command [list $tcode yview]]
        grid $tcode  $ybar -sticky news
        grid $xbar   -     -sticky news
        grid columnconfigure $frame 0 -weight 1
        grid columnconfigure $frame 1 -weight 0
        grid rowconfigure    $frame 0 -weight 1
        grid rowconfigure    $frame 1 -weight 0

        .textw.t window create end -window $frame
        .textw.t insert end \n
        .textw.t window create end -window $brun

        $tcode tag configure code -font "Courier 12"
        $tcode insert end $code code
    }
}

# preparation --
#     Create the text widget, open the file and set a few global parameters
#
set count 0
set mode  ""
if { $mode == "manpage" } {

    # TODO

} else {
    toplevel  .textw
    text      .textw.t -yscrollcommand {.textw.yscroll set} -wrap word
    scrollbar .textw.yscroll -orient vertical -command {.textw.t yview}
    grid      .textw.t .textw.yscroll -sticky news
    grid columnconfigure .textw 0 -weight 1
    grid columnconfigure .textw 1 -weight 0
    grid rowconfigure    .textw 0 -weight 1

    .textw.t tag configure plain -font "Times 12"
    .textw.t tag configure fixed -font "Courier 12"
    .textw.t tag configure bullets -font "Times 12"
    .textw.t tag configure title -font "Helvetica 14 bold"


    # TODO: wm protocol WM_DELETE ...
}
