reset
set ylabel 'time(usec)'
set style fill solid
set title 'total time per iteration 100 active clients'
set term png enhanced font 'Verdana,10'
set output '100_polling_notimeout.png'
#set xtics 100 #設定x軸刻度增加量(刻度間距)
set logscale x
plot [100:][:]'data/100_polling_epoll.txt' using 1:2 with line title 'epoll', \
'data/100_polling_poll.txt' using 1:2 with line title 'poll'	
