#!/bin/bash

# Save the script argument.
limit=$1

# Initialize the loop counter.
i=0
while [  $i -lt $limit ]; do

    let i=i+1

    # Execute the simultaneous test experiment.
    out/site

    # Test the site execution result.
    if [ $? -eq 0 ]; 
    then 
	echo "*** executed successfully:" $i
    else 
	echo "*** terminated unsuccessfully: " $i
	exit 1
    fi
done

exit 0
