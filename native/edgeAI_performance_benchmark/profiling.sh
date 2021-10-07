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

COUNTER=0
RUNNING_TIME=10

SERVER_CPU_TOT=0
SEVER_MEM_TOT=0
CLIENT_CPU_TOT=0.0
CLIENT_MEM_TOT=0

START_TIME=$(date +%s.%3N)

if [[ "$PROTOCOL" == "query/mqtt" ]]; then
  ./performance_benchmark_query --server -t 100 --mqtthost=localhost --operation=oper --width=$WIDTH --height=$HEIGHT &
  ./performance_benchmark_query --client -t 100 --mqtthost=localhost --operation=oper --width=$WIDTH --height=$HEIGHT &
  SERVER_PID=`ps aux | grep performance_benchmark | grep server | awk '{print $2}'`
  CLIENT_PID=`ps aux | grep performance_benchmark | grep client | awk '{print $2}'`
elif [[ "$PROTOCOL" == "query/tcp" ]]; then
  ./performance_benchmark_query --server -t 100 --width=$WIDTH --height=$HEIGHT &
  ./performance_benchmark_query --client -t 100 --width=$WIDTH --height=$HEIGHT &
  SERVER_PID=`ps aux | grep performance_benchmark | grep server | awk '{print $2}'`
  CLIENT_PID=`ps aux | grep performance_benchmark | grep client | awk '{print $2}'`
elif [[ "$PROTOCOL" == "zmq" ]]; then
  ./performance_benchmark_broadcast --zmq --pub -t 100 --width=$WIDTH --height=$HEIGHT &
  ./performance_benchmark_broadcast --zmq --sub -t 100 --width=$WIDTH --height=$HEIGHT &
  SERVER_PID=`ps aux | grep performance_benchmark | grep pub | awk '{print $2}'`
  CLIENT_PID=`ps aux | grep performance_benchmark | grep sub | awk '{print $2}'`
else
  ./performance_benchmark_broadcast --mqtt --pub -t 100 --width=$WIDTH --height=$HEIGHT &
  ./performance_benchmark_broadcast --mqtt --sub -t 100 --width=$WIDTH --height=$HEIGHT &
  SERVER_PID=`ps aux | grep performance_benchmark | grep pub | awk '{print $2}'`
  CLIENT_PID=`ps aux | grep performance_benchmark | grep sub | awk '{print $2}'`
fi

echo "server pid: $SERVER_PID client pid: $CLIENT_PID"

while true; do
  STAT=`pidstat -u -r -p $SERVER_PID -p $CLIENT_PID 1 1`

  SERVER_CPU=`echo "$STAT" | awk 'NR==4{print}' | awk '{print $10}'`
  SEVER_MEM=`echo "$STAT" | awk 'NR==8{print}' | awk '{print $9}'`
  SERVER_CPU_TOT=`awk "BEGIN{ print $SERVER_CPU_TOT + $SERVER_CPU }"`
  SEVER_MEM_TOT=$[ $SEVER_MEM_TOT + $SEVER_MEM]

  CLIENT_CPU=`echo "$STAT" | awk 'NR==5{print}' | awk '{print $10}'`
  CLIENT_MEM=`echo "$STAT" | awk 'NR==9{print}' | awk '{print $9}'`
  CLIENT_CPU_TOT=`awk "BEGIN{ print $CLIENT_CPU_TOT + $CLIENT_CPU }"`
  CLIENT_MEM_TOT=$[ $CLIENT_MEM_TOT + $CLIENT_MEM]

  echo "Server CPU total: $SERVER_CPU_TOT, mem total: $SEVER_MEM_TOT"
  echo "Client CPU total: $CLIENT_CPU_TOT, mem total: $CLIENT_MEM_TOT"
  echo ""

  COUNTER=$[ $COUNTER +1 ]
  echo "$COUNTER sec"
  if [ $COUNTER -ge $RUNNING_TIME ]; then
    break;
  fi
done
ps aux  |  grep -i performance  |  awk '{print $2}'  |  xargs kill -15
END_TIME=$(date +%s.%3N)
ELAPSED=$(echo "scale=3; $END_TIME - $START_TIME" | bc)
SERVER_CPU_AVG=`echo "scale=4; $SERVER_CPU_TOT / $RUNNING_TIME" | bc`
SERVER_MEM_AVG=`echo "scale=4; $SEVER_MEM_TOT / $RUNNING_TIME" | bc`
CLEINT_CPU_AVG=`echo "scale=4; $CLIENT_CPU_TOT / $RUNNING_TIME" | bc`
CLIENT_MEM_AVG=`echo "scale=4; $CLIENT_MEM_TOT / $RUNNING_TIME" | bc`

echo "$ELAPSED"
echo "$SERVER_CPU_AVG"
echo "$SERVER_MEM_AVG"
echo "$CLEINT_CPU_AVG"
echo "$CLIENT_MEM_AVG"
