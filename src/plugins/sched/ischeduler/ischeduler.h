/*****************************************************************************\
 *  ischeduler.h - header for Invasic scheduler plugin.
 *****************************************************************************
 *  Copyright (C) 2003-2007 The Regents of the University of California.
 *  Copyright (C) 2008-2010 Lawrence Livermore National Security.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Morris Jette <jette1@llnl.gov>
 *  CODE-OCEC-09-009. All rights reserved.
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/


#ifndef _SLURM_ISCHEDULER_H
#define _SLURM_ISCHEDULER_H

#include "src/common/xmalloc.h"

/* ischeduler_agent - detached thread periodically when pending jobs can start */
extern void *isched_agent(void *);

/* Terminate builtin_agent */
extern void stop_isched_agent(void);

/* Note that slurm.conf has changed */
extern void ischeduler_reconfig(void);

extern void stop_ping_agent(void);

extern void *ping_agent(void *);

extern void stop_feedback_agent(void);

extern void *feedback_agent(void *);

extern void stop_irm_agent(void);

extern void *irm_agent(void *);

extern int receive_resource_offer (slurm_fd_t, slurm_msg_t *);

extern int process_resource_offer (resource_offer_msg_t *, uint16_t *, int *);

extern int send_resource_offer_resp(slurm_msg_t *, char *); 

extern int request_resource_offer(slurm_fd_t);

extern int protocol_init(slurm_fd_t);

extern int protocol_fini(slurm_fd_t);

extern int send_custom_data(slurm_fd_t);

extern int receive_feedback(slurm_fd_t, slurm_msg_t *);

extern int process_feedback(status_report_msg_t *);

extern int _connect_to_irmd(char *, uint16_t, bool *, int, char *);

extern int send_recv_urgent_job(slurm_fd_t, slurm_msg_t *);

extern int schedule_urgent_jobs(void);

extern void *schedule_loop(void *);

#define timeout (20*1000)

#define _TESTING 0

//extern bool urgent_jobs;
extern bool stop_ping_agent;
extern pthread_mutex_t urgent_lock;

#endif	/* _SLURM_ISCHEDULER_H */
