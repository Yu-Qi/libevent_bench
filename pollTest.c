#include <sys/poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <event.h> 
#include <event2/event.h>

struct event_config *cfg;
struct event_base *base;
struct event *ev1;
struct event *ev2;

void cb_func2(evutil_socket_t fd, short what, void *arg){
        char buf1[256];
        for(int i=0;i<256;i++) buf1[i] = '0';
        int n;
        n = read(fd,buf1,256);
        for(int i=0;i<n;i++) printf("%c",buf1[i]);
        printf("\n");

}

void cb_func1(evutil_socket_t fd, short what, void *arg)
{
        printf("Got an event on socket %d:%s%s%s%s\n",
            (int) fd,
            (what&EV_TIMEOUT) ? " timeout" : "",
            (what&EV_READ)    ? " read" : "",
            (what&EV_WRITE)   ? " write" : "",
            (what&EV_SIGNAL)  ? " signal" : "");

        struct sockaddr_in client;
        int connfd,clnlen;
     
        clnlen=sizeof(client);
        connfd=accept(fd,(struct sockaddr *)&client,&clnlen);
        if (connfd <0){ 
            perror("accept");
            return;
        } 

        ev2 = event_new(base, connfd, EV_READ, cb_func2, NULL);     
        event_add(ev2, NULL);
        event_base_dispatch(base);
}

int main(){
    struct sockaddr_in serv1;
    int s1fd;
       
    memset(&serv1,0,sizeof(serv1));
    serv1.sin_family=AF_INET;
    serv1.sin_addr.s_addr=inet_addr("127.0.0.1");
    serv1.sin_port=htons(8080);

    s1fd = socket(PF_INET, SOCK_STREAM, 0);
    if (s1fd==-1)
    {
        perror("sockfd:s1fd");
        return 0;
    }

    setsockopt(s1fd, SOL_SOCKET, SO_REUSEADDR, (struct sockaddr *)&serv1, sizeof(serv1)); // reuse address  
    if (bind(s1fd,(struct sockaddr *)&serv1,sizeof(serv1))==-1)
    {
        perror("bind:s1fd");
        return 0;
    }

    if (listen(s1fd,1024)==-1)
    {
        perror("listen:s1fd");
        return 0;
    }   

    int i;
    const char **methodsList = event_get_supported_methods();
    printf("Starting Libevent %s.  Available methods are:\n",event_get_version());
    for (i=0; methodsList[i] != NULL; ++i) {
        printf("    %s\n", methodsList[i]);
    }

    cfg = event_config_new();
    event_config_avoid_method(cfg, "epoll");
    event_config_avoid_method(cfg, "select");
    base = event_base_new_with_config(cfg);
    event_config_free(cfg);    
    const char *theMethod = event_base_get_method(base);
    printf("the actual method in use :  %s\n",theMethod);

    ev1 = event_new(base, s1fd, EV_READ, cb_func1, NULL);
    event_add(ev1, NULL);
    event_base_dispatch(base);
    return 0;
}
