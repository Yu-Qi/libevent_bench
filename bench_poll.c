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

#define timersub(tvp, uvp, vvp)           \
  do {                \
    (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;    \
    (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec; \
    if ((vvp)->tv_usec < 0) {       \
      (vvp)->tv_sec--;        \
      (vvp)->tv_usec += 1000000;      \
    }             \
  } while (0)

#define insertion_sort(){ \
  for(int i =0 ;i<1000;i++){    \
    for(int j=i ; j>0 ;j--){  \
      if(d[j] <d[j-1]){  \
        SWAP(d[j],d[j-1]);}  \
      else      \
        break;    \
    } \
  }   \
}

#define SWAP(a,b){  \
  tmp = a;  \
  a = b;    \
  b = tmp;  \
}

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#include <sys/poll.h>


static int count, writes, fired;
static int *pipes;
static int num_pipes, num_active, num_writes, run_mode;
static int timers, native;
static struct pollfd *ufds;   //default value , if fd >1024 ,then using dynamic array
static FILE *fout_a, *fout_p;
static long time_all[1000], time_polling[1000], buffer_all[1000], buffer_polling[1000];


long  average(long d[]) {
  long tmp;
  insertion_sort();
  long total = 0;
  for (int i = 100; i < 900; i++)
    total += d[i];
  return total / 800;
}

void
read_cb(int fd, int idx)
{
  int widx = idx + 1;
  u_char ch;
  /*struct timeval tv;

  if (timers)
  {
    tv.tv_sec  = 10;
    tv.tv_usec = drand48() * 1e6;
  }*/

  count += read(fd, &ch, sizeof(ch));   //ch = 'e'  ,receive from write()
  //printf("fd: %d , fd2: %d\n",fd,(2*widx+1));
  if (writes) {     //left number how many clinets need to do write-IO
    if (widx >= num_pipes)
      widx -= num_pipes;
    write(pipes[2 * widx + 1], "e", 1);
    writes--;
    fired++;
  }
}

struct timeval *
run_once(int index)
{

  int *cp, i, space;
  static struct timeval ta, ts, te, tv;

  gettimeofday(&ta, NULL);
  /*struct timeval tv;
  if(timers){
    tv.tv_sec  = 10;
    tv.tv_usec = drand48() * 1e6;
  }*/

  for (cp = pipes, i = 0; i < num_pipes; i++, cp += 2) {
    //the even index of cp equls socketpair[0],that is used to read

    ufds[i].fd = cp[0];   //set FD into poll's FD sturcture
    ufds[i].events = POLLIN | POLLPRI;    //set the condition when event is active

  }
  //event_loop(EVLOOP_ONCE | EVLOOP_NONBLOCK);
  //為什麼這裡需要一次wait

  fired = 0;
  space = num_pipes / num_active;
  space = space * 2;
  for (i = 0; i < num_active; i++, fired++)
    write(pipes[i * space + 1], "e", 1);
  count = 0;
  writes = num_writes;

  gettimeofday(&ts, NULL);
  do {
    //printf("count %d,fired %d\n",count,fired);

    int rv = poll(ufds, num_pipes,  timers ? &tv : 0 );  //third parameter is timeout
    if (rv == -1) {
      perror("poll"); // error occurred in poll()
    } else if (rv == 0) {
      printf("Timeout occurred!  No data after 3.5 seconds.\n");
    } else {
      for (int i = 0 ; i < num_pipes ; i++) {

        // check for events
        if (ufds[i].revents & POLLIN) {
          read_cb(ufds[i].fd , i);
        }
        /*
        if (ufds[i].revents & POLLPRI) {
            recv(ufds[i], buf1, sizeof buf1, MSG_OOB); // out-of-band data
        }*/
      }
    }
    // puts("  loop");
  } while (count != fired);
  gettimeofday(&te, NULL);

  timersub(&te, &ta, &ta);
  timersub(&te, &ts, &ts);
  time_all[index] = ta.tv_sec * 1000000L + ta.tv_usec;
  time_polling[index] = ts.tv_sec * 1000000L + ts.tv_usec;

  return (&te);
}

int main (int argc, char **argv)
{
  struct rlimit rl;
  int i, c;
  struct timeval *tv;
  int *cp;
  extern char *optarg;

  num_pipes = 100;
  num_active = 1;
  num_writes = num_pipes;
  while ((c = getopt(argc, argv, "r:n:a:w:te")) != -1) {
    switch (c) {
    case 'r':
      run_mode = atoi(optarg);
      num_active = run_mode;
      break;
    case 'n':
      num_pipes = atoi(optarg);
      break;
    case 'a':
      num_active = atoi(optarg);
      break;
    case 'w':
      num_writes = atoi(optarg);
      break;
    case 'e':
      native = 1;
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
  rl.rlim_cur = rl.rlim_max = 65536; //RLIM_INFINITY;
  if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
    perror("setrlimit");
  }
#endif


  fout_a = fopen("100_all_poll.txt", "w");
  fout_p = fopen("100_polling_poll.txt", "w");
  puts("it's the poll version");
  for ( i = 0; i < 1000; i++) {
    buffer_polling[i] = 0;
    buffer_all[i] = 0;
  }
  for (int x = run_mode , index = 0; x <= 32700 ; x += 500, index++) {
    num_pipes = x;

    pipes = calloc(num_pipes * 2, sizeof(int));
    ufds = (struct pollfd*) malloc (sizeof(struct pollfd) * num_pipes);
    if (pipes == NULL || ufds == NULL) {
      perror("malloc ");
      exit(1);
    }

    for (cp = pipes, i = 0; i < num_pipes; i++, cp += 2) {
      if (socketpair(AF_UNIX, SOCK_STREAM, 0, cp) == -1) {

        perror("pipe");
        exit(1);
      }
    }
    for ( i = 0 ; i < 1000; i++)
      tv = run_once(i);
    buffer_all[index] = average(time_all);
    buffer_polling[index] = average(time_polling);

    printf("%d th\n", x);

    for (cp = pipes, i = 0; i < num_pipes; i++, cp += 2) {
      shutdown(cp[0], 2);
      shutdown(cp[1], 2);
      close(cp[0]);
      close(cp[1]);
    }
    free(ufds);
    free(pipes);
  } 
  for (int x = run_mode , index = 0; x <= 32700 ; x += 500, index++) {

    fprintf(fout_a, "%d %ld\n",
            x,
            buffer_all[index]
           );
    fprintf(fout_p, "%d %ld\n",
            x,
            buffer_polling[index]
           );
  }
  return 0;
}
