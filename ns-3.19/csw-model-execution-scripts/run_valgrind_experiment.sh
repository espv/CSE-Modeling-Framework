#! /bin/bash

if [ "$1" == "" ]
then
        echo "USAGE: ./run_valgrind_experiment.sh <simulator-program>.cc"
else
	./waf --disable-gtk --command-template="valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all --log-file=log.txt %s" --run $1

	echo "Running grep 'Invalid' log.txt to see if there were any memory errors."
	echo "For more details, read through log.txt"
	grep "Invalid" log.txt
	echo "Done."
fi

