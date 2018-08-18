#! /bin/bash

if [ "$1" == "" ]
then
        echo "USAGE: ./run_generate_plot.sh <simulator-program>.cc"
else
	rm plots/*.gnu
	rm plots/*.png
        ./waf --run $1
	cd plots
	gnuplot *.gnu
	montage -tile 3x -geometry +0+0 *.png montage.png
	feh montage.png&
fi

