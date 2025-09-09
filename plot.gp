set terminal pngcairo size 1200,700 enhanced font "Arial,14"
set output 'result.png'

set style data histogram
set style fill solid 0.8 border -1
set boxwidth 0.6
set key off

unset xtics

set ylabel 'time (ms)'
set title 'speed testings'
set style line 1 lc rgb "#666666" lt 1 lw 1
set grid ytics linestyle 1
set logscale y

plot 'results.dat' using 1:2 with boxes lc rgb '#4C72B0' title '', \
     '' using 1:2:3:4 with yerrorbars lc rgb 'black' notitle

