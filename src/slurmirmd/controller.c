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
#include <signal.h>

#include "slurm/slurm.h"
#include "slurm/slurm_errno.h"

#include "src/common/list.h"
#include "src/common/macros.h"
#include "src/common/node_select.h"
#include "src/common/parse_time.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/xsignal.h"

#include "src/slurmirmd/slurmirmd.h"

#ifndef BACKFILL_INTERVAL
#  define BACKFILL_INTERVAL	10
#endif
#define MAX_NEGOTIATION_ATTEMPTS 5

#define DONT_EXECUTE_NOW 1

typedef enum{UNINITIALIZED, PROTOCOL_INITIALIZED, PROTOCOL_IN_PROGRESS, PROTOCOL_TERMINATING} STATE;

static int controller_sigarray[] = {
	SIGINT, SIGTERM, SIGHUP, SIGABRT,
	SIGQUIT, 0
};

static void * _slurmirmd_signal_hand(void *no_data);
static void _default_sigaction(int sig);

/*********************** local variables *********************/
static bool stop_agent = false;
bool stop_agent_urgent_job = false;
static pthread_mutex_t term_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t urgent_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  term_cond = PTHREAD_COND_INITIALIZER;
static pthread_t feedback_thread = 0;
static pthread_t urgent_job_agent = 0;
static pthread_t thread_id_sig = 0;
static bool initialized = false;
//static bool config_flag = false;
//static int irm_interval = BACKFILL_INTERVAL;
//static int max_sched_job_cnt = 50;
//static int sched_timeout = 0;

/*********************** local functions *********************/
//static void _compute_start_times(void);
static void _load_config(void);
extern void stop_urgent_job_agent(void);
//static void _my_sleep(int secs);
//static int _init_comm(void);

#ifdef TESTING
   extern resource_offer_msg_t tc_offer;
   FILE *log_irm_agent = NULL;
   FILE *log_feedback_agent = NULL;
   FILE *log_ug_agent = NULL;
#endif

/* Terminate ischeduler_agent */
extern void stop_irm_agent(void)
{
	pthread_mutex_lock(&term_lock);
	stop_agent = true;
	stop_urgent_job_agent();
	//stop_agent_urgent_job = true;
        print(log_irm_agent, "\nStopping IRM agent\n");
	pthread_cond_signal(&term_cond);
	pthread_mutex_unlock(&term_lock);
}

extern void stop_urgent_job_agent(void)
{
	pthread_mutex_lock(&urgent_lock);
	stop_agent_urgent_job = true;
	print(log_ug_agent, "\nStopping urgent job agent\n");
	pthread_mutex_unlock(&urgent_lock);
}

/*static void _my_sleep(int secs)
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
}*/

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
/*static int _init_comm(void) {
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
}*/

/* Note that slurm.conf has changed */
/*extern void irm_reconfig(void)
{
	config_flag = true;
}*/

/* irm daemon */
int main(int argc, char *argv[])
{
        slurm_msg_t msg;
        slurm_fd_t fd = -1;
        slurm_fd_t client_fd = -1;
        char *buf = NULL;
#ifdef TESTING
	char str[1000];
#endif
	uint16_t last_mapping_error_code = 0;
	char *last_mapping_error_msg = NULL;
        int ret_val;
        int attempts = 0;
        slurm_addr_t cli_addr;
        int val = -1, input = -1;
        resource_offer_msg_t *req = NULL;
        resource_offer_resp_msg_t *resp = NULL;
        bool no_jobs = true;
	bool final_negotiation = false;
	int flag = 0;
	STATE irm_state = UNINITIALIZED;
	pthread_attr_t attr;

        buf = (char *)malloc(sizeof(int));
	req = xmalloc(sizeof(resource_offer_msg_t));

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

	/* This must happen before we spawn any threads
         * which are not designed to handle them */
        if (xsignal_block(controller_sigarray) < 0)
                error("Unable to block signals");

	/*
	 * create attached thread for signal handling
	 */
	slurm_attr_init(&attr);
	while (pthread_create(&thread_id_sig,
			      &attr, _slurmirmd_signal_hand,
			      NULL)) {
		error("pthread_create %m");
		sleep(1);
	}
	slurm_attr_destroy(&attr);

#if defined (IRM_DEBUG)
        print(log_irm_agent, "\n[IRM_DAEMON]: Entering irm_agent\n");
#endif
        fd = _init_comm("127.0.0.1", 12345, "IRM_DAEMON");

        if (fd == -1) { 
           print(log_irm_agent, "\n[IRM_DAEMON]: Unsuccessful initialization of communication engine. Agent shutting down\n");
           return 0;
        }

        client_fd = _accept_msg_conn(fd, &cli_addr);
        if (client_fd != SLURM_SOCKET_ERROR) {
	#ifdef IRM_DEBUG
           print(log_irm_agent, "\n[IRM_DAEMON]: Accepted connection from iScheduler. Communications can now start\n");
	#endif
        } else {
           print(log_irm_agent, "\n[IRM_DAEMON]: Unable to receive any connection request from iScheduler. Shutting down the daemon.\n");
           stop_agent = true;
	   goto total_return;
        }

	_load_config();

        slurm_msg_t_init(&msg);

	if (!initialized) {
           ret_val = protocol_init(client_fd);
           initialized = true;
	   irm_state = PROTOCOL_INITIALIZED;
        }
        if (ret_val != SLURM_SUCCESS) {
           print(log_irm_agent, "\nProtocol initialization falied\n");
           stop_irm_agent();
        } else {
	   slurm_attr_init(&attr);
	   if (pthread_create( &urgent_job_agent, &attr, schedule_loop, NULL)) {
	      print(log_irm_agent, "\nUnable to start the agent to handle urgent jobs\n");
	   } else {
#if defined (IRM_DEBUG) || defined (TESTING)
	      print(log_irm_agent, "\nSuccessfully created a thread to handle urgent jobs\n");
#endif
	   }
	}
	while (!stop_agent) {
		ret_val = SLURM_SUCCESS;

		if (stop_agent) {
		   if (flag)
		      stop_feedback_agent();
	 	   break;
		}
                //if (input == 0) {
		/*if (last_mapping_error_code == ESLURM_MAPPING_FROM_JOBS_TO_OFFER_REJECT) {
                   printf("\niRM has not accepted the mapping from iScheduler. We will send a new offer now.\n");
                   attempts++;
                } 
                //if (input == 1) {
		if (last_mapping_error_code == SLURM_SUCCESS && irm_state == PROTOCOL_IN_PROGRESS) {
                   printf("\niRM has accepted the mapping from iScheduler. Will launch the submitted jobs shortly. After launch we will send further new offers.\n");
                   attempts = 0;
                }*/ /*else {
                   if (attempts) {
                      printf("\nEither iScheduler did not accept the offer we sent or it was an invalid response.\n");
                   } 
                }*/
                
                //input = -1;

                if (no_jobs) {
                   //ret_val = wait_req_rsrc_offer(client_fd, &msg);
                   ret_val = wait_req_rsrc_offer(client_fd);/*, req_msg);*/
		   irm_state = PROTOCOL_IN_PROGRESS;
		   attempts = 1;
		   req->negotiation = 1;
                }
                if (ret_val == SLURM_SUCCESS) {
		   if (!flag) {
		      /* Create an attached thread for feedback agent */
        	      slurm_attr_init(&attr);
        	      if (pthread_create(&feedback_thread, &attr, feedback_agent, NULL)) {
                         print(log_irm_agent, "pthread_create error %m");
        	      }
		#if defined (IRM_DEBUG) || defined (TESTING)
		      print(log_irm_agent, "\nSuccessfully created a thread for the feedback agent\n");
		#endif
        	      slurm_attr_destroy(&attr);
		      flag = 1;
		   }
                   no_jobs = false; 
                   //xfree(msg.data);
		   //slurm_free_request_resource_offer_msg(req_msg);
#if defined (IRM_DEBUG) 
                   print(log_irm_agent, "\nCreating a new resource offer to send to iScheduler\n");
#endif
                   //ret_val = slurm_submit_resource_offer(client_fd, &req, &resp);
		   //Populate the request message here with the error code and error msg for the previous mapping of jobs to offer
		   req->error_code = last_mapping_error_code;
		   req->error_msg = last_mapping_error_msg;
		#ifdef TESTING
		   memcpy(&tc_offer, req, sizeof(resource_offer_msg_t));
		#endif
		   sleep(1);
		   req->resource_offer = create_resource_offer();
                   ret_val = slurm_submit_resource_offer(client_fd, req, &resp);
		   if (attempts == 0) attempts++;
                } else {
                   print(log_irm_agent, "\nHave not received any request for resource offer yet. Shutting down the daemon along with the feedback agent\n");
		   if (flag)
		      stop_feedback_agent();
                   stop_irm_agent();
		   pthread_kill(thread_id_sig, SIGTERM);
                   continue;
                }
                if (ret_val != SLURM_SUCCESS) {
                   print(log_irm_agent, "\niRM agent shutting down along with feedback agent\n");
                   /*xfree(resp.error_msg); Not valid because this could be a negotiation end message. Need to handle this better */
		   pthread_kill(thread_id_sig, SIGTERM);
		   stop_feedback_agent();
                   stop_irm_agent();
                   continue;
                }/* else {
		   last_mapping_error_code = 0;
		   last_mapping_error_msg = NULL;*/
		   /*if (resp == NULL) { printf("\nNULL pointer\n"); }
		   printf("\nError code = %d, Error msg = %s\n", resp->error_code, resp->error_msg);
		   if (err_msg) {
		      printf("\nFreeing the local memory allocation for an error msg\n");
		      free(err_msg);
		   }*/
		   /*if (resp->error_msg != NULL) {
		      printf("\nError message inside the response message is %s\n", resp->error_msg);
		      err_msg = malloc(sizeof(char) * strlen(resp->error_msg));
		      memcpy(err_msg, resp->error_msg, strlen(resp->error_msg));
		      printf("\nTrying to free the error_msg inside response msg\n");
		      //xfree(resp->error_msg);
		   }
		}*/
		last_mapping_error_code = 0;
		last_mapping_error_msg = NULL;

                /*val = resp->value; */

                /*if (val == 500) { */
		if (resp->error_code == ESLURM_INVASIVE_JOB_QUEUE_EMPTY) {
		#ifdef IRM_DEBUG
                   print(log_irm_agent, "\niScheduler responded saying that it has no jobs. We will now wait till we receive a request from the iScheduler for a resource offer\n");
		#endif
                   no_jobs = true;
                   attempts = 0;
		   req->negotiation = 0;
		#ifdef TESTING
		   print(log_irm_agent, "\n=========================================================================================================================================\n\n");
		#endif
             /*      xfree(resp.error_msg); */
		   slurm_free_resource_offer_resp_msg(resp);
                   continue;
                }        

                if (attempts == MAX_NEGOTIATION_ATTEMPTS) {
		   print(log_irm_agent, "\nReached the limit for negotiation attempts. Accepting the mapping given by the iScheduler. The transaction ends here. No. of attempts-->");
		   sprintf(str, "%d\n", attempts);
		   print(log_irm_agent, str);
                   print(log_irm_agent, "\nA new transaction will start now by constructing a new resource offer and sending it.\n\n");
		   print(log_irm_agent, "===========================================================================================================================================\n\n");
                   attempts = 0;
                   ret_val = process_rsrc_offer_resp(resp, true);
		   req->negotiation = 0;
               /*    xfree(resp.error_msg); */
		   slurm_free_resource_offer_resp_msg(resp);
                   continue;
                }

                /*if (val == 0) {*/
		if (resp->error_code == ESLURM_RESOURCE_OFFER_REJECT) {
		#ifdef IRM_DEBUG
                   print(log_irm_agent, "\niScheduler did not accept this offer.\n");
		#endif
                   attempts++;
		   req->negotiation = 1;
		   //slurm_free_resource_offer_resp_msg(resp);
                /*} else if (val == 1) {*/
		} else if (resp->error_code == SLURM_SUCCESS) {
		#ifdef IRM_DEBUG
                   print(log_irm_agent, "\niScheduler accepted the offer\n");
		#endif
                   ret_val = process_rsrc_offer_resp(resp, false);
		   if (ret_val != SLURM_SUCCESS) {
		      last_mapping_error_code = ret_val;
		      last_mapping_error_msg = slurm_strerror(last_mapping_error_code);
		      print(log_irm_agent, "\niRM has rejected the mapping.\n");
		      attempts++;
		      req->negotiation = 1;
		   } else {
		      print(log_irm_agent, "\niRM has accepted the mapping. The transaction ends here. No. of attempts-->");
		      sprintf(str, "%d\n", attempts);
		      print(log_irm_agent, str);
		      print(log_irm_agent, "\n==============================================================================================================================================\n\n");
		      attempts = 0;
		      req->negotiation = 0;
		   }
                } else {
                   print(log_irm_agent, "\nInvalid response from iScheduler. Ignoring this.\n");
		   last_mapping_error_code = SLURM_UNEXPECTED_MSG_ERROR;
		   last_mapping_error_msg = slurm_strerror(last_mapping_error_code);
                   attempts++;
		   req->negotiation = 1;
                }  
		slurm_free_resource_offer_resp_msg(resp);
	}

/* Be careful when using slurm_strerror to initialize the error msg data member of messages. This function returns a pointer into a statically
   allocated string array holding the string representations of these errors. Do not attempt slurm_xfree of the msg via slurm_free_.... call
   without first setting the error_msg pointer to NULL. This will result in trying to free a statically allocated memory resulting in
   segmentation fault */

total_return:req->error_msg = NULL;
	slurm_free_resource_offer_msg(req);
	// slurm_free_resource_offer_resp_msg(resp);  //May not be required. Can be removed later after sufficient testing
        free(buf);
        close(client_fd);
        close(fd);
	slurm_conf_destroy();
	log_fini();
	pthread_join(feedback_thread,  NULL);
	pthread_join(thread_id_sig, NULL);
	pthread_join(urgent_job_agent, NULL);
        print(log_irm_agent, "\n[IRM_DAEMON]: Exiting iRM Daemon\n");
	fflush(log_irm_agent);
	fflush(log_feedback_agent);
	fflush(log_ug_agent);
	fclose(log_irm_agent);
	fclose(log_feedback_agent);
	fclose(log_ug_agent);
	return 0;
}


/* _slurmirmd_signal_hand - Process daemon-wide signals */
static void *_slurmirmd_signal_hand(void *no_data)
{
	printf("\nInside signal handler\n");
        int sig;
        int i, rc;
        int sig_array[] = {SIGINT, SIGTERM, SIGHUP, SIGABRT, SIGQUIT, 0};
        sigset_t set;

        (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

        /* Make sure no required signals are ignored (possibly inherited) */
        for (i = 0; sig_array[i]; i++)
                _default_sigaction(sig_array[i]);

        while (1) {
                xsignal_sigset_create(sig_array, &set);
		printf("\nBefore calling sigwait\n");
                rc = sigwait(&set, &sig);
                if (rc == EINTR)
                        continue;
                switch (sig) {
                case SIGINT:    /* kill -2  or <CTRL-C> */
                case SIGTERM:   /* kill -15 */
                case SIGHUP:    /* kill -1 */
		case SIGABRT:   /* abort */
		case SIGQUIT:
			//if (!stop_agent_feedback)   variable is stop_agent and is still static in the feedback_agent.c file. Take care of it later
			printf("\nReceived a signal\n");
			   stop_feedback_agent();
			//if (!stop_agent_irm)
			   stop_irm_agent();			
			break;
                default:
                        error("Invalid signal (%d) received", sig);
                }
		if (rc != EINTR) break;
        }
	printf("\nExiting signal handler\n");
	return NULL;
}

static void _default_sigaction(int sig)
{
        struct sigaction act;
        if (sigaction(sig, NULL, &act)) {
                error("sigaction(%d): %m", sig);
                return;
        }
        if (act.sa_handler != SIG_IGN)
                return;

        act.sa_handler = SIG_DFL;
        if (sigaction(sig, &act, NULL))
                error("sigaction(%d): %m", sig);
}

