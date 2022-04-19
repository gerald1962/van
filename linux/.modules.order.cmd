cmd_/home/gerald/van_development/van/linux/modules.order := {   echo /home/gerald/van_development/van/linux/hello.ko; :; } | awk '!x[$$0]++' - > /home/gerald/van_development/van/linux/modules.order
