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
bool stop_agent_irm = false;
bool urgent_jobs = false;
bool stop_ug_agent = false;
static pthread_t urgent_job_agent = 0;
static pthread_t feedback_thread = 0;
//pthread_mutex_t urgent_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t term_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t urgent_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  term_cond = PTHREAD_COND_INITIALIZER;
static bool config_flag = false;
static int irm_interval = BACKFILL_INTERVAL;
//static int max_sched_job_cnt = 50;
//static int sched_timeout = 0;

typedef enum {UNINITIALIZED, PROTOCOL_INITIALIZED, PROTOCOL_IN_PROGRESS, PROTOCOL_TERMINATING} STATE;

/*********************** local functions *********************/
//static void _compute_start_times(void);
static void _load_config(void);
static void _my_sleep(int secs);
//static int _connect_to_irmd(void);

#ifdef TESTING
FILE *log_irm_agent = NULL;
FILE *log_feedback_agent = NULL;
FILE *log_ug_agent = NULL;
#endif

/* Terminate iScheduler_agent */
extern void stop_irm_agent(void)
{
	pthread_mutex_lock(&term_lock);
	stop_agent_irm = true;
	if (!stop_agent_feedback) stop_feedback_agent();
	if (!stop_agent_ping) stop_ping_agent();
	if (!stop_ug_agent) stop_urgent_job_agent();
        print(log_irm_agent, "\nStopping IRM agent\n");
	if (!stop_agent_sleep) stop_sleep_agent();
	pthread_cond_signal(&term_cond);
	pthread_mutex_unlock(&term_lock);
}


extern void stop_urgent_job_agent(void)
{
	pthread_mutex_lock(&urgent_lock);
	stop_ug_agent = true;
        print(log_ug_agent, "\nStopping urgent job agent\n");
	pthread_mutex_unlock(&urgent_lock);
}


/* Set the shared flag urgent_jobs to TRUE */
extern void set_flag_urgent_jobs(void)
{
	pthread_mutex_lock(&urgent_lock);
	urgent_jobs = true;
        printf("\nSetting urgent jobs flag to TRUE\n");
	pthread_mutex_unlock(&urgent_lock);
}


static void _my_sleep(int secs)
{
	struct timespec ts = {0, 0};
	struct timeval now;

	gettimeofday(&now, NULL);
	ts.tv_sec = now.tv_sec + secs;
	ts.tv_nsec = now.tv_usec * 1000;
	pthread_mutex_lock(&term_lock);
	if (!stop_agent_irm)
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
/*static int _connect_to_irmd(void) {
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
   }*/
   /*if (!stop_agent)
      printf("\n[IRM_AGENT]: Successfully connected to iRM daemon\n");*/
   /*return fd;*/
//}

/* Note that slurm.conf has changed */
extern void irm_reconfig(void)
{
	config_flag = true;
}

/* irm_agent */
extern void *irm_agent(void *args)
{
        slurm_fd_t fd = -1;
        char *buf = NULL; 
	char *str = NULL;
        bool empty_queue = true;
        uint16_t buf_val = -1;
        int ret_val = 0;
        slurm_msg_t *msg = NULL;
        resource_offer_msg_t res_off_msg;
        bool initialized = false;
	bool terminated = false;
	bool feedback_initialized = false;
	pthread_attr_t attr;
	//bool non_zero_attemtpts = false;
	int attempts = 0;
	STATE irm_state = UNINITIALIZED;

        buf = (char *)malloc(sizeof(uint16_t));
        msg = xmalloc(sizeof(slurm_msg_t));
        msg->data = &res_off_msg;
	str = (char *)malloc(400 * sizeof(char));

#ifdef TESTING
	log_irm_agent = fopen(LOG_IRM_AGENT, "w");
	if (log_irm_agent == NULL) {
	   printf("\nError in opening the log file for iRM_Agent\n");
	   return -1;
	}
	log_feedback_agent = fopen(LOG_FEEDBACK_AGENT, "w");
	if (log_feedback_agent == NULL) {
	   printf("\nError in opening the log file for feedback_agent\n");
	   return -1;
	}
	log_ug_agent = fopen(LOG_UG_AGENT, "w");
	if (log_ug_agent == NULL) {
	   printf("\nError in opening the log file for urgent_jobs_agent\n");
	   return -1;
	}
#endif   

	slurm_attr_init(&attr);
#if defined (ISCHED_DEBUG) || defined (TESTING)
        print(log_irm_agent, "\n[IRM_AGENT]: Entering irm_agent\n");
        print(log_irm_agent, "\n[IRM_AGENT]: Attempting to connect to iRM Daemon\n");
#endif
        fd = _connect_to_irmd("127.0.0.1", 12345, &stop_agent_irm, irm_interval, "IRM_AGENT");

        if (fd < 0) { 
           print(log_irm_agent, "\n[IRM_AGENT]: Unable to reach iRM daemon. Agent shutting down\n");
           return NULL;
        }

	_load_config();

        slurm_msg_t_init(msg);

	if (irm_state == UNINITIALIZED) {
	   ret_val = protocol_init(fd);
	   irm_state = PROTOCOL_INITIALIZED;
	}
	if (ret_val != SLURM_SUCCESS) {
	   print(log_irm_agent, "\nProtocol initialization failed\n");
	   if (!stop_agent_irm) stop_irm_agent();
	} else {
	   slurm_attr_init(&attr);
           if (pthread_create( &urgent_job_agent, &attr, schedule_loop, NULL)) {
              print(log_irm_agent, "\nUnable to start a thread to execute a schedule loop for urgent jobs\n");
	      if (!stop_agent_irm) stop_irm_agent();
           } else {
#if defined (ISCHED_DEBUG) || defined (TESTING)
	      print(log_irm_agent, "\nSuccessfully created a thread which serves as an agent to dispatch urgent jobs immediately to iRM\n");
#endif
	   }
	   slurm_attr_destroy(&attr);
/*	   slurm_attr_init(&attr);
	   if (pthread_create( &feedback_thread, &attr, feedback_agent, NULL)) {
	      error("\nUnable to start a thread to process feedbacks from iRM\n");
	   } else {
#ifdef ISCHED_DEBUG
	      printf("\nSuccessfully created a thread which serves as the feedback agent\n");
#endif
	   }
	   slurm_attr_destroy(&attr); */
	}
#if defined (ISCHED_DEBUG) || defined (TESTING)
	sprintf(str, "\nstop_agent_irm = %d\n", stop_agent_irm);
	print(log_irm_agent, str);
#endif
	while (!stop_agent_irm) {
		irm_state = PROTOCOL_IN_PROGRESS;
                ret_val = SLURM_SUCCESS;
		if (stop_agent_irm)
			break;
		if (config_flag) {
			config_flag = false;
			_load_config();
		}
                if (empty_queue) {
#if defined (ISCHED_DEBUG) || defined(TESTING)
		   print(log_irm_agent, "\nIn this state, the irm agent will request for a resource offer only when some jobs have been added to the queue. For this purpose, we will have to periodically inspect the invasive job queue for new jobs.\n");
#endif
                   ret_val = request_resource_offer(fd);
                   empty_queue = false;
                }
                if (ret_val == SLURM_SUCCESS) {
                   ret_val = receive_resource_offer(fd, msg);
		   attempts++;
                } else {
                   print(log_irm_agent, "\nError in sending the request for resource offer to iRM. Shutting down the iRM agent.\n");
                   if (!stop_agent_irm) stop_irm_agent();
		   //stop_feedback_agent();
                   continue;
                }
                if (ret_val != SLURM_SUCCESS) {
                   print(log_irm_agent, "\nError in receiving the resource offer. Stopping iRM agent.\n");
                   if (!stop_agent_irm) stop_irm_agent();
		   //stop_feedback_agent();
                   continue;
                }
		if (!feedback_initialized) {
		   feedback_initialized = true;
		   slurm_attr_init(&attr);
                   if (pthread_create( &feedback_thread, &attr, feedback_agent, NULL)) {
                      print(log_irm_agent, "\nUnable to start a thread to process feedbacks from iRM\n");
		      if (!stop_irm_agent) stop_irm_agent();
                   } else {
#if defined (ISCHED_DEBUG) || defined (TESTING)
                      print(log_irm_agent, "\nSuccessfully created a thread which serves as the feedback agent\n");
#endif
                   }
                   slurm_attr_destroy(&attr);
		}
#if defined (ISCHED_DEBUG) || defined (TESTING)
                print(log_irm_agent, "[IRM_AGENT]: Processing the offer\n");
#endif
                ret_val = process_resource_offer(msg->data, &buf_val, &attempts);
                memcpy(buf, (char *)&buf_val, sizeof(buf_val)); 

                if (buf_val == 500) { 
		   empty_queue = true; 
		   attempts = 0;
		}
                if (buf_val == 400) {
		   irm_state = PROTOCOL_TERMINATING;
		   attempts = 0;
		   slurm_free_resource_offer_resp_msg(msg->data);
		   if (!stop_agent_irm) stop_irm_agent();
		   //stop_feedback_agent();
		   continue;
		}
#if defined (ISCHED_DEBUG) || defined (TESTING)
                print(log_irm_agent, "\nBefore sending the response to the resource offer\n");
#endif
                ret_val = 0;
		slurm_free_resource_offer_resp_msg(msg->data);
                ret_val = send_resource_offer_resp(msg, buf);
                if (ret_val != SLURM_SUCCESS) {
                   printf("\nError in sending the response for the resource offer.\n");
                   if (!stop_agent_irm) stop_irm_agent();
		   //stop_feedback_agent();
                   continue;
                }
#if defined (ISCHED_DEBUG) || defined (TESTING)
                print(log_irm_agent, "[IRM_AGENT]: Sent back a response to the resource offer\n");
#endif
	}

        switch(irm_state) {
	    case PROTOCOL_IN_PROGRESS:
		 if (ret_val == SLURM_SUCCESS) {
		    ret_val = receive_resource_offer(fd, msg);
		    if (ret_val == SLURM_SUCCESS) {
		       print(log_irm_agent, "\nReceived a resource offer from iRM. We are proceeding to terminate so we ignore this message.\n");
		       slurm_free_resource_offer_resp_msg(msg->data);
		    }
		    ret_val = protocol_fini(fd);
		 }
		 break;
 	    case PROTOCOL_TERMINATING:
 	    case PROTOCOL_INITIALIZED:
		 if (ret_val == SLURM_SUCCESS) {
		    ret_val = protocol_fini(fd);
		 }
		 break;
 	    default:
		 ret_val = SLURM_ERROR;
	}

	if (ret_val == SLURM_SUCCESS) {
	   print(log_irm_agent, "\nProtocol termination succeeded\n");
        } else {
           print(log_irm_agent, "\nProtocol termination failed\n");
        }

        free(buf);
        slurm_free_msg(msg);
        close(fd);
	pthread_join(urgent_job_agent, NULL);
	pthread_join(feedback_thread, NULL);
        print(log_irm_agent, "\n[IRM_AGENT]: Exiting irm_agent\n");
	free(str);
	fclose(log_irm_agent);
	fclose(log_feedback_agent);
	fclose(log_ug_agent);
	return NULL;
}
