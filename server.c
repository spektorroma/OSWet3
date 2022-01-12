#include "segel.h"
#include "request.h"

// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// HW3: Parse the new arguments too
void getargs(int *port, int *worker, int *buffer, char **handler, int argc, char *argv[])
{
    if (argc < 2) {
	fprintf(stderr, "Usage: %s <port>\n", argv[0]);
	exit(1);
    }
    *port = atoi(argv[1]);
    *worker = atoi(argv[2]);
    *buffer = atoi(argv[3]);
    *handler = argv[4];
}




int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen, worker, buffer;
    char *handler;
    struct sockaddr_in clientaddr;

    getargs(&port, &worker, &buffer, &handler, argc, argv);

    // 
    // HW3: Create some threads...
    //
    pool_status_t *pool_status;
    pthread_t  thread;
    
    pool_status = calloc(1, sizeof(*pool_status));

    pthread_mutex_init(&(pool_status->request_mutex), NULL);
    pthread_cond_init(&(pool_status->pool_cond), NULL);
    pthread_cond_init(&(pool_status->block_cond), NULL);
    
    pool_status->first = NULL;
    pool_status->last  = NULL;
    pool_status->func = *requestHandle;
    pool_status->size = 0;
    pool_status->buffer = buffer;
    
    for (size_t i=0; i<worker; i++) {
        pthread_create(&thread, NULL, thread_func, pool_status);
    }
    
    listenfd = Open_listenfd(port);
    while (1) {
        pool_t *pool;
	clientlen = sizeof(clientaddr);
	pthread_mutex_lock(&(pool_status->request_mutex));
	if (pool_status->size == buffer){
	    if (handler[1] == 'h'){
		get_pool(pool_status);
	    }
	    if (handler[1] == 't'){}
	    if (handler[0] == 'b'){
		pthread_cond_wait(&(pool_status->block_cond), &(pool_status->request_mutex));
	    }
            if (handler[0] == 'r'){
		int random, buffer_copy;
		buffer_copy = buffer - 1;
		while(buffer_copy > buffer/2){
		    random = rand() % buffer_copy;
		    get_pool_by_num(pool_status, random);
		    buffer_copy--;
		}
		int max = buffer - 1;
		random = rand() % max;
	    }
        }
	pthread_mutex_unlock(&(pool_status->request_mutex));
	connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
	pool = pool_create(connfd);
   

        pthread_mutex_lock(&(pool_status->request_mutex));
        
        if (pool_status->first == NULL) {
            pool_status->first = pool;
            pool_status->last = pool_status->first;
        } else {
            pool_status->last->next = pool;
            pool_status->last = pool;
        }
        pool_status->size++;

        pthread_cond_broadcast(&(pool_status->pool_cond));
        pthread_mutex_unlock(&(pool_status->request_mutex));
	// 
	// HW3: In general, don't handle the request in the main thread.
	// Save the relevant info in a buffer and have one of the worker threads 
	// do the work. 
	// 
	
    }

}


    


 
