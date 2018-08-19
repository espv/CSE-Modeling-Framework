#! /bin/bash

if [ "$1" == "" ]
then
	echo "USAGE: ./run_gdb_experiment.sh <simulator-program>.cc"
else
	./waf --run $1 --command-template="gdb %s"
fi
