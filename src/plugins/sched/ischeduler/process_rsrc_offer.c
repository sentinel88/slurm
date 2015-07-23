/*****************************************************************************\
 *  process_rsrc_offer.c - process the resource offer sent by iRM Daemon and 
 *                         send back a response which is a Map:Jobs->Offer.
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
//Similar to slurm_receive_msg
int
isched_recv_rsrc_offer (slurm_fd_t fd, slurm_msg_t *msg, int timeout)/*resource_offer_msg_t *req,
		        resource_offer_resp_msg_t **resp*/
{
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
                forward_init(&header.forward, NULL);
                rc = errno;
                goto total_return;
        }

#if     _DEBUG
        _print_data (buf, buflen);
#endif
        buffer = create_buf(buf, buflen);

        if (unpack_header(&header, buffer) == SLURM_ERROR) {
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

        if ((header.body_length > remaining_buf(buffer)) || (unpack(msg, buffer) != SLURM_SUCCESS)) {
           rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
           free_buf(buffer);
           goto total_return;
        }

        free_buf(buffer);
        rc = SLURM_SUCCESS;

total_return:
        destroy_forward(&header.forward);

        slurm_seterrno(rc);
        if (rc != SLURM_SUCCESS) {
        } else {
                rc = 0;
        }
        return rc;
}

int
process_rsrc_offer (resource_offer_msg_t *msg, uint16_t *buf_val)
		        
{
        int input = -1;
        printf("\nEnter your choice 1/0 on whether to accept/reject the resource offer\n");
        scanf("%d", &input);

        if (input == 1) {
           printf("\nSuccessfully mapped jobs to this offer and sending the list of jobs to be launched\n");
           *buf_val = htons(1);
           //memcpy(buf, &buf_val, sizeof(buf_val));
        } else {
           printf("\nOffer not accepted. Sending back a negative response\n");
           *buf_val = htons(0);
           //memcpy(buf, &buf_val, sizeof(buf_val));
        }

        return 0;
}

int isched_send_irm_msg(slurm_msg_t *msg, char *buf, int timeout) 
{
        char *buf = NULL;
        size_t buflen = 0;
        header_t header;
        int rc;
        resource_offer_resp_msg_t offer_resp_msg;
        //void *auth_cred = NULL;
        Buf buffer;

        slurm_msg_t resp_msg;

        if (msg->conn_fd < 0) {
                slurm_seterrno(ENOTCONN);
                return SLURM_ERROR;
        }

        offer_resp_msg->value = *(uint16_t *)(buf);

        slurm_msg_t_init(&resp_msg);
        resp_msg.protocol_version = msg->protocol_version;
        resp_msg.address  = msg->address;
        resp_msg.msg_type = RESPONSE_RESOURCE_OFFER;
        resp_msg.data     = &offer_resp_msg;
        resp_msg.flags = msg->flags;
        resp_msg.forward = msg->forward;
        resp_msg.forward_struct = msg->forward_struct;
        resp_msg.ret_list = msg->ret_list;
        resp_msg.orig_addr = msg->orig_addr;


        init_header(&header, msg, msg->flags);

        /*
         * Pack header into buffer for transmission
         */
        buffer = init_buf(BUF_SIZE);
        pack_header(&header, buffer); 

/*
         * Pack message into buffer
         */
        _pack_msg(msg, &header, buffer);

#if     _DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif
        /*
         * Send message
         */
        rc = _slurm_msg_sendto( msg->conn_fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

        if (rc < 0) {
           printf("\nProblem with sending the resource offer response msg back to iRM Daemon\n");
        }

        free_buf(buffer);
        return rc;
}
