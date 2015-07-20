/*****************************************************************************\
 *  controller.c - Central controller for Invasive resource management. 
 *****************************************************************************
 *  Copyright (C) 2015-2016 Nishanth Nagendra, Technical University of Munich.
\*****************************************************************************/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "slurm/slurm.h"
#include "slurm/slurm_errno.h"

#include "src/common/list.h"
#include "src/common/macros.h"
#include "src/common/node_select.h"
#include "src/common/parse_time.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

#include "src/slurmirmd/slurmirmd.h"

#ifndef BACKFILL_INTERVAL
#  define BACKFILL_INTERVAL	10
#endif

/*********************** local variables *********************/
static bool stop_agent = false;
static pthread_mutex_t term_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  term_cond = PTHREAD_COND_INITIALIZER;
//static bool config_flag = false;
static int irm_interval = BACKFILL_INTERVAL;
//static int max_sched_job_cnt = 50;
//static int sched_timeout = 0;

/*********************** local functions *********************/
//static void _compute_start_times(void);
static void _load_config(void);
static void _my_sleep(int secs);
static int _init_comm(void);

/* Terminate ischeduler_agent */
extern void stop_irm_agent(void)
{
	pthread_mutex_lock(&term_lock);
	stop_agent = true;
        printf("\nStopping IRM agent\n");
	pthread_cond_signal(&term_cond);
	pthread_mutex_unlock(&term_lock);
}

static void _my_sleep(int secs)
{
	struct timespec ts = {0, 0};
	struct timeval now;

	gettimeofday(&now, NULL);
	ts.tv_sec = now.tv_sec + secs;
	ts.tv_nsec = now.tv_usec * 1000;
	pthread_mutex_lock(&term_lock);
	if (!stop_agent)
		pthread_cond_timedwait(&term_cond, &term_lock, &ts);
	pthread_mutex_unlock(&term_lock);
}

static void _load_config(void)
{
/*	char *sched_params, *select_type, *tmp_ptr;

	sched_timeout = slurm_get_msg_timeout() / 2;
	sched_timeout = MAX(sched_timeout, 1);
	sched_timeout = MIN(sched_timeout, 10);

	sched_params = slurm_get_sched_params();

	if (sched_params && (tmp_ptr=strstr(sched_params, "interval=")))
		builtin_interval = atoi(tmp_ptr + 9);
	if (builtin_interval < 1) {
		error("Invalid SchedulerParameters interval: %d",
		      builtin_interval);
		builtin_interval = BACKFILL_INTERVAL;
	}

	if (sched_params && (tmp_ptr=strstr(sched_params, "max_job_bf=")))
		max_sched_job_cnt = atoi(tmp_ptr + 11);
	if (sched_params && (tmp_ptr=strstr(sched_params, "bf_max_job_test=")))
		max_sched_job_cnt = atoi(tmp_ptr + 16);
	if (max_sched_job_cnt < 1) {
		error("Invalid SchedulerParameters bf_max_job_test: %d",
		      max_sched_job_cnt);
		max_sched_job_cnt = 50;
	}
	xfree(sched_params);

	select_type = slurm_get_select_type();
	if (!strcmp(select_type, "select/serial")) {*/
		/* Do not spend time computing expected start time for
		 * pending jobs */
/*		max_sched_job_cnt = 0;
		stop_builtin_agent();
	}
	xfree(select_type);*/
}

//Connect to iRM daemon via a TCP connection
static int _init_comm(void) {
   slurm_fd_t fd = -1;
   slurm_addr_t addr;
   uint16_t port = 12345;
   char *host = "127.0.0.1";

   slurm_set_addr(&addr, port, host);
   fd = slurm_init_msg_engine(&addr);

   if (fd < 0) {
      printf("\n[IRM_DAEMON]: Failed to initialize communication engine. Dameon will shutdown shortly\n");
      return -1;
   }
   printf("\n[IRM_DAEMON]: Successfully initialized communication engine\n");
   return fd;
}

/* Note that slurm.conf has changed */
/*extern void irm_reconfig(void)
{
	config_flag = true;
}*/

/* irm daemon */
int main(int argc, char *argv[])
{
	time_t now;
	double wait_time;
	static time_t last_mapping_time = 0;
        slurm_fd_t fd = -1;
        slurm_fd_t client_fd = -1;
        char *buf = NULL;
        int ret_val;
        int timeout = 10 * 1000;   // 30 secs converted to millisecs
        slurm_addr_t cli_addr;
        //pthread_attr_t attr;
	/* Read config, nodes and partitions; Write jobs */
	//slurmctld_lock_t all_locks = {
	//	READ_LOCK, WRITE_LOCK, READ_LOCK, READ_LOCK };

        buf = (char *)malloc(sizeof(int));

        printf("\n[IRM_DAEMON]: Entering irm_agent\n");
        printf("\n[IRM_DAEMON]: Attempting to connect to iRM Daemon\n");

        fd = _init_comm();

        if (fd == -1) { 
           printf("\n[IRM_DAEMON]: Unsuccessful initialization of communication engine. Agent shutting down\n");
           return 0;
        }

        client_fd = slurm_accept_msg_conn(fd, &cli_addr);
        if (client_fd != SLURM_SOCKET_ERROR) {
           printf("\n[IRM_DAEMON]: Accepted connection from iScheduler. Communications can now start\n");
        } else {
           printf("\n[IRM_DAEMON]: Unable to receive any connection request from iScheduler. Shutting down the daemon.\n");
           stop_agent = true;
        }

	_load_config();

	last_mapping_time = time(NULL);
	while (!stop_agent) {
		_my_sleep(irm_interval);
		if (stop_agent)
			break;
		/*if (config_flag) {
			config_flag = false;
			_load_config();
		}*/
		now = time(NULL);
		wait_time = difftime(now, last_mapping_time);
		if ((wait_time < irm_interval))
			continue;
                printf("\nCreating a new resource offer to send to iScheduler\n");
                sleep(5);
                *buf = 1;
                ret_val = _slurm_send_timeout(client_fd, buf, sizeof(int), 0, timeout);
                if (ret_val < 4) {
                   printf("\n[IRM_DAEMON]: Did not send correct number of bytes\n");
                   printf("\n[IRM_DAEMON]: iRM Daemon closing\n");
                   break;
                }

                printf("[IRM_DAEMON]: Sent the offer. Waiting for a mapping from jobs to this offer\n");
		//lock_slurmctld(all_locks);
                
                //printf("\n***************[iRM AGENT]****************\n");
                //printf("\nReceived a resource offer from iRM\n");
                //printf("\nProcessing the offer for mapping jobs in the Invasic queue to this offer\n");
 
                ret_val = _slurm_recv_timeout(client_fd, buf, sizeof(int), 0, timeout);
                if (ret_val < 4) {
                   printf("\n[IRM_DAEMON]: Did not receive correct number of bytes\n");
                   printf("\n[IRM_DAEMON]: iRM Daemon closing\n");
                   break;
                }
                printf("\nReceived a mapping from jobs to offer from iScheduler. Processing the same\n");
		//_compute_start_times();
		last_mapping_time = time(NULL);
		//unlock_slurmctld(all_locks);
	}
        free(buf);
        close(client_fd);
        close(fd);
        printf("\n[IRM_DAEMON]: Exiting iRM Daemon\n");
	return 0;
}
