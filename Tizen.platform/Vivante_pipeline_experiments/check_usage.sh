#!/usr/bin/env bash
##
## @file check_usage.sh
## @author HyoungJoo Ahn <hello.ahn@samsung.com>
## @date 13 May 2020
## @brief print cpu(real/user/sys) time & memory(peak) usage 
##

if [ -z "$1" ]; then
  ENABLE_VNN_INCEPTION=1
else
  ENABLE_VNN_INCEPTION=$1
fi

if [ -z "$2" ]; then
  ENABLE_VNN_YOLO=1
else
  ENABLE_VNN_YOLO=$2
fi

if [ -z "$3" ]; then
  ENABLE_TFLITE_INCEPTION=1
else
  ENABLE_TFLITE_INCEPTION=$3
fi

if [ -z "$4" ]; then
  SLEEP_TIME=10
else
  SLEEP_TIME=$4
fi

time vivante-pipeline-experiment $ENABLE_VNN_INCEPTION $ENABLE_VNN_YOLO $ENABLE_TFLITE_INCEPTION $SLEEP_TIME &
process_id=`/bin/ps -fu $USER| grep "vivante-pipeline-experiment" | grep -v "grep" | awk '{print $2}'`

taskset -cp 2-5 $process_id # set affinity
end=$((SECONDS+$SLEEP_TIME+1))

PEAK=0
MIN=10000000000000
CNT=0
SUM_MEM=0

if [ -d "/proc/$process_id" ]; then
  sleep $SLEEP_TIME
  output=`grep VmRSS /proc/$process_id/status`
  IFS=' ' read -ra mem_str <<< "$output"
  mem_usage=$((${mem_str[1]}))
fi
echo "Mem Usage: $mem_usage"
