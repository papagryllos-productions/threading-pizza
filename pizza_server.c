/*
 * George Papanikolaou - Prokopis Gryllos
 * Operating Systems Project 2012 - Pizza Delivery
 * There is absolutely no warranty
 *
 * status1 --- status2:
 *    0           0   : Pending
 *    0           1   : Cooking
 *    1           0   : Cooked
 *    1           1   : Delivering
 *
 * false = 0
 * true  = 1
 */

#include "pizza.h"
#include <pthread.h>
#include <time.h>
#include <semaphore.h>
#include <sys/stat.h>

/* ========== Globals ========== */
/* the main list for the orders in global scope */
order_t order_list[LIMIT];
/* mutexes (and static initialization) */
pthead_mutex_t list_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthead_mutex_t cook_mutex     = PTHREAD_MUTEX_INITIALIZER;
pthead_mutex_t delivery_mutex = PTHREAD_MUTEX_INITIALIZER;
/* condition variables (and static initialization) */
pthread_cond_t cook_cond     = PTHREAD_COND_INITIALIZER;
pthread_cond_t delivery_cond = PTHREAD_COND_INITIALIZER;
/* global counters */
short cookers = N_PSISTES;
short delivery_guys = N_DIANOMEIS;

/* ========== Functions ========== */
/* A function to display an error message and then exit */
void fatal(char *message) {
    fprintf(stderr, "\a!! - Fatal error - ");
    fprintf(stderr, "%s\n", message);
    /* also writing to a logfile */
    FILE *fd;
    time_t raw_time;
    time(&raw_time);
    fd = fopen("logfile", "a");
    fprintf(fd, "!!! Fatal error: %s --- %s", message, ctime(&raw_time));
    fclose(fd);
    exit(EXIT_FAILURE);
}

/* The function to log messages from everywhere */
void log(char *message) {
    FILE *fd;
    time_t raw_time;
    time(&raw_time);
    fd = fopen("logfile", "a");
    fprintf(fd, "[%d] --- %s --- %s", (int)getpid(), message, ctime(&raw_time));
    fclose(fd);
}

/* The waiting function for the delivery */
void delivering(bool d) {
    if (d == false)
        usleep(T_KONTA);
    else if (d == true)
        usleep(T_MAKRIA);
    else
        fatal("Wrong input on delivery function");
}

/* Thread function for complete individual order handling */
void* order_handling(void* incoming) {
    /* variables for elapsed time counting */
    struct timeval begin, end;
    gettimeofday(&begin, NULL);
    /* initialization for the coca cola process */
    incoming.start_sec = begin.tv_sec; 
    incoming.start_usec = begin.tv_usec;

    /* Try to find a place in the order_list array */
    short local = 0;
	/* synchronization with mutex */
	pthread_mutex_lock(&list_mutex);
    while (order_list[local].exists == true)
        local++;
    order_list[local] = incoming;
	/* done */
	pthread_mutex_unlock(&list_mutex);

    /* new thread id for the sub-threads (max = pizza counter) */
    pthread_t sub_id[counter];
	/* for pizza identification on the cook() function */
    char pizza_type = 'n';

    /* distribute pizzas in sub-threads */
    short j=0;
    while (order_list[local].m_num != 0) {
        (order_list[local].m_num)--;
        /* Margarita type */
        pizza_type = 'm';
        if (pthread_create(&sub_id[j], NULL, &cook, &pizza_type) != 0)
			fatal("Failed to create 'cooking' thread");
        /* increment the special counter */
        j++;
    }	
    while (order_list[local].p_num != 0) {
        (order_list[local].p_num)--;
        /* Pepperoni */
        pizza_type = 'p';
		if (pthread_create(&sub_id[j], NULL, &cook, &pizza_type) != 0)
			fatal("Failed to create 'cooking' thread");
		/* increment the special counter */
        j++;
    }
    while (order_list[local].s_num != 0) {
        (order_list[local].s_num)--;
        /* Special */
        pizza_type = 's';
		if (pthread_create(&sub_id[j], NULL, &cook, &pizza_type) != 0)
			fatal("Failed to create 'cooking' thread");
        /* increment the special counter */
        j++;
    }

    /* set "cooking" status */
    order_list[local].status2 = true;

	/* wait for pizzas to get ready */
    for (j; j>0; j--) {
        if (pthread_join(sub_id[j], NULL) != 0)
            fatal("Thread failed to join");
    }
    
    /* set "cooked" status */
    order_list[local].status1 = true;
    order_list[local].status2 = false;

    log("ready for delivery");

	/* get the delivery guy */
	if (delivery_guys == 0) {
		/* if noone is available, wait */
		pthread_cond_wait(&delivery_cond, &delivery_mutex);
		/* take the guy */
		delivery_guys--;
	} else {
		/* take the guy */
		delivery_guys--;
	}
	/* actually delivering */
	delivering(order_list[local].time);
	
	/* and give him back */
	delivery_guys++;
	pthread_cond_signal(&delivery_cond, &delivery_mutex);
	/* ...done */

    /* log time */
    gettimeofday(&end,NULL);
    FILE *fd;
    fd = fopen("logfile", 'a');
    fprintf(fd, "[%d] -^- elapsed time: %ld seconds and %ld microseconds \n",
          getpid(), (end.tv_sec -begin.tv_sec ),(end.tv_usec - begin.tv_usec));
    fclose(fd);

    /* delete the order */
    order_list[local].exists = false;

    /* exit successfully */
    pthread_exit(0);
}

/* Thread function for cooking individual pizzas */
void* cook(void* pizza_type) {
    log("ready to get cooked");

	/* using condition variable to get the cooker */
	if (cookers == 0) {
		pthread_cond_wait(&cook_cond, &cook_mutex);
		/* take the cooker */
		cookers--;
	} else {
		cookers--;
	}

	/* we have the cooker. Actually cook (wait) */
    if (*pizza_type == 'm')
        usleep(TIME_MARGARITA);
		/* TODO: pthread_cond_timewait() */
    else if (*pizza_type == 'p')
        usleep(TIME_PEPPERONI);
    else if (*pizza_type == 's')
        usleep(TIME_SPECIAL);
    else
        fatal("Wrong input on cook function");

    log("cooked");

	/* give the cooker back */
	cookers++;
	pthread_cond_signal(&cook_cond, &cook_mutex);

	/* bail out to join with parent thread */
    pthread_exit(0);
}

/* Coca-Cola handling function (to a single independent thread) */
void* coca_cola(void* unused) {
    /* set variable in order to test elapsed time of order */
    struct timeval test;

	/* we are gonna need to work without alarm() since it's signal-based
	 * and signals are per-process (we don't want to stop the whole
	 * program when the coca_cola handler kicks in) */

    /* scan the order lists */
    while(true) {
        /* get current test time */	
        gettimeofday(&test, NULL);

        int j = 0;
		/* while ??? */
        while (order_list[j].exists == true) {
            j++;
            /* substract order time from current time to get elapsed time */
            if ( (test.tv_sec - order_list2->start_sec)* 1000000 +
                    (test.tv_usec - order_list2->start_usec) >= 3000) {
                /* open file to write into */
                FILE *coke;
                coke = fopen("logfile", 'a');

                fprintf(coke,"### coca cola for [%d] order. Elapsed time:\
                        (%ld seconds and %ld microseconds)\n",
                        order_list2->mypid,
                        (test.tv_sec - order_list2->start_sec),
                        (test.tv_usec - order_list2->start_usec));
                fclose(coke);
            }
            order_list2++;
        }
        /* TODO: Change this */
        usleep(T_VERYLONG);
    }
}

int main() {
    /* thread type declarations */
    pthread_t id[LIMIT];
    /* thread for coca-cola handling */
    pthread_t colas;
    /* temporary place for incoming data */
    order_t incoming;
    /* thread id counter */
    int i;
    /* socket file descriptors */
    int sd, new_conn;

    /* unix socket address declarations and lengths */
    struct sockaddr_un server_addr, client_addr;
    socklen_t addr_len;

    /* ============= END OF DECLARATIONS ================ */

    /* Fork off the parent process to get into deamon mode (background) */
    pid = fork();
    if (pid == -1)
        fatal("Can't fork parent");
    if (pid > 0) {
        /* Stopping the parent proccess and just keeping the child */
        printf(">> Deamon mode <<\n");
        printf("Use 'make kill' to kill the server\n");
        exit(EXIT_SUCCESS);
    }

    /* Socket business */
    sd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (sd == -1)
        fatal("while creating server's socket");
    unlink(PATH);
    /* socket internal information --- Maybe: AF_LOCAL */
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, PATH);
    /* bind function call with typecasted arguments of server address */
    if (bind (sd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        fatal("while binding");
    if (listen(sd, QUEUE) == -1)
        fatal("while listening");

    if (pthread_create(&colas, NULL, &coca_cola, NULL) != 0 )
        fatal("Failed to create coca-colas handling thread");
    
    /* endless loop to get new connections */
    i=0;
    while (true) {
        /* get order via socket */
        addr_len = sizeof(struct sockaddr_un);
        /* getting new connections from the client socket */
        new_conn = accept(sd, (struct sockaddr *) &client_addr, &addr_len);
        read(new_conn, &incoming, sizeof(incoming));
        /* required variable for memory organization */
        incoming.exists = true;
        /* close connection with this client */
        close(new_conn);

        /* Threading */
        if (pthread_create(&id[i], NULL, &order_handling, &incoming) != 0)
			fatal("Failed to create basic order handling thread");

        /* required action to counter (increment or zero) */
        if (i < LIMIT)
            i++;
        else 
			/* (the LIMIth+1 order must not take the place of 1st order
			 * if 1st order is not finished) */
            i=0;
    }
}
