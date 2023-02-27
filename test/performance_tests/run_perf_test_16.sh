#! /bin/bash
#***************************************************************************
echo "***QZ_TOOL run_perf_test_16.sh start"

set -e

Test_file_path="/opt/compressdata"
RC=0

rm -rf test_result
rm -f result_stderr

CURRENT_PATH=`dirname $(readlink -f "$0")`

#check whether test exists
if [ ! -f "$QZ_ROOT/test/test" ]; then
    echo "$QZ_ROOT/test/test: No such file. Compile first!"
    exit 1
fi

#get the type of QAT hardware
platform=`lspci | grep Co-processor | awk '{print $6}' | head -1`
echo "platform=$platform"


/bin/cp $CURRENT_PATH/config_file/4xxx_16/4xxx*.conf /etc

service qat_service restart
echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
rmmod usdm_drv
insmod $ICP_ROOT/build/usdm_drv.ko max_huge_pages=2048 max_huge_pages_per_process=1024
sleep 5

cat /sys/kernel/debug/qat_4xxx_0000\:6b\:00.0/fw_counters >> test_result_fwcounters
cat /sys/kernel/debug/qat_4xxx_0000\:70\:00.0/fw_counters >> test_result_fwcounters
cat /sys/kernel/debug/qat_4xxx_0000\:75\:00.0/fw_counters >> test_result_fwcounters
cat /sys/kernel/debug/qat_4xxx_0000\:7a\:00.0/fw_counters >> test_result_fwcounters
cat /sys/kernel/debug/qat_4xxx_0000\:e8\:00.0/fw_counters >> test_result_fwcounters
cat /sys/kernel/debug/qat_4xxx_0000\:ed\:00.0/fw_counters >> test_result_fwcounters
cat /sys/kernel/debug/qat_4xxx_0000\:f2\:00.0/fw_counters >> test_result_fwcounters
cat /sys/kernel/debug/qat_4xxx_0000\:f7\:00.0/fw_counters >> test_result_fwcounters

function performance_test_with_calgary
{
    NumLoop=$1
    Num2=$2
    Process=1
    Thread=$4
    Service=$5
    Level=$6
    Testfile=$7
    Buffersz=$8

    if [ ! -f "$Testfile" ]
    then
        echo "$Testfile does not exit!"
        return 1
    fi

    cp -rf $Testfile ./

    #$QZ_TOOL/install_drv/install_upstream.sh -P $NumProcess -D $NumDcInstance -L
    
    #echo 4096 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
    #rmmod usdm_drv
    #insmod $ICP_ROOT/build/usdm_drv.ko max_huge_pages=2048 max_huge_pages_per_process=960
    
    rm -rf result_stderr
    rm -rf result

    if [ $Service == "comp" ];then
        cpu_list=0
        for((num = 0; num < $Process; num++))
        do
		    taskset -c $cpu_list $QZ_ROOT/test/test -m 4 -l $NumLoop -t $Thread -B 0 -D $Service -L $Level -i $Testfile -C $Buffersz  >> result 2>> result_stderr &
            cpu_list=$(($cpu_list + 1))
        done
        wait
    else
        cpu_list=0
        for((num = 0; num < $Process; num++))
        do
            taskset -c $cpu_list $QZ_ROOT/test/test -m 4 -l $NumLoop -t $Thread -B 0 -D $Service -L $Level -i $Testfile -C $Buffersz  >> result 2>> result_stderr &
            cpu_list=$(($cpu_list + 1))
        done
        wait
    fi

    Throughput=`awk '{sum+=$8} END{print sum}' result`
	us=`awk '{$6} END{print $6}' result`

	if [ $Service == "comp" ];then
		    ratio=`awk '{sum+=substr($12, 7)}  {if(substr($12, 7)>0) nn+=1} END{print sum/nn}'  result`
            echo "Level $Level, Loop $NumLoop, Thread $Thread, $Testfile, $Buffersz dynamic $Service, Throughput= $Throughput Gbps, $us Ratio= $ratio" &
            echo "Level $Level, Loop $NumLoop, Thread $Thread, $Testfile, $Buffersz dynamic $Service, Throughput= $Throughput Gbps, $us Ratio= $ratio" >> test_result
    else
            echo "Level $Level, Loop $NumLoop, Thread $Thread, $Testfile, $Buffersz dynamic $Service, Throughput= $Throughput Gbps, $us" &
            echo "Level $Level, Loop $NumLoop, Thread $Thread, $Testfile, $Buffersz dynamic $Service, Throughput= $Throughput Gbps, $us" >> test_result
    fi
    cat /sys/kernel/debug/qat_4xxx_0000\:6b\:00.0/fw_counters >> test_result_fwcounters
    cat /sys/kernel/debug/qat_4xxx_0000\:70\:00.0/fw_counters >> test_result_fwcounters
    cat /sys/kernel/debug/qat_4xxx_0000\:75\:00.0/fw_counters >> test_result_fwcounters
    cat /sys/kernel/debug/qat_4xxx_0000\:7a\:00.0/fw_counters >> test_result_fwcounters
    cat /sys/kernel/debug/qat_4xxx_0000\:e8\:00.0/fw_counters >> test_result_fwcounters
    cat /sys/kernel/debug/qat_4xxx_0000\:ed\:00.0/fw_counters >> test_result_fwcounters
    cat /sys/kernel/debug/qat_4xxx_0000\:f2\:00.0/fw_counters >> test_result_fwcounters
    cat /sys/kernel/debug/qat_4xxx_0000\:f7\:00.0/fw_counters >> test_result_fwcounters

    if [ -s result_stderr ];then
        echo "Error in test!"
       RC=1
    fi

    #rm -rf result_stderr
    #rm -rf result
    #rm -rf $Testfile
}

#Parameter: NumProcess NumDcInstance Process Thread Service Level Testfile Buffersz 

./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 comp 1 /opt/compressdata/calgary 16384
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 decomp 1 /opt/compressdata/calgary 16384
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10

./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 comp 1 /opt/compressdata/calgary 65536
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 decomp 1 /opt/compressdata/calgary 65536
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10

Date=$(date "+%Y%m%d%H%M%S")
echo "The test end at $Date, directory: $QZ_ROOT/result_test_2023/test"
cp test_result $QZ_ROOT/result_test_2023/test/run_perf_test_result_$Date

if [ RC == 1 ];then
   echo "Performance test failed."
   exit 1
else
   echo "Performance test passed."
fi
echo "***QZ_TOOL run_perf_test_16.sh end"
exit 0

./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 comp 1 /opt/compressdata/calgary 4096
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 decomp 1 /opt/compressdata/calgary 4096
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 comp 1 /opt/compressdata/calgary 8192
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 decomp 1 /opt/compressdata/calgary 8192
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10

./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 comp 1 /opt/compressdata/calgary 32768
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 decomp 1 /opt/compressdata/calgary 32768
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10

./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 comp 1 /opt/compressdata/calgary 16384
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 decomp 1 /opt/compressdata/calgary 16384
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10

./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 comp 1 /opt/compressdata/calgary 65536
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 decomp 1 /opt/compressdata/calgary 65536
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 comp 1 /opt/compressdata/calgary 131072
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 100 0 1 4 decomp 1 /opt/compressdata/calgary 131072
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10