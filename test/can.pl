#! /usr/bin/perl

use strict;

my (%tested, %untested, %other,%fun_d, %fun_c);               # Summary per file
my ($f_tested, $f_untested, $f_other,$f_fun_d,$f_fun_c);      # Functions

my $f_cnt;
my $f;

sub do_perc {
    my $val1 = shift;
    my $val2 = shift;
    if ($val2 == 0) {
	$val2 = 1;
    }
    return int(($val1/$val2*100)+0.5);
}
sub do_summary {
    my $f = shift;
    $tested{$f}   = 0;
    $untested{$f} = 0;;
    $other{$f}    = 0;;
    $fun_d{$f}    = 0;;
    $fun_c{$f}    = 0;;
    open(F, "$f") || die "Cannot open $f \n";
    while(<F>) {
        if(/^  *-: /) {
            $other{$f} ++;
        } elsif (/^  +#+: /) {
            $untested{$f} ++;
        } elsif (/^  +[1-9][0-9]*: /) {
            $tested{$f} ++;
        } elsif (/^function /) {
	    my @l;
            @l= split;
	    $fun_d{$f}++ ;
	    if ($l[3] > 0) { # called at least one
	        $fun_c{$f}++ ;
	    } else {
		$f_cnt++;
		print "$f_cnt: $f: $l[1] not called\n";
	    }
        } else {
	    # comments
        }
    }
    close(F);
}
foreach $f (@ARGV) {
    do_summary($f);
}
print "\nFile;used; not used; comments;percentage;function defined;function called;percentage\n";

foreach $f (sort(keys(%other))) {
    my @l=split(/\//,$f);

    $f_tested   += $tested{$f};
    $f_untested += $untested{$f};
    $f_other    += $other{$f};
    $f_fun_d    += $fun_d{$f};
    $f_fun_c    += $fun_c{$f};

    print "$l[$#l];$tested{$f};$untested{$f};$other{$f};";
    
    if ($tested{$f} > 0)  {
        print  '' . do_perc($tested{$f},($untested{$f}+$tested{$f}));
    } else {
        print 0;
    }
    print "; $fun_d{$f};$fun_c{$f}";
    if ( $fun_c{$f} > 0 ) {
	print ';' . do_perc(int($fun_c{$f}) , int($fun_d{$f}) );
    } else {
        print ";0";
    }
    print "\n";
}
if ( $f_tested == 0 ) {$f_tested = 1;}
if ( $f_fun_d == 0 )  {$f_fun_d = 1;}

print "van;$f_tested;$f_untested;$f_other;" . do_perc($f_tested,($f_untested+$f_tested)) ;
print "; $f_fun_d;$f_fun_c;" . do_perc(int($f_fun_c) , int($f_fun_d) ) . "\n";

$f_cnt = $f_fun_d - $f_fun_c;
print "\nSummary:\nexec(s)=$f_tested; incative(s)=$f_untested; ratio(exec(s))=" . do_perc($f_tested,($f_untested+$f_tested)) ;
print "%;\nexec(f)=$f_fun_c; inactive(f)=$f_cnt; ratio(exec(f))=" . do_perc(int($f_fun_c) , int($f_fun_d) ) . "%\n";

exit 0;
