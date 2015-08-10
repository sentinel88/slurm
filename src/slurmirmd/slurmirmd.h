/*****************************************************************************\
 *  slurmirmd.h - iRM Daemon.
 *****************************************************************************
 *  Copyright (C) 2015-2016 Nishanth Nagendra, Technical University of Munich.
\*****************************************************************************/


#ifndef _SLURM_IRMD_H
#define _SLURM_IRMD_H


extern int slurm_submit_resource_offer(slurm_fd_t fd, resource_offer_msg_t *, resource_offer_resp_msg_t *);

extern int wait_req_rsrc_offer (slurm_fd_t, slurm_msg_t *);

#define timeout (30*1000)
/* ischeduler_agent - detached thread periodically when pending jobs can start */
//extern void *isched_agent(void *args);

/* Terminate irm_agent */
//extern void stop_irm_agent(void);

/* Note that slurm.conf has changed */
//extern void ischeduler_reconfig(void);

#endif	/* _SLURM_IRMD_H */
