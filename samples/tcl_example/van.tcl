#!/usr/bin/wish

#vrf simulation

package require Tk

global schalter_n
global schalter_s
global schalter_s_text

global v
global e
global u
global i
global anzeige_text

set schalter_n 1
set schalter_s 0
set schalter_s_text "Ein"

set v 0
set e 10
set u 0
set i 0
set t 0

proc anzeige_update {} {
    global anzeige_text
    global v
    global e
    global u
    global i
    
    set anzeige_text "\n\
    Kapazität\[Ws]: $e\n\
    Verbrauch\[Ws]: $v\n\
    Spanung\[V]: $u\n\
    Strom\[A]: $i\n"

    return $anzeige_text
}

proc toggle_schalter_s {} {
    global schalter_n
    global schalter_s
    global schalter_s_text
    set schalter_s [expr {$schalter_s?0:1}]
    if {$schalter_s} {
	if {$schalter_n} {
	    set schalter_s_text "Aus"
	} else {
	    set schalter_s_text "Exit"	    
	}
    } else {
	set schalter_s_text "Ein"
    }
    puts "Taste EinAus: $schalter_s"
}

set anzeige_text [anzeige_update]

label .lb -textvariable anzeige_text -width 40
pack [button .b -textvariable schalter_s_text -width 10 -command {toggle_schalter_s}]
pack .lb -side bottom


proc van_init {} {
    global schalter_n
    global schalter_s
    global schalter_s_text

    global anzeige_text
    
    global v
    global e
    global u
    global i
    
    set schalter_n 1
    set schalter_s 0
    set schalter_s_text "Ein"
    
    set v 0
    set e 10
    set u 1
    set i 0
    
    set anzeige_text [anzeige_update]

    set t 0    
    return
}

proc van_simulate {} {
    global v
    global e

    if {$v < $e} {
	incr v
	return 1
    }
    return 0;
}

proc van_control {} {
    global schalter_n
    global schalter_s
    global schalter_s_text

    global anzeige_text
    
    global v
    global e
    global u
    global i
    
    van_init

    while 1 {
	update
	
	incr t
	
	if {$u == 0} {
	    set schalter_n 0
	    
	    if {$schalter_s == 1} {
		set schalter_s_text "Exit"
	    }
	}

	set i 0
	if {$schalter_n == 1} {
	    if {$schalter_s == 1} {
		set u [van_simulate]
		set i $u
	    }
	}
	
	puts "\
	Zeit\[seconds]: $t \
	Verbrauch\[Ws]: $v \
	Schalter Ein: $schalter_s \
	Schalter NotAus: $schalter_n"

	if {$schalter_n == 0} {
	    if {$schalter_s == 0} {
		break
	    }
	}
	anzeige_update
	after 1000
    }
    puts "Aus"
    exit
}

van_control
