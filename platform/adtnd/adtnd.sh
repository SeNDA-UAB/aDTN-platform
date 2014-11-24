#!/bin/bash

###
# Specify here the installation prefic and the time between tests
###

INSTALL_PREFIX=/usr/local

CHECK_PERIOD=120

#####
# Don't modify this part of the script if you don't know perfectly what are you doing.
#####


if [ -z $1 ]; then
	config_file=$INSTALL_PREFIX"/etc/adtn/adtn.ini"
else
	config_file=$1
fi

nohup $INSTALL_PREFIX/bin/information_exchange -c $config_file -f > /dev/null 2>&1 &
let RITDPID=$!

nohup $INSTALL_PREFIX/bin/queueManager -c $config_file > /dev/null 2>&1 &
let QUEUEPID=$!

nohup $INSTALL_PREFIX/bin/processor -c $config_file > /dev/null 2>&1 &
let OUTGOINGPID=$!

nohup $INSTALL_PREFIX/bin/receiver -c $config_file > /dev/null 2>&1 &
let AGENTPID=$!

nohup $INSTALL_PREFIX/bin/executor -c $config_file > /dev/null 2>&1 &
let EXECUTORPID=$!

while :
do
	if ! kill -0 $RITDPID  > /dev/null 2>&1; then
		nohup $INSTALL_PREFIX/bin/information_exchange -c $config_file > /dev/null 2>&1 &
		let RITDPID=$!
	fi

	if ! kill -0 $QUEUEPID  > /dev/null 2>&1; then
		nohup $INSTALL_PREFIX/bin/queueManager -c $config_file > /dev/null 2>&1 &
		let QUEUEPID=$!
	fi

	if ! kill -0 $OUTGOINGPID  > /dev/null 2>&1; then
		nohup $INSTALL_PREFIX/bin/processor -c $config_file > /dev/null 2>&1 &
		let OUTGOINGPID=$!
	fi

	if ! kill -0 $AGENTPID  > /dev/null 2>&1; then
		nohup $INSTALL_PREFIX/bin/receiver -c $config_file > /dev/null 2>&1 &
		let AGENTPID=$!
	fi

	if ! kill -0 $EXECUTORPID  > /dev/null 2>&1; then
		nohup $INSTALL_PREFIX/bin/executor -c $config_file > /dev/null 2>&1 &
		let EXECUTORPID=$!
	fi

	sleep $CHECK_PERIOD
done
