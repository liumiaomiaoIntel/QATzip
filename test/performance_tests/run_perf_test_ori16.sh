#! /bin/bash
################################################################
#   BSD LICENSE
#
#   Copyright(c) 2007-2022 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
################################################################

set -e
echo "***QZ_ROOT run_perf_test.sh start"

rm -f result_comp_stderr
rm -f result_decomp_stderr
rm -f result_comp
rm -f result_decomp

CURRENT_PATH=`dirname $(readlink -f "$0")`

#check whether test exists
if [ ! -f "$QZ_ROOT/test/test" ]; then
    echo "$QZ_ROOT/test/test: No such file. Compile first!"
    exit 1
fi

#get the type of QAT hardware
platform=`lspci | grep Co-processor | awk '{print $6}' | head -1`
if [[ $platform != "37c8" && $platform != "4940" ]]
then
    platform=`lspci | grep Co-processor | awk '{print $5}' | head -1`
    if [[ $platform != "DH895XCC" && $platform != "C62x" ]]
    then
        platform=`lspci | grep Co-processor | awk '{print $7}' | head -1`
        if [ $platform != "C3000" ]
        then
            echo "Unsupport Platform: `lspci | grep Co-processor` "
            exit 1
        fi
    fi
fi
echo "platform=$platform"


#Replace the driver configuration files and configure hugepages
echo "Replace the driver configuration files and configure hugepages."
process=1
\cp $CURRENT_PATH/config_file/4xxx_16/4xxx*.conf /etc

service qat_service restart
echo 2048 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
rmmod usdm_drv
insmod $ICP_ROOT/build/usdm_drv.ko max_huge_pages=2048 max_huge_pages_per_process=1024
sleep 5

#Perform performance test
echo "Perform performance test"

thread=16
Buffersz=65536
Testfile=/opt/compressdata/calgary
echo > result_comp
cpu_list=0
for((numProc_comp = 0; numProc_comp < $process; numProc_comp ++))
do
    taskset -c $cpu_list $QZ_ROOT/test/test -m 4 -l 1000 -t $thread -D comp -B 0 -i $Testfile -C $Buffersz >> result_comp 2>> result_comp_stderr &
    cpu_list=$(($cpu_list + 1))
done
wait
compthroughput=`awk '{sum+=$8} END{print sum}' result_comp`
echo "compthroughput=$compthroughput Gbps"

echo > result_decomp
cpu_list=0
for((numProc_decomp = 0; numProc_decomp < $process; numProc_decomp ++))
do
    taskset -c $cpu_list $QZ_ROOT/test/test -m 4 -l 1000 -t $thread -D decomp -B 0 -i $Testfile -C $Buffersz >> result_decomp 2>> result_decomp_stderr &
    cpu_list=$(($cpu_list + 1))
done
wait
decompthroughput=`awk '{sum+=$8} END{print sum}' result_decomp`
echo "decompthroughput=$decompthroughput Gbps"

#rm -f result_comp
#rm -f result_decomp
echo "***QZ_ROOT run_perf_test.sh end"
