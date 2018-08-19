#! /bin/bash

if [ "$1" == "" ]
then
	echo "USAGE: ./run_experiment.sh <simulator-program>.cc"
else
	./waf --run $1
fi
