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

#ifdef INVASIC_SCHEDULING
static void _copy_job_to_forward(struct forward_job_record *forward_job_ptr, struct job_record *job_ptr);
#endif
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

#ifdef INVASIC_SCHEDULING
static List schedule_invasic_jobs(resource_offer_msg_t *offer)
{
	int j, rc = SLURM_SUCCESS, job_cnt = 0, mapped_job_cnt = 0;
	List invasic_job_queue, last_mapped_job_queue;
	List mapped_job_queue;
	invasic_job_queue_rec_t *invasive_job_queue_rec;
	List preemptee_candidates = NULL;
	uint16_t rand_choice = 0;
	struct job_record *job_ptr;
	struct forward_job_record *forward_job_ptr = NULL;
	/*bitstr_t *alloc_bitmap = NULL, *avail_bitmap = NULL;
	bitstr_t *exc_core_bitmap = NULL;
	uint32_t max_nodes, min_nodes, req_nodes, time_limit; */
	time_t now = time(NULL), sched_start, last_job_alloc;
	char str[1000];

	print(log_irm_agent, "\nInside job scheduler\n");

	sched_start = now;
	//last_job_alloc = now - 1;
	//alloc_bitmap = bit_alloc(node_record_count);
	mapped_job_queue = list_create(_invasive_job_queue_rec_del);

	invasic_job_queue = build_invasic_job_queue(true, false, log_irm_agent);
	sort_invasic_job_queue(invasic_job_queue);

	while ((invasive_job_queue_rec = (invasive_job_queue_rec_t *) list_pop(invasic_job_queue))) {
		job_ptr  = invasive_job_queue_rec->job_ptr;
		xfree(invasive_job_queue_rec);

		if (rand_choice = (rand() % 2) ) {
		   /* We consider the job found in this iteration for the mapping to the available offer */
		   forward_job_ptr = (struct forward_job_record *) xmalloc(sizeof(struct forward_job_record));
		   _copy_job_to_forward(forward_job_ptr, job_ptr);
		   list_append(mapped_job_queue, forward_job_ptr);
		}

		/*if (job_cnt++ > max_sched_job_cnt) {
			debug2("scheduling loop exiting after %d jobs",
			       max_sched_job_cnt);
			break;
		} */

		/*if (strcmp(job_ptr->partition, "invasic") == 0) {
		   print(log_irm_agent, "\nFound an invasic job, ");
		   sprintf(str, "Job ID=%d\n", job_ptr->job_id);
		   print(log_irm_agent, str);
		}*/


	/*	if ((time(NULL) - sched_start) >= sched_timeout) {
			debug2("scheduling loop exiting after %d jobs",
			       max_sched_job_cnt);
			break;
		}*/
	}
	list_destroy(invasic_job_queue);
	//FREE_NULL_BITMAP(alloc_bitmap); 
	print(log_irm_agent, "\nExiting job scheduler\n");
	return mapped_job_queue
} 

static void _copy_job_to_forward(struct forward_job_record *forward_job_ptr, struct job_record *job_ptr)
{
	struct job_details *details = (struct job_details *)xmalloc(sizeof(struct job_details));
	struct job_details *job_details_ptr = job_ptr->details;

	forward_job_ptr->cr_enabled = job_ptr->cr_enabled;
	forward_job_ptr->db_index = job_ptr->db_index;
	forward_job_ptr->details = details;

	details->acctg_freq = xstrdup(job_details_ptr->acctg_freq);
	details->argc = job_details_ptr->argc;
	details->argv = (char **) NULL;
	details->begin_time = job_details_ptr->time;
	details->ckpt_dir = xstrdup(job_details_ptr->ckpt_dir);
	details->contiguous = job_details_ptr->contiguous;
	details->core_spec = job_details_ptr->core_spec;
	details->cpu_bind = xstrdup(job_details_ptr->cpu_bind);
	details->cpu_bind_type = job_details_ptr->cpu_bind_type;
	details->cpus_per_task = job_details_ptr->cpus_per_task;
	details->depend_list = depended_list_copy(job_details_ptr->depend_list);
	details->dependency = xstrdup(job_details_ptr->dependency);
	details->orig_dependency = xstrdup(job_details_ptr->orig_dependency);
	details->env_cnt = job_details_ptr->env_cnt;
	details->env_sup = (char **) NULL;
	details->exc_node_bitmap = bit_copy(job_details_ptr->exc_node_bitmap);	
	details->exc_nodes = xstrdup(job_details_ptr->exc_nodes);
	details->expanding_job_id = job_details_ptr->expanding_job_id;
	details->exc_nodes = xstrdup(job_details_ptr->exc_nodes);
	details->expanding_job_id = job_details_ptr->expanding_job_id;
	details->feature_list = NULL;
	details->features = xstrdup(job_details_ptr->features);
	details->magic = job_details_ptr->magic;
	details->max_cpus = job_details_ptr->max_cpus;
	details->max_nodes = job_details_ptr->max_nodes;
	details->mc_ptr----------------------
	details->mem_bind = xstrdup(job_details_ptr->mem_bind);
	details->mem_bind_type = job_details_ptr->mem_bind_type;
	details->min_cpus = job_details_ptr->min_cpus;
	details->min_nodes = job_details_ptr->min_nodes;
	details->nice = job_details_ptr->nice;
	details->ntasks_per_node = job_details_ptr->ntasks_per_node;
	details->num_tasks = job_details_ptr->num_tasks;	
	details->open_mode = job_details_ptr->open_mode;
	details->overcommit = job_details_ptr->overcommit;
	details->plane_size = job_details_ptr->plane_size;	
	details->pm_min_cpus = job_details_ptr->pm_min_cpus;
	details->pm_min_memory = job_details_ptr->pm_min_memory;
	details_pm_min_tmp_disk = job_details_ptr->pm_min_tmp_disk;		
	details->prolog_running = job_details_ptr->prolog_running;
	details->reserved_resources = job_details_ptr->reserved_resources;
	details->req_node_bitmap = bit_copy(job_details_ptr->req_node_bitmap);
	details->req_node_layout = NULL;
	details->preempt_start_time = job_details_ptr->preempt_start_time;
	details->req_nodes = xstrdup(job_details_ptr->req_nodes);
	details->requeue = job_details_ptr->requeue;
	details->restart_dir = xstrdup(job_details_ptr->restart_dir);
	details->share_res = job_details_ptr->share_res;
	details->std_err = xstrdup(job_details_ptr->std_err);
	details->std_in = xstrdup(job_details_ptr->std_in);
	details->std_out = xstrdup(job_details_ptr->std_out);
	details->submit_time = job_details_ptr->submit_time;
	details->task_dist = job_details_ptr->task_dist;
	details->usable_nodes = job_details_ptr->usable_nodes;
	details->whole_node = job_details_ptr->whole_node;
	details->work_dir = xstrdup(job_details_ptr->work_dir);

	forward_job_ptr->direct_set_prio = job_ptr->direct_set_prio;
	forward_job_ptr->job_id = job_ptr->job_id;
	forward_job_ptr->magic = job_ptr->magic;
	forward_job_ptr->name = xstrdup(job_ptr->name);
	forward_job_ptr->requid = job_ptr->requid;
	forward_job_ptr->time_limit = job_ptr->time_limit;
	forward_job_ptr->time_min = job_ptr->time_min;
	forward_job_ptr->user_id = job_ptr->user_id;
	forward_job_ptr->wckey = xstrdup(job_ptr->wckey);
}

#endif

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
	slurmctld_lock_t all_locks = {
                READ_LOCK, WRITE_LOCK, READ_LOCK, READ_LOCK };
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

	srand(time(NULL));

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
#if defined (ISCHED_DEBUG) 
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
#if defined (ISCHED_DEBUG)
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
#if defined (ISCHED_DEBUG)
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
		   print(log_irm_agent, "\n\nIn this state, the irm agent will request for a resource offer only when some jobs have been added to the queue. For this purpose, we will have to periodically inspect the invasive job queue for new jobs.\n\n");
		   print(log_irm_agent, "===========================================================================================================================================\n\n");
#endif
                   ret_val = request_resource_offer(fd);
                   empty_queue = false;
                }
                if (ret_val == SLURM_SUCCESS) {
                   ret_val = receive_resource_offer(fd, msg);
		   //if (attempts != MAX_NEGOTIATION_ATTEMPTS)
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
#if defined (ISCHED_DEBUG) 
                      print(log_irm_agent, "\nSuccessfully created a thread which serves as the feedback agent\n");
#endif
                   }
                   slurm_attr_destroy(&attr);
		}

#if defined (ISCHED_DEBUG) 
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
#if defined (ISCHED_DEBUG) 
                print(log_irm_agent, "\nBefore sending the response to the resource offer\n");
#endif
                ret_val = 0;
	#ifdef INVASIC_SCHEDULING		
		if (*buf_val == 1) {
		   lock_slurmctld(all_locks);
		   schedule_invasic_jobs(msg->data);
		   unlock_slurmctld(all_locks);
		}
	#endif
		slurm_free_resource_offer_resp_msg(msg->data);
                ret_val = send_resource_offer_resp(msg, buf);
                if (ret_val != SLURM_SUCCESS) {
                   printf("\nError in sending the response for the resource offer.\n");
                   if (!stop_agent_irm) stop_irm_agent();
		   //stop_feedback_agent();
                   continue;
                }
#if defined (ISCHED_DEBUG) 
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
	fflush(log_irm_agent);
	fflush(log_feedback_agent);
	fflush(log_ug_agent);
	fclose(log_irm_agent);
	fclose(log_feedback_agent);
	fclose(log_ug_agent);
	return NULL;
}
