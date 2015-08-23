/*****************************************************************************\
 *  feedback_agent.c - Feedback Agent for receiving periodic reports from
 *                     Invasive resource manager.
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
#  define BACKFILL_INTERVAL	30
#endif

/*********************** local variables *********************/
static bool stop_agent = false;
static pthread_mutex_t term_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  term_cond = PTHREAD_COND_INITIALIZER;
static bool config_flag = false;
static int feedback_interval = BACKFILL_INTERVAL;
//static int max_sched_job_cnt = 50;
//static int sched_timeout = 0;

/*********************** local functions *********************/
//static void _compute_start_times(void);
static void _load_config(void);
static void _my_sleep(int secs);

/* Terminate feedback_agent */
extern void stop_feedback_agent(void)
{
	pthread_mutex_lock(&term_lock);
	stop_agent = true;
        printf("\nStopping FEEDBACK AGENT\n");
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

//Connect to iRM daemon's feedback agent via a TCP connection
static int _connect_to_irmd(void) {
   slurm_fd_t fd = -1;
   slurm_addr_t irm_address;
   uint16_t port = 12346;
   char *host = "127.0.0.1";

   _slurm_set_addr_char(&irm_address, port, host);

   while (!stop_agent) {
      fd = slurm_open_msg_conn(&irm_address);
      if (fd < 0) {
         printf("\n[FEEDBACK_AGENT]: Failed to contact iRM daemon's feedback agent.\n");
         //return -1;
      } else {
         printf("\n[FEEDBACK_AGENT]: Successfully connected with iRM daemon's feedback agent.\n");
         break;
      }
      _my_sleep(feedback_interval);
   }
   /*if (!stop_agent)
      printf("\n[IRM_AGENT]: Successfully connected to iRM daemon\n");*/
   return fd;
}

/* Note that slurm.conf has changed */
extern void feedback_agent_reconfig(void)
{
	config_flag = true;
}

/* feedback_agent */
extern void *feedback_agent(void *args)
{
	int ret_val = SLURM_SUCCESS;
	slurm_fd_t fd = -1;
	slurm_msg_t *msg;
        printf("\n[FEEDBACK_AGENT]: Entering feedback_agent\n");
	printf("\n[FEEDBACK_AGENT]: Attempting to connect to the feedback agent of iRM daemon\n");

	fd = _connect_to_irmd();
        if (fd == -1) {
           printf("\n[FEEDBACK_AGENT]: Unable to reach iRM daemon. Agent shutting down\n");
           return NULL;
        }

	msg = xmalloc(sizeof(slurm_msg_t));

	last_feedback_time = time(NULL);
	while (!stop_agent) {
		_my_sleep(feedback_interval);
		if (stop_agent)
			break;
		}
		now = time(NULL);
		wait_time = difftime(now, last_feedback_time);
		if ((wait_time < feedback_interval))
			continue;

		//lock_slurmctld(all_locks);
		ret_val = receive_feedback(fd, msg);
		if (ret_val != SLURM_SUCCESS) {
		   printf("\nError in receiving the periodic feedback from iRM. Shutting down the feedback agent\n");
		   stop_feedback_agent();
		   continue;
		}
                printf("\nFeedback report received from iRM\n");
                printf("\nProcessing the report now\n");
		ret_val = process_feedback(msg);
		if (ret_val != SLURM_SUCCESS) {
		   printf("\nError in processing the feedback from iRM. Shutting down the agent\n");
		   stop_feedback_agent();
		   continue;
		}
                printf("\nFinished updating history. Will sleep for sometime before processing the next feedback report\n");
		//_compute_start_times();
		last_feedback_time = time(NULL);
		//unlock_slurmctld(all_locks);
	}
	slurm_free_status_report_msg(msg);
        printf("\n[FEEDBACK_AGENT]: Exiting feedback_agent\n");
	return NULL;
}
