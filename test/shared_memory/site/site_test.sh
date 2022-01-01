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

	# 1. Van-python cable.
	
	# 1.1. Execute the simultaneous test experiments with the blocking sync.
	#      interfaces.
    
	# 1.1.1. Corner case: transfer buffer with lower limit, siehe site -h.
	out/site       -c p -d 111 -l 1 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v bz -c p -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
    
	# 1.1.2. Corner case: transfer buffer with upper limit, siehe site -h.
	out/site       -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v bz -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
    
	# 1.1.3. Corner case: linking of the 1. and 2. corner case.
	out/site       -c p -d 111 -l 1 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v bz -c p -d 111 -l 2048 -f x -u 111 -s 1 -r y
	site_test $? $i
    
	# 1.1.4. Corner case: simulate the battery controller.
	out/site       -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v bz -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	
	# 1.2. Execute the simultaneous test experiments with the async. interfaces.
    
	# 1.2.1. Corner case: transfer buffer with lower limit, siehe site -h.
	out/site -n a      -c p -d 111 -l 1 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v a      -c p -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
	out/site -v a -n a -c p -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
    
	# 1.2.2. Corner case: transfer buffer with upper limit, siehe site -h.
	out/site -n a      -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v a      -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v a -n a -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
    
	# 1.2.3. Corner case: linking of the 1. and 2. corner case.
	out/site -n a      -c p -d 111 -l 1 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v a      -c p -d 111 -l 2048 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v a -n a -c p -d 111 -l 1 -f x -u 111 -s 2048 -r y
	site_test $? $i
    
	# 1.2.4. Corner case: simulate the battery controller.
	out/site -n a      -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v a      -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v a -n a -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i

	# 1.3. Execute the simultaneous test experiments with the non blocking
	#      sync. interfaces.
    
	# 1.3.1. Corner case: transfer buffer with lower limit, siehe site -h.
	out/site -n nc       -c p -d 111 -l 1 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v nc       -c p -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
	out/site -v a -n nc  -c p -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
    
	# 1.3.2. Corner case: transfer buffer with upper limit, siehe site -h.
	out/site -v nc       -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v bz -n nc -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v nc -n a  -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
    
	# 1.3.3. Corner case: linking of the 1. and 2. corner case.
	out/site -n nc       -c p -d 111 -l 1 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v bz -n nz -c p -d 111 -l 2048 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v a  -n nc -c p -d 111 -l 2048 -f x -u 111 -s 1 -r y
	site_test $? $i
    
	# 1.3.4. Corner case: simulate the battery controller.
	out/site -v nc -n nc -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v bz -n bc -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v nc -n a  -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	
	# 2. Van-tcl cable.
	
	# 2.1. Execute the simultaneous test experiments with blocking the sync.
	#      interfaces.
    
	# 2.1.1. Corner case: transfer buffer with lower limit, siehe site -h.
	out/site       -c t -d 111 -l 1 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v bz -c t -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
    
	# 2.1.2. Corner case: transfer buffer with upper limit, siehe site -h.
	out/site       -c t -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v bz -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
    
	# 2.1.3. Corner case: linking of the 1. and 2. corner case.
	out/site       -c t -d 111 -l 1 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v bz -c t -d 111 -l 2048 -f x -u 111 -s 1 -r y
	site_test $? $i
    
	# 2.1.4. Corner case: simulate the battery controller.
	out/site       -c t -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v bz -c t -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	
	# 2.2. Execute the simultaneous test experiments with the async. interfaces.
    
	# 2.2.1. Corner case: transfer buffer with lower limit, siehe site -h.
	out/site -n a      -c t -d 111 -l 1 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v a      -c t -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
	out/site -v a -n a -c t -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
    
	# 2.2.2. Corner case: transfer buffer with upper limit, siehe site -h.
	out/site -n a      -c t -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v a      -c t -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v a -n a -c t -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
    
	# 2.2.3. Corner case: linking of the 1. and 2. corner case.
	out/site -n a      -c t -d 111 -l 1 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v a      -c t -d 111 -l 2048 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v a -n a -c t -d 111 -l 1 -f x -u 111 -s 2048 -r y
	site_test $? $i
    
	# 2.2.4. Corner case: simulate the battery controller.
	out/site -n a      -c t -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v a      -c t -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v a -n a -c t -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i

	# 2.3. Execute the simultaneous test experiments with the non blocking
	#      sync. interfaces.
    
	# 2.3.1. Corner case: transfer buffer with lower limit, siehe site -h.
	out/site -n nc       -c p -d 111 -l 1 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v nc       -c p -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
	out/site -v a -n nc  -c p -d 111 -l 1 -f x -u 111 -s 1 -r y    
	site_test $? $i
    
	# 2.3.2. Corner case: transfer buffer with upper limit, siehe site -h.
	out/site -v nc       -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v bz -n nc -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v nc -n a  -c p -d 111 -l 2048 -f x -u 111 -s 2048 -r y
	site_test $? $i
    
	# 2.3.3. Corner case: linking of the 1. and 2. corner case.
	out/site -n nc       -c p -d 111 -l 1 -f x -u 111 -s 2048 -r y
	site_test $? $i
	out/site -v bz -n nz -c p -d 111 -l 2048 -f x -u 111 -s 1 -r y
	site_test $? $i
	out/site -v a  -n nc -c p -d 111 -l 2048 -f x -u 111 -s 1 -r y
	site_test $? $i
    
	# 2.3.4. Corner case: simulate the battery controller.
	out/site -v nc -n nc -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v bz -n bc -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
	out/site -v nc -n a  -c p -d 111 -l 10 -f r -u 111 -s 30 -r c
	site_test $? $i
    done
    
    exit 0
}

# main - call the start function of the simultaneous data transfer experiments.
#
# Return:        the exit code of site.
main $1
