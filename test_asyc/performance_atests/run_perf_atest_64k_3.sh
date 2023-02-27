#! /bin/bash
#***************************************************************************
echo "***QZ_TOOL run_perf_atest_64K_3.sh start"

set -e

Test_file_path="/opt/compressdata"
RC=0

rm -rf test_result

/bin/cp -rf ./config_file/c6xx_dev0.conf /etc
/bin/cp -rf ./config_file/c6xx_dev1.conf /etc
/bin/cp -rf ./config_file/c6xx_dev2.conf /etc

service qat_service restart

function performance_test_with_calgary
{
    NumProcess=$1
    NumDcInstance=$2
    Num=$3
    Thread=$4
    Service=$5
    Level=$6
    Testfile=$7
    Huffman=$8
    Buffersz=$9
    tid=${10}

    if [ ! -f "$Test_file_path/$Testfile" ]
    then
        echo "$Test_file_path/$Testfile does not exit!"
        return 1
    fi

    cp -rf $Test_file_path/$Testfile ./

    #$QZ_TOOL/install_drv/install_upstream.sh -P $NumProcess -D $NumDcInstance -L
    
    #echo 4096 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
    #rmmod usdm_drv
    #insmod $ICP_ROOT/build/usdm_drv.ko max_huge_pages=2048 max_huge_pages_per_process=960
    
    rm -rf result_stderr
    rm -rf result
    
    if [ $Service == "comp" ];then
        for((num = 0; num < $Num; num++))
        do
		      taskset -c 0	../atest -m 4 -l 500 -t $Thread -B 0 -D $Service -L $Level -i $Testfile -T $Huffman -C $Buffersz -Q $tid >> result 2>> result_stderr &
        done
        wait
    else
        for((num = 0; num < $Num; num++))
        do
          taskset -c 0    ../atest -m 4 -l 500 -t $Thread -B 0 -D $Service -L $Level -i $Testfile -T $Huffman -C $Buffersz -Q $tid >> result 2>> result_stderr &
        done
        wait
    fi

    Throughput=`awk '{sum+=$8} END{print sum}' result`
	  us=`awk '{$6} END{print $6}' result`
	
#    if [ -s result_stderr ];then
#        echo "Error in test!"
#        RC=1
#    fi

    rm -rf result_stderr
    rm -rf result
    rm -rf $Testfile

    echo "Level $Level Thread $Thread $Testfile $Buffersz $Huffman $Service Throughput= $Throughput Gbps $us"
    echo "Level $Level Thread $Thread $Testfile $Buffersz $Huffman $Service Throughput= $Throughput Gbps $us" >> test_result

}

#Parameter: NumProcess NumDcInstance Num Thread Service Level Testfile Huffman Buffersz  tid
#Parameter: 1             2           1   6        -       1   calgary.512M static  64k   1     
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 1 2 1 6 comp 1 calgary.512M static 65536 1
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10

#Parameter: NumProcess NumDcInstance Num Thread Service Level Testfile Huffman Buffersz  tid
#Parameter: 1             2           1   6        -       1   calgary.512M static  64k   1    
./server_cpu_usage.pl 7 NGINX
./memory_monitor.pl
performance_test_with_calgary 1 2 1 6 decomp 1 calgary.512M static 65536 1
./read_server_cpu_usage_nginx.pl 7
./read_memory.sh
sleep 10

Date=$(date "+%Y%m%d%H%M%S")
echo "The test end at $Date"
cp test_result $ATF_PATH/run_perf_test_result_$Date

if [ RC == 1 ];then
   echo "Performance test failed."
   exit 1
else
   echo "Performance test passed."
fi
echo "***QZ_TOOL run_perf_atest_64K_3.sh end"
exit 0
