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
#include "src/common/forward.h"
#include "src/common/slurm_protocol_pack.h"

#define timeout (30 * 1000)



static void _print_data(char *data, int len)
{
        int i;
        for (i = 0; i < len; i++) {
                if ((i % 10 == 0) && (i != 0))
                        printf("\n");
                printf("%2.2x ", ((int) data[i] & 0xff));
                if (i >= 200)
                        break;
        }
        printf("\n\n");
}


int
wait_req_rsrc_offer (slurm_fd_t fd, slurm_msg_t *msg)
{
        printf("\nInside wait_req_rsrc_offer\n");
        char *buf = NULL;
        size_t buflen = 0;
        header_t header;
        int rc;
        //void *auth_cred = NULL;
        Buf buffer;

        xassert(fd >= 0);

        slurm_msg_t_init(msg);
        msg->conn_fd = fd;

        /*
         * Receive a msg. slurm_msg_recvfrom() will read the message
         *  length and allocate space on the heap for a buffer containing
         *  the message.
         */
        if (_slurm_msg_recvfrom_timeout(fd, &buf, &buflen, 0, timeout) < 0) {
                printf("\nError in receiving\n");
                forward_init(&header.forward, NULL);
                rc = errno;
                goto total_return;
        }

//#if     _DEBUG
        _print_data (buf, buflen);
//#endif
        buffer = create_buf(buf, buflen);

        if (unpack_header(&header, buffer) == SLURM_ERROR) {
                printf("\nError in unpacking header\n");
                free_buf(buffer);
                rc = SLURM_COMMUNICATIONS_RECEIVE_ERROR;
                goto total_return;
        }


        /*
         * Unpack message body
         */
        msg->protocol_version = header.version;
        msg->msg_type = header.msg_type;
        msg->flags = header.flags;

        switch(msg->msg_type) {
           case REQUEST_RESOURCE_OFFER:
                printf("\nReceived a request for a resource offer from iScheduler.\n");
                if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(msg, buffer) != SLURM_SUCCESS)) {
                     printf("\nError in buffer size and unpacking of buffer into the msg structure\n");
                     rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
                     //free_buf(buffer);
                } else {
                   rc = SLURM_SUCCESS;
                }
                break;
           default:
                slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
                printf("\nUnexpected message\n");
                rc = errno;
        }

        free_buf(buffer);
        //if (rc != SLURM_SUCCESS) goto total_return;

total_return:
        destroy_forward(&header.forward);

        slurm_seterrno(rc);
        if (rc != SLURM_SUCCESS) {
        } else {
                rc = 0;
        }
        printf("\nExiting wait_req_rsrc_offer\n");
        return rc;
}


/*
 * slurm_submit_resource_offer - issue RPC to submit a resource offer
 *                               to iScheduler plugin.
 * NOTE: free the response using slurm_free_submit_response_response_msg
 * IN resource_offer_msg - description of resource offer
 * OUT 
 * RET 0 on success, otherwise return -1 and set errno to indicate the error
 */
int
slurm_submit_resource_offer (slurm_fd_t fd, resource_offer_msg_t *req,
		        resource_offer_resp_msg_t *resp)
{
        printf("\nInside slurm_submit_resource_offer\n");
        int rc;
        char ch;
        slurm_msg_t req_msg;
        slurm_msg_t resp_msg;

        Buf buffer;
        header_t header;
        char *buf = NULL;
        size_t buflen = 0;
        //int timeout = 20 * 1000;

        req->value = 1;   // For the time being the resource offer is just a value of 1 being sent in the message.

	slurm_msg_t_init(&req_msg);
	slurm_msg_t_init(&resp_msg);

        forward_init(&req_msg.forward, NULL);
        req_msg.ret_list = NULL;
        req_msg.forward_struct = NULL;


	req_msg.msg_type = RESOURCE_OFFER;
	req_msg.data     = req;

        init_header(&header, &req_msg, req_msg.flags);

        /*
         * Pack header into buffer for transmission
         */
        buffer = init_buf(BUF_SIZE); 
        pack_header(&header, buffer);

        /*
         * Pack message into buffer
         */
        new_pack_msg(&req_msg, &header, buffer);

//#if     _DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
//#endif
        /*
         * Send message
         */

        printf("\nPress enter\n");
        scanf("%c", &ch);

        rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );
    
        if (rc < 0) {
           printf("\nProblem with sending the resource offer to iScheduler\n");
           rc = errno;
           goto total_return;           
        }
    
        printf("[IRM_DAEMON]: Sent the offer. Waiting for a mapping from jobs to this offer\n");

        free_buf(buffer);

        resp_msg.conn_fd = fd;

        /*
         * Receive a msg. slurm_msg_recvfrom() will read the message
         *  length and allocate space on the heap for a buffer containing
         *  the message.
         */
        if (_slurm_msg_recvfrom_timeout(fd, &buf, &buflen, 0, timeout) < 0) {
                forward_init(&header.forward, NULL);
                printf("\n[IRM_DAEMON]: Did not receive correct number of bytes\n");
                printf("\n[IRM_DAEMON]: iRM Daemon closing\n");
                rc = errno;
                goto total_return;
        }

//#if     _DEBUG
        _print_data (buf, buflen);
//#endif
        buffer = create_buf(buf, buflen);

        if (unpack_header(&header, buffer) == SLURM_ERROR) {
                free_buf(buffer);
                rc = SLURM_COMMUNICATIONS_RECEIVE_ERROR;
                goto total_return;
        }

        /*
         * Unpack message body
         */
        resp_msg.protocol_version = header.version;
        resp_msg.msg_type = header.msg_type;
        resp_msg.flags = header.flags;

        if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(&resp_msg, buffer) != SLURM_SUCCESS)) {
           rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
           free_buf(buffer);
           goto total_return;
        }

/*	if (rc == SLURM_SOCKET_ERROR)
		return SLURM_ERROR; */

	switch (resp_msg.msg_type) {
	/*case RESPONSE_SLURM_RC:
		rc = ((return_code_msg_t *) resp_msg.data)->return_code;
		if (rc)
			slurm_seterrno_ret(rc);
		*resp = NULL;
		break;*/
	   case RESPONSE_RESOURCE_OFFER:
                printf("\nResponse received from iScheduler for the resource offer is %d\n", ((resource_offer_resp_msg_t *)(resp_msg.data))->value);
                memcpy(resp, resp_msg.data, sizeof(resource_offer_resp_msg_t));
                rc = SLURM_SUCCESS;
		//*resp = (resource_offer_response_msg_t *) resp_msg.data;
		break;
	   default:
		slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
                printf("\nUnexpected message.\n");
                rc = errno;
	}

	//return SLURM_PROTOCOL_SUCCESS;
        //free_buf(buffer);


        free_buf(buffer);
total_return:
        printf("\nExiting slurm_submit_resource_offer\n");
        return rc;
}

void process_rsrc_offer(resource_offer_resp_msg_t *resp) {
/* Nothing as of now */
}
