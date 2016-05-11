FLAGS = -O3 -fno-guess-branch-probability -g
PARA = -n 1000 -a 1000 -w 50

make:
	gcc $(FLAGS) -o bench bench.c -levent 

run_100:
	./bench -a 100 -m 1
	./bench -a 100 -m 2	

plot_100:
	gnuplot 100_all_notimeout.gp
	gnuplot 100_polling_notimeout.gp	
