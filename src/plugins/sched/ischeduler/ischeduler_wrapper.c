/*****************************************************************************\
 *  ischeduler_wrapper.c - plugin implementing a scheduler for invasic jobs
 *****************************************************************************
 *  Copyright (C) 2015-2016 Nishanth Nagendra, Technical University of Munich.
\*****************************************************************************/

#include <stdio.h>

#include "slurm/slurm_errno.h"

#include "src/common/plugin.h"
#include "src/common/log.h"
#include "src/common/node_select.h"
#include "src/common/slurm_priority.h"
#include "src/slurmctld/job_scheduler.h"
#include "src/slurmctld/reservation.h"
#include "src/slurmctld/slurmctld.h"
#include "src/plugins/sched/ischeduler/ischeduler.h"

const char		plugin_name[]	= "SLURM IScheduler Scheduler plugin";
const char		plugin_type[]	= "sched/ischeduler";
const uint32_t		plugin_version	= 110;

/* A plugin-global errno. */
static int plugin_errno = SLURM_SUCCESS;

static pthread_t isched_thread = 0;
static pthread_mutex_t thread_flag_mutex = PTHREAD_MUTEX_INITIALIZER;

/**************************************************************************/
/*  TAG(                              init                              ) */
/**************************************************************************/
int init( void )
{
	pthread_attr_t attr;

	verbose( "sched: Invasic scheduler plugin loaded" );

	pthread_mutex_lock( &thread_flag_mutex );
	if ( isched_thread ) {
		debug2( "iScheduler thread already running, "
			"not starting another" );
		pthread_mutex_unlock( &thread_flag_mutex );
		return SLURM_ERROR;
	}

	slurm_attr_init( &attr ); 
	/* since we do a join on this later we don't make it detached */
	if (pthread_create( &isched_thread, &attr, isched_agent, NULL))
		error("Unable to start iScheduler thread: %m");
	pthread_mutex_unlock( &thread_flag_mutex );
	slurm_attr_destroy( &attr );

	return SLURM_SUCCESS;
}

/**************************************************************************/
/*  TAG(                              fini                              ) */
/**************************************************************************/
void fini( void )
{
	pthread_mutex_lock( &thread_flag_mutex );
	if ( isched_thread ) {
		verbose( "iScheduler plugin shutting down" );
                printf("\niScheduler plugin shutting down\n"); 
		stop_isched_agent();
		pthread_join(isched_thread, NULL);
		isched_thread = 0;
	}
	pthread_mutex_unlock( &thread_flag_mutex );
}

/**************************************************************************/
/* TAG(              slurm_sched_p_reconfig                             ) */
/**************************************************************************/
int slurm_sched_p_reconfig( void )
{
	//builtin_reconfig();
	return SLURM_SUCCESS;
}

/***************************************************************************/
/*  TAG(                   slurm_sched_p_schedule                        ) */
/***************************************************************************/
int
slurm_sched_p_schedule( void )
{
	return SLURM_SUCCESS;
}

/***************************************************************************/
/*  TAG(                   slurm_sched_p_newalloc                        ) */
/***************************************************************************/
int
slurm_sched_p_newalloc( struct job_record *job_ptr )
{
	return SLURM_SUCCESS;
}

/***************************************************************************/
/*  TAG(                   slurm_sched_p_freealloc                       ) */
/***************************************************************************/
int
slurm_sched_p_freealloc( struct job_record *job_ptr )
{
	return SLURM_SUCCESS;
}


/**************************************************************************/
/* TAG(                   slurm_sched_p_initial_priority                ) */
/**************************************************************************/
uint32_t
slurm_sched_p_initial_priority( uint32_t last_prio,
				     struct job_record *job_ptr )
{
	//return priority_g_set(last_prio, job_ptr);
	return SLURM_SUCCESS;
}

/**************************************************************************/
/* TAG(              slurm_sched_p_job_is_pending                       ) */
/**************************************************************************/
void slurm_sched_p_job_is_pending( void )
{
	/* Empty. */
}

/**************************************************************************/
/* TAG(              slurm_sched_p_partition_change                     ) */
/**************************************************************************/
void slurm_sched_p_partition_change( void )
{
	/* Empty. */
}

/**************************************************************************/
/* TAG(              slurm_sched_p_get_errno                            ) */
/**************************************************************************/
int slurm_sched_p_get_errno( void )
{
	return plugin_errno;
}

/**************************************************************************/
/* TAG(              slurm_sched_p_strerror                             ) */
/**************************************************************************/
char *slurm_sched_p_strerror( int errnum )
{
	return NULL;
}

/**************************************************************************/
/* TAG(              slurm_sched_p_requeue                              ) */
/**************************************************************************/
void slurm_sched_p_requeue( struct job_record *job_ptr, char *reason )
{
	/* Empty. */
}

/**************************************************************************/
/* TAG(              slurm_sched_p_get_conf                             ) */
/**************************************************************************/
char *slurm_sched_p_get_conf( void )
{
	return NULL;
}
