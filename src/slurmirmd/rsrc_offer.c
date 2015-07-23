/*****************************************************************************\
 *  rsrc_offer.c - submit a resource offer to iScheduler
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
extern pid_t getsid(pid_t pid);		/* missing from <unistd.h> */
#endif

#include "slurm/slurm.h"

#include "src/common/read_config.h"
#include "src/common/slurm_protocol_api.h"

/*
 * slurm_submit_resource_offer - issue RPC to submit a resource offer
 *                               to iScheduler plugin.
 * NOTE: free the response using slurm_free_submit_response_response_msg
 * IN resource_offer_msg - description of resource offer
 * OUT 
 * RET 0 on success, otherwise return -1 and set errno to indicate the error
 */
int
slurm_submit_resource_offer (resource_offer_msg_t *req,
		        resource_offer_resp_msg_t **resp)
{
        int rc;
        slurm_msg_t req_msg;
        slurm_msg_t resp_msg;

	slurm_msg_t_init(&req_msg);
	slurm_msg_t_init(&resp_msg);


	req_msg.msg_type = RESOURCE_OFFER;
	req_msg.data     = req;

	rc = slurm_send_recv_ischeduler_msg(&req_msg, &resp_msg);

	if (rc == SLURM_SOCKET_ERROR)
		return SLURM_ERROR;

	switch (resp_msg.msg_type) {
	case RESPONSE_SLURM_RC:
		rc = ((return_code_msg_t *) resp_msg.data)->return_code;
		if (rc)
			slurm_seterrno_ret(rc);
		*resp = NULL;
		break;
	case RESPONSE_RESOURCE_OFFER:
		*resp = (resource_offer_response_msg_t *) resp_msg.data;
		break;
	default:
		slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
	}

	return SLURM_PROTOCOL_SUCCESS;
}
