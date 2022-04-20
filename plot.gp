set title "Run Time"
set xlabel "n-th"
set ylabel "ns"
set terminal png font " Verdana,12 "
set output "plot.png"
set xtics 0 ,10 ,100
set key left 

plot \
"performance.txt" using 1:2 with linespoints linewidth 2 title "user space", \
"performance.txt" using 1:3 with linespoints linewidth 2 title "kernel space", \
"performance.txt" using 1:4 with linespoints linewidth 2 title "kernel to user space", \