/*****************************************************************************\
 *  irm_agent.c - Agent which is responsible for talking to Invasive resource 
 *                Manager and for scheduling of jobs from Invasive queue.
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

#include "src/slurmctld/locks.h"
#include "src/slurmctld/preempt.h"
#include "src/slurmctld/reservation.h"
#include "src/slurmctld/slurmctld.h"
#include "src/plugins/sched/ischeduler/ischeduler.h"

#ifndef BACKFILL_INTERVAL
#  define BACKFILL_INTERVAL	10
#endif

#define DONT_EXECUTE 1

/*********************** local variables *********************/
static bool stop_agent = false;
static pthread_mutex_t term_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  term_cond = PTHREAD_COND_INITIALIZER;
static bool config_flag = false;
static int irm_interval = BACKFILL_INTERVAL;
//static int max_sched_job_cnt = 50;
//static int sched_timeout = 0;

/*********************** local functions *********************/
//static void _compute_start_times(void);
static void _load_config(void);
static void _my_sleep(int secs);
static int _connect_to_irmd(void);

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

//static void _compute_start_times(void)
//{
/*	int j, rc = SLURM_SUCCESS, job_cnt = 0;
	List job_queue;
	job_queue_rec_t *job_queue_rec;
	List preemptee_candidates = NULL;
	struct job_record *job_ptr;
	struct part_record *part_ptr;
	bitstr_t *alloc_bitmap = NULL, *avail_bitmap = NULL;
	bitstr_t *exc_core_bitmap = NULL;
	uint32_t max_nodes, min_nodes, req_nodes, time_limit;
	time_t now = time(NULL), sched_start, last_job_alloc;
	bool resv_overlap = false;

	sched_start = now;
	last_job_alloc = now - 1;
	alloc_bitmap = bit_alloc(node_record_count);
	job_queue = build_job_queue(true, false);
	sort_job_queue(job_queue);
	while ((job_queue_rec = (job_queue_rec_t *) list_pop(job_queue))) {
		job_ptr  = job_queue_rec->job_ptr;
		part_ptr = job_queue_rec->part_ptr;
		xfree(job_queue_rec);
		if (part_ptr != job_ptr->part_ptr)
			continue;*/	/* Only test one partition */

	/*	if (job_cnt++ > max_sched_job_cnt) {
			debug2("scheduling loop exiting after %d jobs",
			       max_sched_job_cnt);
			break;
		} */

		/* Determine minimum and maximum node counts */
		/* On BlueGene systems don't adjust the min/max node limits
		   here.  We are working on midplane values. */
	/*	min_nodes = MAX(job_ptr->details->min_nodes,
				part_ptr->min_nodes);

		if (job_ptr->details->max_nodes == 0)
			max_nodes = part_ptr->max_nodes;
		else
			max_nodes = MIN(job_ptr->details->max_nodes,
					part_ptr->max_nodes);

		max_nodes = MIN(max_nodes, 500000); */    /* prevent overflows */

	/*	if (job_ptr->details->max_nodes)
			req_nodes = max_nodes;
		else
			req_nodes = min_nodes;

		if (min_nodes > max_nodes) {*/
			/* job's min_nodes exceeds partition's max_nodes */
	/*		continue;
		}

		j = job_test_resv(job_ptr, &now, true, &avail_bitmap,
				  &exc_core_bitmap, &resv_overlap);
		if (j != SLURM_SUCCESS) {
			FREE_NULL_BITMAP(avail_bitmap);
			FREE_NULL_BITMAP(exc_core_bitmap);
			continue;
		}

		rc = select_g_job_test(job_ptr, avail_bitmap,
				       min_nodes, max_nodes, req_nodes,
				       SELECT_MODE_WILL_RUN,
				       preemptee_candidates, NULL,
				       exc_core_bitmap);
		if (rc == SLURM_SUCCESS) {
			last_job_update = now;
			if (job_ptr->time_limit == INFINITE)
				time_limit = 365 * 24 * 60 * 60;
			else if (job_ptr->time_limit != NO_VAL)
				time_limit = job_ptr->time_limit * 60;
			else if (job_ptr->part_ptr &&
				 (job_ptr->part_ptr->max_time != INFINITE))
				time_limit = job_ptr->part_ptr->max_time * 60;
			else
				time_limit = 365 * 24 * 60 * 60;
			if (bit_overlap(alloc_bitmap, avail_bitmap) &&
			    (job_ptr->start_time <= last_job_alloc)) {
				job_ptr->start_time = last_job_alloc;
			}
			bit_or(alloc_bitmap, avail_bitmap);
			last_job_alloc = job_ptr->start_time + time_limit;
		}
		FREE_NULL_BITMAP(avail_bitmap);
		FREE_NULL_BITMAP(exc_core_bitmap);

		if ((time(NULL) - sched_start) >= sched_timeout) {
			debug2("scheduling loop exiting after %d jobs",
			       max_sched_job_cnt);
			break;
		}
	}
	list_destroy(job_queue);
	FREE_NULL_BITMAP(alloc_bitmap); */
//} 


//Connect to iRM daemon via a TCP connection
static int _connect_to_irmd(void) {
   slurm_fd_t fd = -1;
   slurm_addr_t irm_address;
   uint16_t port = 12345;
   char *host = "127.0.0.1";

   _slurm_set_addr_char(&irm_address, port, host);

   while (!stop_agent) {
      fd = slurm_open_msg_conn(&irm_address);   
      if (fd < 0) {
         printf("\n[IRM_AGENT]: Failed to contact iRM daemon.\n");
         //return -1;
      } else {
         printf("\n[IRM_AGENT]: Successfully connected with iRM daemon.\n");
         break;
      }
      _my_sleep(irm_interval);
   }
   /*if (!stop_agent)
      printf("\n[IRM_AGENT]: Successfully connected to iRM daemon\n");*/
   return fd;
}

/* Note that slurm.conf has changed */
extern void irm_reconfig(void)
{
	config_flag = true;
}

/* irm_agent */
extern void *irm_agent(void *args)
{
	//time_t now;
	//double wait_time;
	//static time_t last_mapping_time = 0;
        slurm_fd_t fd = -1;
        //int log_fd = -1;
        int input = 0;
        char *buf = NULL; 
        bool empty_queue = true;
        uint16_t buf_val = -1;
        int ret_val = 0;
        slurm_msg_t *msg = NULL;
        resource_offer_msg_t res_off_msg;
        //int timeout = 30 * 1000;   // 30 secs converted to millisecs
        //pthread_attr_t attr;
	/* Read config, nodes and partitions; Write jobs */
	/*slurmctld_lock_t all_locks = {
		READ_LOCK, WRITE_LOCK, READ_LOCK, READ_LOCK };*/

        buf = (char *)malloc(sizeof(uint16_t));
        msg = xmalloc(sizeof(slurm_msg_t));
        msg->data = &res_off_msg;

        printf("\n[IRM_AGENT]: Entering irm_agent\n");
        printf("\n[IRM_AGENT]: Attempting to connect to iRM Daemon\n");

        fd = _connect_to_irmd();

        if (fd == -1) { 
           printf("\n[IRM_AGENT]: Unable to reach iRM daemon. Agent shutting down\n");
           return NULL;
        }

	_load_config();

        slurm_msg_t_init(msg);

	//last_mapping_time = time(NULL);
	while (!stop_agent) {
                ret_val = SLURM_SUCCESS;
		//_my_sleep(irm_interval);
		if (stop_agent)
			break;
		if (config_flag) {
			config_flag = false;
			_load_config();
		}
		/*now = time(NULL);
		wait_time = difftime(now, last_mapping_time);
		if ((wait_time < irm_interval))
			continue;*/
#ifdef DONT_EXECUTE
                if (empty_queue) {
                   ret_val = slurm_request_resource_offer(fd);
                   empty_queue = false;
                }
                if (ret_val == SLURM_SUCCESS)
                   ret_val = isched_recv_rsrc_offer(fd, msg);
                else {
                   printf("\nError in sending the request for resource offer to iRM. Shutting down the iRM agent.\n");
                   stop_irm_agent();
                   continue;
                }
                if (ret_val != SLURM_SUCCESS) {
                   printf("\nError in receiving the resource offer. Stopping iRM agent.\n");
                   stop_irm_agent();
                   continue;
                }
                printf("[IRM_AGENT]: Processing the offer\n");
                ret_val = process_rsrc_offer(msg->data, &buf_val);
                memcpy(buf, (char *)&buf_val, sizeof(buf_val)); 

                if (buf_val == 500) empty_queue = true; 
                //printf("\nbuf = %s\n", buf);
                //slurm_msg_t_init(msg);
                printf("\nBefore sending the response to the resource offer\n");
                ret_val = 0;
                xfree(msg->data);
                ret_val = isched_send_irm_msg(msg, buf);
                if (ret_val != SLURM_SUCCESS) {
                   printf("\nError in sending the response for the resource offer.\n");
                   stop_irm_agent();
                   continue;
                }
                printf("[IRM_AGENT]: Sent back a response to the resource offer\n");
#else
                ret_val = _slurm_recv_timeout(fd, buf, sizeof(uint16_t), 0, timeout);
                printf("\nret_val = %d\n", ret_val);
                printf("\nExited the recv timeout function\n");
                if (ret_val < 2) {
                   printf("\n[IRM_AGENT]: Did not receive correct number of bytes\n");
                   printf("\n[IRM_AGENT]: iRM agent closing\n");
                   stop_irm_agent();
                   continue;
                }

                printf("\n[IRM_AGENT]: Received a resource offer from iRM daemon which is %d\n", ntohs(*(int *)(buf)));
                printf("[IRM_AGENT]: Processing the offer\n");
                printf("\nEnter your choice 1/0 to accept/reject the offer\n");
                scanf("%d", &input);
		//lock_slurmctld(all_locks);
                
                //printf("\n***************[iRM AGENT]****************\n");
                //printf("\nReceived a resource offer from iRM\n");
                //printf("\nProcessing the offer for mapping jobs in the Invasic queue to this offer\n");
                if (input == 1) {
                   printf("\nSuccessfully mapped jobs to this offer and sending the list of jobs to be launched\n"); 
                   buf_val = htons(1);   
                   memcpy(buf, &buf_val, sizeof(buf_val));
                } else {
                   printf("\nOffer not accepted. Sending back a negative response\n");
                   buf_val = htons(0);   
                   memcpy(buf, &buf_val, sizeof(buf_val));
                }
                 //  printf("\nNegotiation complete\n");
                //printf("\n***************[iRM AGENT]****************\n");
                //memset(buf, 0, sizeof(int));
                //*buf = 0;
                sleep(5);
                ret_val = _slurm_send_timeout(fd, buf, sizeof(uint16_t), 0, timeout);
                if (ret_val < 2) {
                   printf("\n[IRM_AGENT]: Did not send correct number of bytes\n");
                   printf("\n[IRM_AGENT]: iRM agent closing\n");
                   stop_irm_agent();
                   continue;
                }
		//_compute_start_times();
		//last_mapping_time = time(NULL);
		//unlock_slurmctld(all_locks);
#endif
	}
        free(buf);
        xfree(msg->data);
        xfree(msg);
        close(fd);
        printf("\n[IRM_AGENT]: Exiting irm_agent\n");
	return NULL;
}
