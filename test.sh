#!/bin/bash

echo 'LSOC:' `cat */*/*.c | wc -l`

./broker &

./server &
./server &
./server &

function execute_dummy_task {
  for i in {1..100}
  do
    ./client "uname -a" &
  done
}

execute_dummy_task

./client "ping google.com" &

execute_dummy_task

sleep 5

killall client
killall server
killall broker
