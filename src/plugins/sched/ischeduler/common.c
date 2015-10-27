/*****************************************************************************\
 *  common.c - 
 *****************************************************************************
 *  Copyright (C) 2015-2016 
 *  Produced at Technical University of Munich, Germany
 *  Written by Nishanth Nagendra <ga38sok@in.tum.de>.
\*****************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#ifndef __USE_XOPEN_EXTENDED
extern pid_t getsid(pid_t pid);         /* missing from <unistd.h> */
#endif

#include "slurm/slurm.h"

#include "src/common/read_config.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_protocol_pack.h"
#include "src/common/xmalloc.h"
#include "src/common/forward.h"
#include "src/slurmctld/slurmctld.h"
#include "ischeduler.h"

extern List build_invasic_job_queue(bool clear_start, bool backfill)
{
	List job_queue;
	ListIterator job_iterator, part_iterator;
	struct job_record *job_ptr = NULL;
	struct part_record *part_ptr;
	int reason;
	struct timeval start_tv = {0, 0};
	int tested_jobs = 0;
	char str[1000];

	print(log_irm_agent, "Inside build_job_queue\n");

	job_queue = list_create(_job_queue_rec_del);
	job_iterator = list_iterator_create(job_list);
	while ((job_ptr = (struct job_record *) list_next(job_iterator))) {
		   print(log_irm_agent, "Inside loop\n");
                //if(strcmp(job_ptr->partition, "invasic") == 0) {
		   print(log_irm_agent, "Invasic job: ");
		   sprintf(str, "ID: %d, Partition: %s\n", job_ptr->job_id, job_ptr->partition);
		   print(log_irm_agent, str);
		   _job_queue_append(job_queue, job_ptr,
                                          job_ptr->part_ptr, job_ptr->priority);
		//}
        }
        list_iterator_destroy(job_iterator);
	print(log_irm_agent, "Exiting build_job_queue\n");

        return job_queue;
}

