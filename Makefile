FLAGS = -O3 -fno-guess-branch-probability -g

make:
	gcc $(FLAGS) -o bench bench.c -levent 

run_100:
	./bench -a 100 -m 2


plot_100:
	gnuplot 100_all_notimeout.gp
	gnuplot 100_polling_notimeout.gp	
