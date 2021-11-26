# The purpose of this script is to calculate the code coverage for a ARTS module test
#
# Please call the script as follow:
# code_cov.pl <summary_file> [<CodeCovLimit>]
#
# The default value for the CodeCovLimit is 80% but you can overwrite it by setting
# the CodeCovLimit parameter.
#
# Following rules are implemented for the function coverage
# - a function is counted as executed as soon as one line from this function was executed
#
# Following rules are implemented for the interface coverage
# - for the interface coverage header files from ../mhw_drv_inc/inc/<subcomponent>/..
#   are taken and analyzed with ctags to get the function names
#   (ToDo take also the interface functions from /uta_inc/<subcomponent>)
#
# New Logfile:
# <Fun|Fuc|Ifn|Ifc> function name
# Fun - Function wasn't covered (0% code coverage)
# Fuc - Function is covered
# Ifn - Interface function wasn't covered (0% code coverage)
# Ifc - Interface function is covered
#
# Remark:
# The interface coverage is commented out (marked with #IF). So there is no check made for
# interface functions. These functions are counted as normal functions in the log
# file (Fun|Fuc).

use warnings;
use strict;

my $DEBUG = 0;
$ENV{'ARTS_TEST_ROOT'} = '/home/wetzella/sandbox/backbone/make' if ($DEBUG);

#$ENV{'ARTS_TEST_ROOT'} =  'C:\UserData\wetzella\Sandbox\backbone\make'; #Used for debug

exit 0 unless ( defined $ARGV[0] );

# Default values for code coverage and function coverage
my $COV_LIMIT  = 80;
my $FUNC_LIMIT = 100;

# Override the default values if a new value is given
$COV_LIMIT  = $ARGV[1] if ( defined $ARGV[1] );
$FUNC_LIMIT = $ARGV[2] if ( defined $ARGV[2] );

# Opening the file for input
open( FILE, $ARGV[0] )
  || die "Not able to open inputfile $ARGV[0]\n";

#
# some preperation like
# - reading and writing subcomponent
# - reading and writing modules
# - fetching the interface header files
my $line = <FILE>;

my @sub = split( ':', $line );
my $subcomponent = $sub[1];    # Finding the subcomponent name
chomp($subcomponent);
$subcomponent = lc $subcomponent;

$line = <FILE>;    # Finding the module names

my %values = (
 "total_lines" => 0,
 "exec_lines"  => 0,
 "total_func"  => 0,
 "exec_func"   => 0,
 "files"   => 0
);

my @modules = split( ':', $line );
my %modules = ();
my $module;
foreach $module (@modules) {
 chomp $module;
 next if ( $module =~ /^\s+$/ );      # clean up
 next if ( $module =~ /^Module$/ );
 foreach ( keys(%values) ) {
  # Init files as an empty string
  if($_ ne "files" )
  {
     $modules{$module}{$_} = 0;
  }
  else
  {
     $modules{$module}{$_} = "";
  }
 }
}
@modules = keys(%modules);

# Opening the log file
my $logfile = $ARGV[0] . "_codecovega.log";
open( LOG_FILE, ">" . $logfile )
  || die "Not able to open logfile " . $logfile . "\n";

# Look forward for filesystem information
#IFmy @interface_files;
#IFmy $file;
#IFwhile (<FILE>) {
#IF    next unless (/File\s+\`(.*)\'$/);
#IF    $file = $1;
#IF    # Get subcomponent from the file statement
#IF    $file =~ /mhw_drv_src[\/\\](\w+)[\/\\]/;
#IF    $subcomponent = $1;
#IF    # Change the directory
#IF    $file =~ s/mhw_drv_src/mhw_drv_inc\/inc/; # we want the include
#IF    $file =~ s/[\/\\]\w+\.\w+$//;        # throw away filename
#IF    # Reduce directory - no src subdirectory
#IF    $file =~ s/$subcomponent[\/\\]\w+[\/\\]src/$subcomponent/;
#IF
#IF    if ($DEBUG) {
#IF        print "Original dir before DEBUG changes: $file\n";
#IF	$file =~ s/.*[\/\\]mhw_drv_inc\/inc/$ENV{'ARTS_TEST_ROOT'}\/\.\.\/\.\./;
#IF    }
#IF
#IF    print LOG_FILE "Examine directory $file:\n";
#IF    opendir DIR, $file;
#IF    push @interface_files, grep !/^\./, readdir DIR;
#IF    @interface_files = map "$file/$_", @interface_files;
#IF
#IF    closedir DIR;
#IF
#IF    last; # We only need one File line for the directory information
#IF}
#IFseek FILE,0,0;
#IF
#IF# Read the interface functions
#IF#
#IFmy %if_functions;
#IFmy $tagfile = "codcovtags.txt";
#IFmy @tagfields;
#IF
#IFunlink $tagfile;
#IF
#IFforeach $file (@interface_files) {
#IF    `ctags -o $tagfile --declarations -a $file &2>tag.err`;
#IF}
#IF
#IFopen TAGFILE, $tagfile
#IF   || die "Couldn't open $tagfile";
#IF
#IFwhile ($line = <TAGFILE>) {
#IF    @tagfields = split /\s+/, $line;
#IF    print LOG_FILE "-- IF Function $tagfields[0] in file $tagfields[1]\n";
#IF    $if_functions{ $tagfields[0] } = 1;
#IF}
#IF
#IFclose TAGFILE;

#
# Analyse the gcov output
#
my $filename = $ARGV[0] . "_codecovega.txt";
open( RESULT_FILE, ">$filename" )
  || die "Not able to open outputfile $filename\n"; #Opening the file for output

print RESULT_FILE "Subcomponent name: ", $subcomponent, "\n";
print RESULT_FILE "Module names: ", "@modules", "\n";

#IFprint RESULT_FILE "------------------Interface Files-------------------\n";
#IFmap { print RESULT_FILE $_."\n" } @interface_files;

my $code_coverage      = 0;
my $function_coverage  = 0;
my $total_functions    = 0;
my $executed_functions = 0;
my $act_module;
my $nextline;
my $word = 0;
my $flag = 0;
my $function;
my @files;
my %quality_if;

print RESULT_FILE "\n------------------Module Coverage-------------------\n";

# What is the total line coverage of the module?
# Reading the input values and sort them to the modules
while ( $line = <FILE> ) {
 if ( $line =~ /Function\s+[\`\'](\w+)\'/ ) {
  $function = $1;
  $total_functions++;

  # Check the next line, if function was called or not
  $nextline = <FILE>;
  if ( $nextline =~ /Lines executed:(\d+.\d+)% of (\d+)/ ) {
   if ( $1 > 0 ) {
    $executed_functions++;

    #IF        if ( defined $if_functions{ $function } ) {
    #IF	  $if_functions{ $function } = 0;
    #IF          $quality_if{ $function } = 1;
    #IF          print LOG_FILE "Ifc ".$function."\n";
    #IF        } else {
    print LOG_FILE "Fuc " . $function . "\n";

    #IF      }
   }
   else {

    #IF         if ( defined $if_functions{ $function } ) {
    #IF	   $quality_if{ $function } = 1;
    #IF           print LOG_FILE "Ifn ".$function."\n";
    #IF        } else {
    print LOG_FILE "Fun " . $function . "\n";

    #IF	}
   }
  }

  # Source file
  # match for entries mhw_drv_src
 }
 elsif ( $line =~ /File\s*.+\/(\w+)\/src\/.+\.[c|cpp|cc]\'$/ ) {
  $act_module = $1;
  push @files, $line;
  $flag = 1;    # raise flag

  # match for UTA
 }
 elsif ( $line =~ /^File\s*.+\/$subcomponent\/(\w+)\/.+\.[c|cpp|cc]\'/ ) {
  $act_module = $1;
  push @files, $line;
  $flag = 1;    # raise flag
 }

 # Check that the previous line matched
 if ( ( $line =~ /Lines executed:(\d+.\d+)% of (\d+)/ ) && ($flag) ) {

  # Now we can store the values
  $modules{$act_module}{"total_lines"} += $2;
  $modules{$act_module}{"exec_lines"}  += $1 / 100 * $2;
  $modules{$act_module}{"total_func"}  += $total_functions;
  $modules{$act_module}{"exec_func"}   += $executed_functions;
  if($modules{$act_module}{"files"})
  {
  $modules{$act_module}{"files"}   =  $modules{$act_module}{"files"} . $files[$#files];
  }
  else
  {
  $modules{$act_module}{"files"}   =  $files[$#files];
  }

  $total_functions    = 0;
  $executed_functions = 0;
  $flag               = 0;    # reset flag
 }
}

#
# Ouput of the Reading values
#
my $all_lines      = 0;
my $all_exec_lines = 0;
my $all_functions  = 0;
my $all_exec_func  = 0;

# Set the thresholds as mandatory as default .
my $man_adj_string = ";ge;adjust;";
my $man_no_string  = ";ge;adjust;mandatory";

my $opt_string = ";ge;adjust;optional";

foreach $module (@modules) {
 if ( $modules{$module}{"total_lines"} > 0 ) {
  $code_coverage =
    $modules{$module}{"exec_lines"} / $modules{$module}{"total_lines"} * 100;
  if ( $modules{$module}{"total_func"} > 0 ) {
   $function_coverage =
     $modules{$module}{"exec_func"} / $modules{$module}{"total_func"} * 100;
  }
  else {
   $function_coverage = $code_coverage;
  }

  # Print files used to measure coverage for this module
  print RESULT_FILE $module, " - files:\n";
  foreach ($modules{$module}{"files"}) {
   print RESULT_FILE $_ ;
  }

  # print coverage information block in summary file
  print RESULT_FILE $module, " - coverage:\n";
  print RESULT_FILE "Total number of lines in ", $module, " module: ",
    $modules{$module}{"total_lines"}, "\n";
  printf RESULT_FILE
    "Total approx. number of executed lines of code:%5.0f\n",
    $modules{$module}{"exec_lines"};

  printf RESULT_FILE "Code coverage:%3.0f%%\n", $code_coverage;
  print RESULT_FILE "Total number of functions in ", $module, " module: ",
    $modules{$module}{"total_func"}, "\n";
  printf RESULT_FILE
    "Total number of executed functions (> 0%% executed lines):%5.0f\n",
    $modules{$module}{"exec_func"};
  printf RESULT_FILE "Function coverage:%3.0f%%\n\n", $function_coverage;

  # print information for ARTS on stdout
  # both code coverage values (mand./fix and optional value)
  if ( $code_coverage >= $COV_LIMIT ) {

   # printing out key for fix value COV_LIMIT
   printf "%s_code_coverage=%0.0f%s\n", $module, $COV_LIMIT, $man_no_string;
  }
  else {
   printf "%s_code_coverage=%0.0f%s\n", $module, $code_coverage,
     $man_adj_string;
  }

  # printing out key for opt. real val
  printf "%s_code_cov_val=%0.0f%s\n", $module, $code_coverage, $opt_string;

  # now the same for the function coverage
  # remark: a fix limit is acitvated for the function coverage at the moment
  if ( $function_coverage >= $FUNC_LIMIT ) {

   # printing out key for fix_value - it's not activated
   printf "%s_function_coverage=%0.0f%s\n", $module,
     $FUNC_LIMIT, $man_no_string;
  }
  else {
   printf "%s_function_coverage=%0.0f%s\n", $module,
     $function_coverage, $man_adj_string;
  }

  # printing out key for opt. real val
  printf "%s_function_cov_val=%0.0f%s\n\n", $module, $function_coverage,
    $opt_string;

  $all_lines      += $modules{$module}{"total_lines"};
  $all_exec_lines += $modules{$module}{"exec_lines"};
  $all_functions  += $modules{$module}{"total_func"};
  $all_exec_func  += $modules{$module}{"exec_func"};

 }
 else {    # Only for debugging purposes

  #    print "No output for module: ".$module."!\n";
 }
}

# finally print the overall values in summary file
print RESULT_FILE "\n";
print RESULT_FILE "---------------Subcomponent Coverage----------------\n";

# Code coverage
$code_coverage = $all_exec_lines / $all_lines * 100 if ($all_lines);
print RESULT_FILE "Total number of lines in subcomponent: $all_lines\n";
printf RESULT_FILE "Total approx. number of executed lines of code:%5.0f\n",
  $all_exec_lines;
printf RESULT_FILE "Code coverage:%3.0f%%\n\n", $code_coverage;

# Function coverage
$function_coverage = $all_exec_func / $all_functions * 100
  if ($function_coverage);
print RESULT_FILE "Total number of functions in subcomponent: $all_functions\n";
printf RESULT_FILE
  "Total number of executed functions (> 0%% executed lines):%5.0f\n",
  $all_exec_func;
printf RESULT_FILE "Functions coverage:%3.0f%%\n\n", $function_coverage;


print RESULT_FILE "\n";
print RESULT_FILE "---------------Files listed in input coverage summery file------\n";
  print RESULT_FILE "Note: Not all of them are necessarily used in the coverage calculation.\n";
  # print all files available in the summary file
  foreach (@files) {
   print RESULT_FILE $_ ;
  }


#IF# Interface functions
#IFmy $allif = keys( %if_functions );
#IFmy $exeif = 0;
#IFmy $covif = 0;
#IFforeach $function ( keys( %if_functions ) ) {
#IF  if ( $if_functions{ $function } ) {
#IF    print LOG_FILE $function." is not covered by coverage measurement.\n" unless (defined $quality_if{ $function });
#IF  } else {
#IF    $exeif++;
#IF  }
#IF}
#IF$covif = $exeif / $allif * 100 if ($allif > 0);
#IFprint RESULT_FILE "Total number of interface functions: ".$allif."\n";
#IFprintf RESULT_FILE "Executed interface functions: %5.0f\n",$exeif ;
#IFprintf RESULT_FILE "Functions coverage:%3.0f%%\n\n", $covif;

# and the rest for ARTS on stdout
use File::Spec;

my @filesToAttach =
  substr( File::Spec->rel2abs($filename), length( $ENV{ARTS_TEST_ROOT} ) + 1 ) if ($ENV{ARTS_TEST_ROOT});
$filesToAttach[1] =
  substr( File::Spec->rel2abs($logfile), length( $ENV{ARTS_TEST_ROOT} ) + 1 ) if ($ENV{ARTS_TEST_ROOT});
$filesToAttach[2] =
  substr( File::Spec->rel2abs( $ARGV[0] ), length( $ENV{ARTS_TEST_ROOT} ) + 1 ) if ($ENV{ARTS_TEST_ROOT});

print "!attach=" . join( ';', @filesToAttach ) . "\n"  if ($ENV{ARTS_TEST_ROOT});

close(FILE)        || die "Can't close input file\n";
close(RESULT_FILE) || die "Can't close output file\n";
close(LOG_FILE)    || die "Can't close log file\n";

print "_postexec=DONE\n";

