#!/bin/bash

#
#   Simple CPU Usage script
#   Measures the CPU usage by taking two samples from /proc/stats
#
#   Emutex Ltd.
#   Jose M. Rodriguez
#


PRINT_USER=0          # Show User mode CPU usage
PRINT_KERNEL=0        # Show Kernel mode CPU usage
PRINT_USER_KERNEL=0   # Shows sum of User+Kernel
PRINT_IDLE=0          # Show IDLE CPU time
PRINT_TOTAL=0         # Show Total CPU time
PRINT_CPU_TIME=0      # Show the CPU usage in CPU time instead of percentage
TIMER=10              # Time to measure

function ctrl_c ()
{
    echo "Stop requested"
    exit
}
trap ctrl_c SIGINT

usage() {

    echo "cpu_usage prints to stdout the CPU usage during the specified time"
    echo "./cpu_usage -t<TIMER> -u -k -i -c -s"
    echo "-t Time to meassure in seconds. Default 10s."
    echo "-u Print user mode."
    echo "-k Print kernel mode."
    echo "-i Print idle mode."
    echo "-c Print CPU time instead of percentage."
    echo "-s Print user+kernel mode."
    echo "-a Print total CPU time."

    echo -e "\nOUTPUT: User Kernel User+Kernel Idle Total"
    echo -e "\nNOTE: Output is always in the same order, flags order does not affect."
    echo -e "NOTE: User+Kernel+Idle could not match total.\n"
}


while getopts "t:ukicsa" OPTION
do
    case $OPTION in
        t)
            TIMER=$OPTARG
            ;;
        u)
            PRINT_USER=1
            ;;
        k)
            PRINT_KERNEL=1
            ;;
        i)
            PRINT_IDLE=1
            ;;
        c)
            PRINT_CPU_TIME=1
            ;;
        s)
            PRINT_USER_KERNEL=1
            ;;
        a)
            PRINT_TOTAL=1
            ;;
        ?)
            usage
            exit
            ;;
    esac
done

CURRENT_CPU_INFO=(`cat /proc/stat | grep "cpu "`)   # Get the aggregated information
CURRENT_CPU_INFO[0]=0                               # Remove the 'cpu' text

TOTAL_START=0                                       # Calculate the total CPU time by adding all the fields
for i in ${CURRENT_CPU_INFO[@]}
do
  let TOTAL_START=$TOTAL_START+$i
done

USER_START=${CURRENT_CPU_INFO[1]}                   # Store the user mode time at the start
KERNEL_START=${CURRENT_CPU_INFO[3]}                 # Store the kernel mode time at the start
IDLE_START=${CURRENT_CPU_INFO[4]}                   # Store the idle time time at the start

sleep $TIMER

CURRENT_CPU_INFO=(`cat /proc/stat | grep "cpu "`)   # Get the aggregated information again
CURRENT_CPU_INFO[0]=0

TOTAL_END=0                                       
for i in ${CURRENT_CPU_INFO[@]}
do
  let TOTAL_END=$TOTAL_END+$i
done

USER_END=${CURRENT_CPU_INFO[1]} 
KERNEL_END=${CURRENT_CPU_INFO[3]}
IDLE_END=${CURRENT_CPU_INFO[4]}

let USER_MODE=$USER_END-$USER_START
let KERNEL_MODE=$KERNEL_END-$KERNEL_START
let USER_KERNEL=$USER_MODE+$KERNEL_MODE
let IDLE_MODE=$IDLE_END-$IDLE_START
let TOTAL=$TOTAL_END-$TOTAL_START

if [ $PRINT_CPU_TIME -eq 0  ]       # Calculate percentages
then
  USER_MODE=`echo  $(echo "scale=3; $USER_MODE*100/$TOTAL" | bc -l 2> /dev/null)`
  if [ ! $USER_MODE ]; then USER_MODE=0; fi

  KERNEL_MODE=`echo  $(echo "scale=3; $KERNEL_MODE*100/$TOTAL" | bc -l 2> /dev/null)`
  if [ ! $KERNEL_MODE ]; then KERNEL_MODE=0; fi

  IDLE_MODE=`echo  $(echo "scale=3; $IDLE_MODE*100/$TOTAL" | bc -l 2> /dev/null)`
  if [ ! $IDLE_MODE ]; then IDLE_MODE=0; fi

  USER_KERNEL=`echo  $(echo "scale=3; $USER_KERNEL*100/$TOTAL" | bc -l 2> /dev/null)`
  if [ ! $USER_KERNEL ]; then USER_KERNEL=0; fi

fi

if [ $PRINT_USER -ne 0 ]
then
  echo -n $USER_MODE ' '
fi

if [ $PRINT_KERNEL -ne 0 ]
then
  echo -n $KERNEL_MODE ' '
fi

if [ $PRINT_USER_KERNEL -ne 0 ]
then
  echo -n $USER_KERNEL ' '
fi

if [ $PRINT_IDLE -ne 0 ]
then
  echo -n $IDLE_MODE ' '
fi

if [ $PRINT_TOTAL -ne 0 ]
then
  echo -n $TOTAL ' '
fi

echo -ne "\n"
