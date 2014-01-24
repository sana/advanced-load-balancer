#!/bin/bash

./broker &

./client "ls -la" &
./client "pwd" &
./client "ls -la" &
./client "pwd" &
./client "ls -la" &
./client "pwd" &
./client "ls -la" &
./client "pwd" &
./client "ls -la" &
./client "pwd" &
./client "ls -la" &
./client "pwd" &

./server &
#./server &
#./server &

#./client &
#./client "ls -la" &
#./client "pwd" &
#./client "ps aux" &
#./client "cat /etc/hosts" &
#./client "host mail.twitter.com" &
#./client "date" &

sleep 5

killall client
killall server
killall broker
