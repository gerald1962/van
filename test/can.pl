#! /usr/bin/perl

use strict;

# Summary per file
my (%exec_s, %inactive_s, %blank, %n_func, %exec_func);

# Total values
my ($t_exec_s, $t_inactive_s, $t_blank, $t_n_func, $t_exec_func);

my $t_cnt;
my $f;

# Calculaate the percent value.
sub percent {
    my $min = shift;
    my $max = shift;
    
    if ($min == 0) {
	$max = 1;
    }
    
    return int(($min / $max * 100) + 0.5);
}

# Analyze a coverage file line by line.
sub file_analyze {
    my $f = shift;
    
    $exec_s{$f}     = 0;
    $inactive_s{$f} = 0;
    $blank{$f}      = 0;
    $n_func{$f}     = 0;
    $exec_func{$f}  = 0;

    # Open the coverage file.
    open(F, "$f") || die "Cannot open $f\n";

    # Read line by line.
    while(<F>) {
        if(/^  *-: /) {
	    # Blank lines or comments.
            $blank{$f}++;
	    
        } elsif (/^  +#+: /) {
	    # Not executed statement.
            $inactive_s{$f}++;

        } elsif (/^  +[1-9][0-9]*: /) {
	    # Executed statement.
            $exec_s{$f}++;
	    
        } elsif (/^function /) {
	    my @line;
	    
            @line = split;
	    $n_func{$f}++;

	    if ($line[3] > 0) {
		# Executed function.
	        $exec_func{$f}++;
		
	    } else {
		# Not executed function.
		$t_cnt++;
		print "$t_cnt: $f: $line[1] not called\n";
	    }
        } else {
	    # comments
        }
    }

    # Close the coverage file.
    close(F);
}

# Analyze all coverage files.
foreach $f (@ARGV) {
    # Analyze a coverage file line by line.
    file_analyze ($f);
}

print "\n *s; -s; p; *f; #f; p; file\n";
print "----------------------------\n";

# Loop over all file names.
foreach $f (sort (keys(%blank))) {
    my @l = split(/\//, $f);

    $t_exec_s     += $exec_s{$f};
    $t_inactive_s += $inactive_s{$f};
    $t_blank      += $blank{$f};
    $t_n_func     += $n_func{$f};
    $t_exec_func  += $exec_func{$f};

    # Print the values for a single file.
    print " $exec_s{$f}; $inactive_s{$f}";
    
    if ($exec_s{$f} > 0)  {
        print  '; ' . percent($exec_s{$f}, ($inactive_s{$f} + $exec_s{$f}));
    } else {
        print "; 0";
    }
    
    print "; $exec_func{$f}; $n_func{$f}";
    
    if ( $exec_func{$f} > 0 ) {
	print '; ' . percent(int($exec_func{$f}) , int($n_func{$f}) ) . "; $l[$#l]"
    } else {
        print "; 0; $l[$#l]";
    }
    print "\n";
}

if ($t_exec_s == 0) {
    $t_exec_s = 1;
}

if ($t_n_func == 0) {
    $t_n_func = 1;
}

print "---------------------------\n";

# Print the total values of the coverage analysis.
print " $t_exec_s; $t_inactive_s; " .
    percent($t_exec_s, ($t_inactive_s + $t_exec_s));

print "; $t_exec_func; $t_n_func; " .
    percent(int($t_exec_func), int($t_n_func) ) . "; van\n";

print "---------------------------\n";
print "*s; -s; p; *f; #f; p; system\n\n";

exit 0;
