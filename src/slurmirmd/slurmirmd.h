/*****************************************************************************\
 *  slurmirmd.h - iRM Daemon.
 *****************************************************************************
 *  Copyright (C) 2015-2016 Nishanth Nagendra, Technical University of Munich.
\*****************************************************************************/


#ifndef _SLURM_IRMD_H
#define _SLURM_IRMD_H

#include "src/common/xmalloc.h"

extern int slurm_submit_resource_offer(slurm_fd_t fd, resource_offer_msg_t *, resource_offer_resp_msg_t *);

extern int wait_req_rsrc_offer (slurm_fd_t);/*, slurm_msg_t * request_resource_offer_msg_t *);*/

extern int protocol_init(slurm_fd_t);

extern int protocol_fini(slurm_fd_t);

extern int send_custom_data(slurm_fd_t);

extern int process_rsrc_offer_resp(resource_offer_resp_msg_t *, bool);

#define timeout (30*1000)

#define _TESTING 0

extern bool initialized;

extern bool terminated;
/* ischeduler_agent - detached thread periodically when pending jobs can start */
//extern void *isched_agent(void *args);

/* Terminate irm_agent */
//extern void stop_irm_agent(void);

/* Note that slurm.conf has changed */
//extern void ischeduler_reconfig(void);

#endif	/* _SLURM_IRMD_H */
