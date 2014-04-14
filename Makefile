COMPILER=gcc
LDFLAGS=-lzmq
CFLAGS=-g2
COMMON_INCLUDE_PATH=./common
QUEUE_INCLUDE_PATH=./broker-impl/src

all: broker server client queue_tester

broker:
	cc broker-impl/broker-impl/main.c broker-impl/broker-impl/queue.c broker-impl/broker-impl/worker.c $(CFLAGS) -I"$(COMMON_INCLUDE_PATH)" -I"$(QUEUE_INCLUDE_PATH)" $(LDFLAGS) -o broker

server:
	cc server-impl/server-impl/main.c $(CFLAGS) -I"$(COMMON_INCLUDE_PATH)" $(LDFLAGS) -o server

client:
	cc client-impl/client-impl/main.c $(CFLAGS) -I"$(COMMON_INCLUDE_PATH)" $(LDFLAGS) -o client

queue_tester:
	cc broker-impl/broker-impl/queue_tester.c broker-impl/broker-impl/queue.c $(CFLAGS) -I"$(COMMON_INCLUDE_PATH)" $(LDFLAGS) -o queue_tester


.PHONY: clean
clean:
	rm -rf broker server client queue_tester
