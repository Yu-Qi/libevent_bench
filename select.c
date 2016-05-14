/*
 * Copyright 2003 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Mon 03/10/2003 - Modified by Davide Libenzi <davidel@xmailserver.org>
 *
 *     Added chain event propagation to improve the sensitivity of
 *     the measure respect to the event loop efficency.
 *
 *
 */

#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
#define insertion_sort(){	\
	for(int i =0 ;i<100;i++){		\
		for(int j=i ; j>0 ;j--){	\
			if(d[j] <d[j-1]){	\
				SWAP(d[j],d[j-1]);	\
			}		\
			else			\
				break;		\
		}	\
	}		\
}

#define SWAP(a,b){	\
	tmp = a;	\
	a = b;		\
	b = tmp;	\
}

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define EPOLL 1
#define POLL 2
#define SELECT 3


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <event.h>
#include <event2/event.h>

#include <pthread.h>

static int count, writes, fired;
static int *pipes;
static int num_pipes, num_active, num_writes;
static int cur_num_fd;
static struct event **events;
static int timers;
static FILE *fout_a, *fout_p;
// 32700 / 500
static int times_all[100], times_polling[100], ave_all[400], ave_polling[400];



typedef struct thread_para{
	int start;
	int end;
} para;

int average(int d[]) {
	int tmp;
	insertion_sort();
	int total = 0;
	for (int i = 10; i < 90; i++)
		total += d[i];
	return total / 80;
}
void
read_cb(int fd, short which, void *arg)
{
	int idx = (int) arg, widx = idx + 1;
	u_char ch;

	if (timers)
	{
		struct timeval tv;
		event_del (events [idx]);
		tv.tv_sec  = 10;
		tv.tv_usec = drand48() * 1e6;
		event_add(events[idx], &tv);
	}

	count += read(fd, &ch, sizeof(ch));   //ch = 'e'  ,receive from write()
	//printf("fd: %d , fd2: %d\n",fd,(2*widx+1));
	if (writes) {			//left number how many clinets need to do write-IO
		if (widx >= num_pipes)
			widx -= num_pipes;
		write(pipes[2 * widx + 1], "e", 1);
		writes--;
		fired++;
	}
}

void *run_once( void *ptr)
/*
struct timeval *
run_once(int index)*/
{	
	struct event_config *cfg;
	struct event_base *base;
	cfg = event_config_new();


	event_config_avoid_method(cfg, "epoll");
	event_config_avoid_method(cfg, "poll");
	
	base = event_base_new_with_config(cfg);
	event_config_free(cfg);
	para *paras = ptr;
	printf("start %d 	end: %d\n",paras->start,paras->end);

	int *cp, i, space;
	static struct timeval ta, ts, te, tv;
	static struct timeval t1, t2, t3;
	gettimeofday(&ta, NULL);
	
	for(cp = pipes+2*paras->start, i = paras->start; i<=paras->end; i++, cp +=2 ){
		printf("check: %d\n",i );
		events[i] = event_new(base, cp[0], EV_READ | EV_PERSIST,  read_cb, (void *) i);
		event_add(events[i], NULL);
	}

	gettimeofday(&t1, NULL);

	//event_loop(EVLOOP_ONCE | EVLOOP_NONBLOCK);
	fired = 0;
	space = num_pipes / num_active;
	space = space * 2;
	for (i = 0; i < num_active; i++, fired++)
		write(pipes[i * space + 1], "e", 1);
	count = 0;
	writes = num_writes;
	gettimeofday(&t2, NULL);

	gettimeofday(&ts, NULL);
	do {
		//printf("count %d,fired %d\n",count,fired);
		event_base_loop(base,	EVLOOP_ONCE | EVLOOP_NONBLOCK);
	} while (count != fired);
	gettimeofday(&te, NULL);

	timersub(&t1, &ta, &t3);
	timersub(&te, &ta, &ta);
	timersub(&te, &ts, &ts);
	timersub(&te, &t1, &t1);
	timersub(&te, &t2, &t2);

	fprintf(stdout, "%8ld %8ld %8ld %8ld %8ld\n",  ta.tv_sec * 1000000L + ta.tv_usec,
	        ts.tv_sec * 1000000L + ts.tv_usec,
	        t1.tv_sec * 1000000L+ t1.tv_usec,
	        t2.tv_sec* 1000000L +  t2.tv_usec,
	        t3.tv_sec * 1000000L +t3.tv_usec
	        );
	//times_all[index] = ta.tv_sec * 1000000L + ta.tv_usec;
	//times_polling[index] = ts.tv_sec * 1000000L + ts.tv_usec;
	/*ave_all[index] = ta.tv_sec * 1000000L + ta.tv_usec;
	ave_polling[index] = ts.tv_sec * 1000000L + ts.tv_usec;*/
	for (int i = 0 ; i < num_pipes ; i++) {
		event_del(events[i]);
		event_free(events[i]);
	}
	return (&te);
}

int
main (int argc, char **argv)
{
	struct rlimit rl;
	int i, c;
	struct timeval *tv;
	int *cp;
	extern char *optarg;
	num_pipes = 100;
	num_active = 1;
	num_writes = num_pipes;
	cur_num_fd = 0;
	while ((c = getopt(argc, argv, "a:w:te")) != -1) {
		switch (c) {
		case 'a':
			num_active = atoi(optarg);
			break;
		case 'w':
			num_writes = atoi(optarg);
			break;
		case 't':
			timers = 1;
			break;
		default:
			fprintf(stderr, "Illegal argument \"%c\"\n", c);
			exit(1);
		}
	}

#if 1
	//rl.rlim_cur = rl.rlim_max = num_pipes * 2 + 50;
	rl.rlim_cur = rl.rlim_max =  RLIM_INFINITY;

	if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
		perror("setrlimit");
	}

#endif
	//cfg = event_config_new();


	fout_a = fopen("data/100_all_select.txt", "w");
	fout_p = fopen("data/100_polling_select.txt", "w");
	/*event_config_avoid_method(cfg, "epoll");
	event_config_avoid_method(cfg, "poll");
	
	base = event_base_new_with_config(cfg);
	event_config_free(cfg);*/

	//const char *theMethod = event_base_get_method(base);
	//printf("the actual method in use :  %s\n", theMethod);

	for ( i = 0; i < 100; i++) {
		ave_polling[i] = 0;
		ave_all[i] = 0;
	}
	pthread_t *pid;
	pid = malloc(sizeof(pthread_t)*2000);
	para *paras;
	paras = malloc(sizeof(para)*2000);
	int x=600;

	//for (int x = num_active , index = 0; x <= 98000 ; x += 500, index++) {
		num_pipes = x;
		events = calloc(num_pipes, sizeof(struct event*));
		pipes = calloc(num_pipes * 2, sizeof(int));
		if (events == NULL || pipes == NULL) {
			perror("malloc");
			exit(1);
		}

		for (cp = pipes, i = 0; i < num_pipes; i++, cp += 2) {
			if (socketpair(AF_UNIX, SOCK_STREAM, 0, cp) == -1) {
				perror("pipe");
				exit(1);
			}
		}
		printf("%d th\n", x);
		//	average the value to reduce error
		for(int index=0,j=0; index<num_pipes; index +=400, j++){
			puts("in");			
			paras[j].start = index;
			if(index +400 >= num_pipes)
				paras[j].end = num_pipes;
			else
				paras[j].end = index+399;
			printf("before s:%d e%d\n",paras[j].start,paras[j].end);
			pthread_create(&pid[j], NULL, run_once, &paras[j]);
			printf("%d th\n",j);
		}
		//for ( i = 0 ; i < 100; i++)
			//tv = run_once(i);
		for(int index=0,j=0; index<num_pipes; index +=500, j++)
			pthread_join(pid[0],NULL);
		//ave_all[index] = average(times_all);
		//ave_polling[index] = average(times_polling);


		for (cp = pipes, i = 0; i < num_pipes; i++, cp += 2) {
			shutdown(cp[0], 2);
			shutdown(cp[1], 2);
			close(cp[0]);
			close(cp[1]);
		}

		free(events);
		free(pipes);

	//}
		/*
	for (int x = num_active , index = 0; x <= 98000  ; x += 500, index++) {

		fprintf(fout_a, "%d %d\n", x,
		        ave_all[index]);
		fprintf(fout_p, "%d %d\n", x,
		        ave_polling[index]);
	}*/

	exit(0);
}



