#! /bin/bash

killall -15 memory_monitor.pl

sleep 3; # wait until the process ends

memfilename=`cat mem_file_name.txt`
echo "MEM FILE NAME = " $memfilename
	
MEM_MAX=`awk -F ',' 'BEGIN {max = 0} {if ($3+0 > max+0) {max=$3 ;content=$3} } END {print content}'  $memfilename`
MEM_AVE=`awk -F ',' '{x+=$3} END {printf ("%.0f\n", x/NR)}' $memfilename`
MEM_END=`awk -F ',' 'END {print $3}' $memfilename`
	
echo "MEM used(KB) MAX = " $MEM_MAX
echo "MEM used(KB) AVE = " $MEM_AVE
echo "MEM used(KB) END = " $MEM_END
echo "-----------------------------------------------------------"

