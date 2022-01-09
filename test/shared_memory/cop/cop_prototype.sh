#!/bin/bash

#
# cop_prototype.sh - controller, battery and display simulation are running as
# autonomous OS process in their own terminal, which is held as long as the user
# does not press the x - close button at the top right corner.
#
# $1: controller->battery generator cycle number.
# $2: controller->display generator cycle number.
# $3: battery->controller generator cycle number.
# $4: display->controller generator cycle number.
#
# Return    None.
#

# Start the controller with wait condition and trace.
# Settings:
# -s c:     stand-alone controller process.
# -cw:      suspend the controller thread, if no I/O data are available.
# -cbc $1:  number of the controller->battery generator cycles.
# -cdc $2:  number of the controller->display generator cycles.
# -t:       the controller program prints some status information.
xfce4-terminal --hold --title=controller -e "out/cop -s c -cw -cbc $1 -cdc $2 -t" &

# The controller needs some time to install the shared memory area, to create
# the resources for two comunication cables. Therefore we delay the start of the
# battery and display process.
# XXX: Battery and display endpoints shall poll the named semaphores for a certain time.
sleep 0.250

# Start the battery with wait condition and trace.
# Settings:
# -s c:     stand-alone battery process.
# -bw:      suspend the battery thread, if no I/O data are available.
# -bcc $3:  number of the battery->controller generator cycles.
# -t:       the battery program prints some status information.
xfce4-terminal --hold --title=battery    -e "out/cop -s b -bw -bcc $3         -t" &

# Start the display with wait condition and trace.
# Settings:
# -s c:     stand-alone display process.
# -dw:      suspend the display thread, if no I/O data are available.
# -dcc $4:  number of the display->controller generator cycles.
# -t:       the diplay program prints some status information.
xfce4-terminal --hold --title=display    -e "out/cop -s d -dw -dcc $4         -t" &

# cop_prototype.sh
