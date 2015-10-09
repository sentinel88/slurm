/*****************************************************************************\
 *  slurmirmd.h - iRM Daemon.
 *****************************************************************************
 *  Copyright (C) 2015-2016 Nishanth Nagendra, Technical University of Munich.
\*****************************************************************************/


#ifndef _SLURM_IRMD_H
#define _SLURM_IRMD_H

#include "src/common/xmalloc.h"
#include <poll.h>

extern int slurm_submit_resource_offer(slurm_fd_t fd, resource_offer_msg_t *, resource_offer_resp_msg_t **);

extern int wait_req_rsrc_offer (slurm_fd_t);/*, slurm_msg_t * request_resource_offer_msg_t *);*/

extern int protocol_init(slurm_fd_t);

extern int protocol_fini(slurm_fd_t);

extern int process_rsrc_offer_resp(resource_offer_resp_msg_t *, bool);

extern int compute_feedback(status_report_msg_t *);

extern int send_feedback(slurm_fd_t, status_report_msg_t *);

extern void *feedback_agent(void *);

extern void stop_feedback_agent(void);

extern void *schedule_loop(void *);

extern int _init_comm(char *, uint16_t, char *);

extern int _accept_msg_conn(slurm_fd_t, slurm_addr_t *);

extern int recv_send_urgent_job(slurm_fd_t);

#define timeout (30*1000)

extern bool stop_agent_urgent_job;

#define TESTING 1

extern int print(FILE *, char *);

#ifdef TESTING
#define LOG_IRM_AGENT "/home/nishanth/logs_SLURM/iHyper_iRM_Daemon.txt"
#define LOG_FEEDBACK_AGENT "/home/nishanth/logs_SLURM/iHyper_Feedback_Agent.txt"
#define LOG_UG_AGENT "/home/nishanth/logs_SLURM/iHyper_UG_Agent.txt"

extern FILE *log_irm_agent;
extern FILE *log_feedback_agent;
extern FILE *log_ug_agent;
#endif


/* ischeduler_agent - detached thread periodically when pending jobs can start */
//extern void *isched_agent(void *args);

/* Terminate irm_agent */
//extern void stop_irm_agent(void);

/* Note that slurm.conf has changed */
//extern void ischeduler_reconfig(void);

#endif	/* _SLURM_IRMD_H */
