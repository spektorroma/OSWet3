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

struct pool {
    int request;
    struct timeval *arrival_time;
    struct pool *next;
};
typedef struct pool pool_t;

struct pool_status {
    pool_t *first;
    pool_t *last;
    pthread_mutex_t request_mutex;
    pthread_cond_t pool_cond;
    pthread_cond_t block_cond;
    
    int size;
    int buffer;
};
typedef struct pool_status pool_status_t;

struct thread_stats {
    int handler_thread_id;
    int handler_thread_req_count;
    int handler_thread_static_req_count;
    int handler_thread_dynamic_req_count;    
};
typedef struct thread_stats thread_stats_t;

struct stats {
    struct timeval *arrival_time;
    struct timeval *dispatch_time;
    thread_stats_t *handler_thread_stats;
    pool_status_t *pool_status;
};
typedef struct stats stats_t;


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


pool_t *pool_create(int request, struct timeval *arrival_time){
    pool_t *pool;

    pool = malloc(sizeof(*pool));
    pool->request = request;
    pool->arrival_time = arrival_time;
    pool->next = NULL;
    return pool;
}

void pool_destroy(pool_t *pool)
{
    if (pool == NULL)
        return;
    free(pool);
}

pool_t *get_pool(pool_status_t *request_pool)
{
    pool_t *pool;

    pool = request_pool->first;
    if (pool == NULL)
        return NULL;

    if (pool->next == NULL) {
        request_pool->first = NULL;
        request_pool->last  = NULL;
    } else {
        request_pool->first = pool->next;
    }
    if (request_pool->size == request_pool->buffer){
	  pthread_cond_signal(&(request_pool->block_cond));
    }
    request_pool->size--;
    return pool;
}

pool_t *get_pool_by_num(pool_status_t *request_pool, int num)
{
    pool_t *pool, *trash;

    pool = request_pool->first;
    if (num == 0){
	request_pool->first = pool->next;
	if (pool->next == NULL)
	   request_pool->last = NULL;
        return pool;
    }
    while (num > 1){
	pool = pool->next;
	num--;
    }
    trash = pool->next;
    pool->next = trash->next;
    if (trash->next == NULL){
	request_pool->last = NULL;
    }
    request_pool->size--;
    return pool;
}


void *thread_func(void *arg){
    stats_t *stats = arg;
    pool_t *pool;
    struct timeval dispatch_time;
    
    while(1){
	pthread_mutex_lock(&(stats->pool_status->request_mutex));
	
	while(stats->pool_status->first == NULL)
	    pthread_cond_wait(&(stats->pool_status->pool_cond), &(stats->pool_status->request_mutex));
	pool = get_pool(stats->pool_status);
	pthread_mutex_unlock(&(stats->pool_status->request_mutex));
	if(pool != NULL){
	    gettimeofday(&dispatch_time, NULL);
            stats->dispatch_time = &dispatch_time;
            stats->arrival_time = pool->arrival_time;
	    requestHandle(pool->request, (void *)stats);
            Close(pool->request);
	    pool_destroy(pool);
	}
	
    }
}

thread_stats_t *creat_thread_stats(int id){
    thread_stats_t *stats;
    stats = malloc(sizeof(*stats));
    stats->handler_thread_id = id;
    stats->handler_thread_req_count = 0;
    stats->handler_thread_static_req_count = 0;
    stats->handler_thread_dynamic_req_count = 0;

    return stats;
}

stats_t *creat_stats(thread_stats_t *thread_stats, pool_status_t *pool_status){
    stats_t *stats;
    stats = malloc(sizeof(*stats));
    stats->arrival_time = NULL;
    stats->dispatch_time = NULL;
    stats->handler_thread_stats = thread_stats;
    stats->pool_status = pool_status;

    return stats;
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
    pool_status->size = 0;
    pool_status->buffer = buffer;
    
    stats_t *stats;
    thread_stats_t *thread_stats;
    struct timeval arrival_time;
    
    for (int i=0; i<worker; i++) {
	thread_stats = creat_thread_stats(i);
        stats = creat_stats(thread_stats, pool_status);
        pthread_create(&thread, NULL, thread_func, stats);
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
	gettimeofday(&arrival_time, NULL);
	if (handler[1] == 't' && pool_status->size == buffer){
	    continue;
	}
	pool = pool_create(connfd, &arrival_time);
   

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


    


 
