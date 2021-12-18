#!/bin/bash

# site_test - test the execution status of site. If site does not work as
# expected, stop the site test cycle and return the error code 1 to the control
# terminal.
#
# $1:  is the same as the return value of site saved in the special bash
# variable $?.
#
# Return:        None.
site_test () {
    if [ $1 -eq 0 ]; 
    then 
	echo "*** executed successfully:" $2
    else 
	echo "*** terminated unsuccessfully: " $2
	exit 1
    fi
}

# main - start function of the simultaneous data transfer experiments.
#
# $1:  number of the test cycles.
#
# Return:        the exit code of site.
main () {
    # Save the final condition of the site test..
    limit=$1

    # Initialize the loop counter.
    i=0

    # Repeat the test cases as requested.
    while [  $i -lt $limit ]; do

	let i=i+1

	# Execute the simultaneous test experiments.
    
	# 1. Corner case: transfer buffer with lower limit, siehe site -h.
	out/site    -d 1111 -l 1 -f x -u 1111 -s 1 -r y
	site_test $? $i
	out/site -z -d 1111 -l 1 -f x -u 1111 -s 1 -r y    
	site_test $? $i
    
	# 2. Corner case: transfer buffer with upper limit, siehe site -h.
	out/site    -d 1111 -l 2048 -f x -u 1111 -s 2048 -r y
	site_test $? $i
	out/site -z -d 1111 -l 2048 -f x -u 1111 -s 2048 -r y
	site_test $? $i
    
	# 3. Corner case: linking of the 1. and 2. corner case.
	out/site    -d 1111 -l 1 -f x -u 1111 -s 2048 -r y
	site_test $? $i
	out/site -z -d 1111 -l 2048 -f x -u 1111 -s 1 -r y
	site_test $? $i
    
	# 4. Corner case: simulate the battery controller.
	out/site    -d 1111 -l 10 -f r -u 1111 -s 30 -r c
	site_test $? $i
	out/site -z -d 1111 -l 10 -f r -u 1111 -s 30 -r c
	site_test $? $i
    done
    
    exit 0
}

# main - call the start function of the simultaneous data transfer experiments.
#
# Return:        the exit code of site.
main $1
