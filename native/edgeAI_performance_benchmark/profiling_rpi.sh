#!/usr/bin/env bash
if [ -z "$1" ]; then
  echo "video width is not given. Use 640."
  WIDTH="640"
else
  WIDTH="$1"
fi
if [ -z "$2" ]; then
  echo "video height is not given. Use 480."
  HEIGHT="480"
else
  HEIGHT="$2"
fi
if [ -z "$3" ]; then
  echo "protocol is not given. Use TCP query."
  PROTOCOL="query/tcp"
else
  PROTOCOL="$3"
fi
if [ -z "$4" ]; then
  echo "Please specify the type of the pipeline. Default server (server|client|pub|sub)."
  TYPE="server"
else
  TYPE="$4"
fi
if [ -z "$5" ]; then
  FRAMERATE="60"
else
  FRAMERATE="$5"
fi

echo "" >> temp_result*.txt
echo "$PROTOCOL" >> temp_result*.txt
echo "$WIDTH X $HEIGHT" >> temp_result*.txt

COUNTER=0
RUNNING_TIME=10

CPU_TOT=0
MEM_TOT=0

# cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors
# echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

# echo 0 > /sys/devices/system/cpu/cpu0/online
# cat /sys/devices/system/cpu/online
# cat /sys/devices/system/cpu/offline
# echo 1 > /sys/devices/system/cpu/cpu0/online

START_TIME=$(date +%s.%3N)

if [[ "$PROTOCOL" == "query/mqtt" ]]; then
  ./performance_benchmark_query --$TYPE -t 100 --srvhost= --desthost= --destport= --topic=profilingTopic --width=$WIDTH --height=$HEIGHT --connecttype=HYBRID --framerate=$FRAMERATE &
elif [[ "$PROTOCOL" == "query/tcp" ]]; then
  ./performance_benchmark_query --$TYPE -t 100 --srvhost= --srvport= --width=$WIDTH --height=$HEIGHT --framerate=$FRAMERATE &
elif [[ "$PROTOCOL" == "pubsub/zmq" ]]; then
  ./performance_benchmark_broadcast --$TYPE -t 100 --host=  --width=$WIDTH --height=$HEIGHT --connecttype=ZMQ &
elif [[ "$PROTOCOL" == "pubsub/hybrid" ]]; then
  ./performance_benchmark_broadcast --$TYPE -t 100 --host= --desthost= --destport= --topic=profilingTopic --width=$WIDTH --height=$HEIGHT --connecttype=HYBRID &
elif [[ "$PROTOCOL" == "pubsub/aitt" ]]; then
  ./performance_benchmark_broadcast --$TYPE -t 100 --host= --desthost= --destport= --topic=profilingTopic --width=$WIDTH --height=$HEIGHT --connecttype=AITT &
else
  ./performance_benchmark_broadcast --$TYPE -t 100 --host= --width=$WIDTH --height=$HEIGHT --connecttype=MQTT &
fi
PID=`ps aux | grep performance_benchmark | grep $TYPE | awk '{print $2}'`

echo "pid: $PID"
taskset -cp 0 $PID

while true; do
  STAT=`pidstat -u -r -p $PID 1 1`

  CPU=`echo "$STAT" | awk 'NR==4{print}' | awk '{print $8}'`
  MEM=`echo "$STAT" | awk 'NR==7{print}' | awk '{print $7}'`
  CPU_TOT=`awk "BEGIN{ print $CPU_TOT + $CPU }"`
  MEM_TOT=$[ $MEM_TOT + $MEM]

  echo "Server CPU total: $MEM, mem total: $MEM_TOT"
  echo ""

  COUNTER=$[ $COUNTER +1 ]
  echo "$COUNTER sec"
  if [ $COUNTER -ge $RUNNING_TIME ]; then
    break;
  fi
done

END_TIME=$(date +%s.%3N)
ELAPSED=$(echo "scale=3; $END_TIME - $START_TIME" | bc)
kill -15 $PID
elapsed=$(( end_time - start_time ))
echo %elapsed

CPU_AVG=`echo "scale=4; $CPU_TOT / $RUNNING_TIME" | bc`
MEM_AVG=`echo "scale=4; $MEM_TOT / $RUNNING_TIME" | bc`

echo "$ELAPSED" >> temp_result*.txt
echo "$CPU_AVG" >> temp_result*.txt
echo "$MEM_AVG" >> temp_result*.txt
