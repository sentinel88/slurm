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
#include "src/common/slurm_protocol_pack.h"
#include "src/common/xmalloc.h"
#include "src/common/forward.h"

#define timeout 30*1000

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
protocol_init(slurm_fd_t fd) 
{
    printf("\nInside protocol_init\n");
    int rc;
    char ch;
    slurm_msg_t req_msg;
    slurm_msg_t resp_msg;
    negotiation_start_msg_t req;

    Buf buffer;
    header_t header;
    char *buf = NULL;
    size_t buflen = 0;
    //int timeout = 20 * 1000;

    req.value = 1;   // For the time being the negotiation start is just a value of 1 being sent in the message.

    slurm_msg_t_init(&req_msg);
    slurm_msg_t_init(&resp_msg);

    forward_init(&req_msg.forward, NULL);
    req_msg.ret_list = NULL;
    req_msg.forward_struct = NULL;


    req_msg.msg_type = NEGOTIATION_START;
    req_msg.data     = &req;

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
       printf("\nProblem with sending the negotiation start message to iRM\n");
       rc = errno;
       free_buf(buffer);
       goto total_return;
    }

    printf("[iSCHED]: Sent the start msg. Waiting for a response.\n");

    free_buf(buffer);

    resp_msg.conn_fd = fd;

    /*
     * Receive a msg. slurm_msg_recvfrom() will read the message
     *  length and allocate space on the heap for a buffer containing
     *  the message.
     */
    if (_slurm_msg_recvfrom_timeout(fd, &buf, &buflen, 0, timeout) < 0) {
	forward_init(&header.forward, NULL);
	printf("\n[iSCHED]: Did not receive the correct response to start negotiation.\n");
	printf("\n[iSCHED]: Need to exit.\n");
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

/*      if (rc == SLURM_SOCKET_ERROR)
	    return SLURM_ERROR; */

    switch (resp_msg.msg_type) {
        /*case RESPONSE_SLURM_RC:
                rc = ((return_code_msg_t *) resp_msg.data)->return_code;
if (rc)
		slurm_seterrno_ret(rc);
	    *resp = NULL;
	    break;*/
       case RESPONSE_NEGOTIATION_START:
	    printf("\nResponse received from iRM for the start of negotiation. Value is %d\n", ((negotiation_start_resp_msg_t *)(resp_msg.data))->value);
	    //memcpy(resp, resp_msg.data, sizeof(resource_offer_resp_msg_t));
	    slurm_free_negotiation_start_resp_msg(resp_msg.data);
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
    printf("\nExiting protocol_init\n");
    return rc;
}


int 
protocol_fini(slurm_fd_t fd) 
{
    printf("\nInside protocol_fini\n");
    int rc;
    int ch;
    slurm_msg_t req_msg;
    slurm_msg_t resp_msg;
    negotiation_end_msg_t req;

    Buf buffer;
    header_t header;
    char *buf = NULL;
    size_t buflen = 0;
    //int timeout = 20 * 1000;

    req.value = 1;   // For the time being the negotiation start is just a value of 1 being sent in the message.

    slurm_msg_t_init(&req_msg);
    slurm_msg_t_init(&resp_msg);

    forward_init(&req_msg.forward, NULL);
    req_msg.ret_list = NULL;
    req_msg.forward_struct = NULL;


    req_msg.msg_type = NEGOTIATION_END;
    req_msg.data     = &req;

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

    printf("\nEnter any number\n");
    scanf("%d", &ch);

    rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
			    get_buf_offset(buffer),
			    SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

    if (rc < 0) {
       printf("\nProblem with sending the negotiation end message to iRM\n");
       rc = errno;
       free_buf(buffer);
       goto total_return;
    }

    printf("[iSCHED]: Sent the end msg. Waiting for a response.\n");

    free_buf(buffer);

    resp_msg.conn_fd = fd;

    /*
     * Receive a msg. slurm_msg_recvfrom() will read the message
     *  length and allocate space on the heap for a buffer containing
     *  the message.
     */
    if (_slurm_msg_recvfrom_timeout(fd, &buf, &buflen, 0, timeout) < 0) {
        forward_init(&header.forward, NULL);
        printf("\n[iSCHED]: Did not receive the correct response to end negotiation.\n");
        printf("\n[iSCHED]: Need to exit.\n");
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

/*      if (rc == SLURM_SOCKET_ERROR)
            return SLURM_ERROR; */

    switch (resp_msg.msg_type) {
        /*case RESPONSE_SLURM_RC:
                rc = ((return_code_msg_t *) resp_msg.data)->return_code;
if (rc)
                slurm_seterrno_ret(rc);
            *resp = NULL;
            break;*/
       case RESPONSE_NEGOTIATION_END:
            printf("\nResponse received from iRM for the end of negotiation. Value is %d\n", ((negotiation_end_resp_msg_t *)(resp_msg.data))->value);
            //memcpy(resp, resp_msg.data, sizeof(resource_offer_resp_msg_t));
	    slurm_free_negotiation_end_resp_msg(resp_msg.data);
            rc = SLURM_SUCCESS;
            //*resp = (resource_offer_response_msg_t *) resp_msg.data;
            break;
       default:
            slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
            printf("\nUnexpected message.\n");
            rc = errno;
    }

    printf("[iSCHED]: Received the response for the end msg.\n");

    free_buf(buffer);
total_return:
    printf("\nExiting protocol_fini\n");
    return rc;
}


int
request_resource_offer (slurm_fd_t fd)
{
        printf("\nInside slurm_request_resource_offer\n");
        int rc;
        int ch;
        slurm_msg_t req_msg;
        request_resource_offer_msg_t req;
        Buf buffer;
        header_t header;
        //char *buf = NULL;
        //size_t buflen = 0;
        //int timeout = 20 * 1000;

        //msg->data = malloc(sizeof(request_resource_offer_msg_t));

        req.value = 1;   // For the time being the request resource offer is just a value of 1 being sent in the message.

        printf("\nJust enter a number to start preparing to send a request for resource offer\n");
        scanf("%d", &ch);

        slurm_msg_t_init(&req_msg);

        forward_init(&req_msg.forward, NULL);
        req_msg.ret_list = NULL;
        req_msg.forward_struct = NULL;


        req_msg.msg_type = REQUEST_RESOURCE_OFFER;
        req_msg.data     = &req;

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

        rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

        if (rc < 0) {
           printf("\nProblem with sending the request resource offer to iRM\n");
           rc = errno;
        } else {
           printf("[IRM_AGENT]: Sent the request for a resource offer.\n");
           rc = SLURM_SUCCESS;
        }

        free_buf(buffer);

        printf("\nExiting slurm_request_resource_offer\n");
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
//Similar to slurm_receive_msg
int
receive_resource_offer (slurm_fd_t fd, slurm_msg_t *msg) 
{
        printf("\nInside isched_recv_rsrc_offer\n");
        //slurm_msg_t msg;
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
           case RESOURCE_OFFER:  // Do not free msg->data as we need the complete resource offer msg back in the caller for further processing
                printf("\nReceived a resource offer from iRM daemon\n");
                if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(msg, buffer) != SLURM_SUCCESS)) {
                     printf("\nError in buffer size and unpacking of buffer into the msg structure\n");
                     rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
                     //free_buf(buffer);
                } else {
                   //memcpy(res_off_msg, msg.data, sizeof(resource_offer_msg_t));
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
        if (rc == SLURM_SUCCESS)
           printf("\n[IRM_AGENT]: Received a resource offer from iRM daemon which is %d\n", ( (resource_offer_msg_t *) (msg->data))->value);

total_return:
        destroy_forward(&header.forward);

        slurm_seterrno(rc);
        if (rc != SLURM_SUCCESS) {
        } else {
                rc = 0;
        }
        printf("\nExiting isched_recv_rsrc_offer\n");
        return rc;
}

int
process_resource_offer (resource_offer_msg_t *msg, uint16_t *buf_val)
		        
{
        int input = -1;
        int choice = -1;
        printf("\nIs the job queue empty?? if yes, then we send a negative response for the resource offer. Enter 1/0 for empty/non-empty job queue\n");
        scanf("%d", &choice);
 
        if (!choice) {
           printf("\nEnter your choice 1/0 on whether to accept/reject the resource offer and 2 to end the negotiation\n");
           scanf("%d", &input);

           if (input == 1) {
              printf("\nSuccessfully mapped jobs to this offer and sending the list of jobs to be launched\n");
              // *buf_val = htons(1);
              *buf_val = 1;
              //memcpy(buf, &buf_val, sizeof(buf_val));
           } else if (input == 0){
              printf("\nOffer not accepted. Sending back a negative response\n");
              // *buf_val = htons(0);
              *buf_val = 0;
              //memcpy(buf, &buf_val, sizeof(buf_val));
           } else if (input == 2) {
	      printf("\nGoing to end this negotiation\n");
              *buf_val = 400;
           } else {
	      printf("\nWrong choice. Sending back a positive response to the resource offer\n");
              *buf_val = 1;
	   }
        } else {
           *buf_val = 500; 
        }

        return 0;
}

int send_resource_offer_resp(slurm_msg_t *msg, char *buf) 
{
        //char *buf = NULL;
        //size_t buflen = 0;
        header_t header;
        int rc;
        resource_offer_resp_msg_t offer_resp_msg;
        slurm_msg_t resp_msg;
        //void *auth_cred = NULL;
        Buf buffer;
        //int choice = 1;
        char err_msg[256] = "Job queue is empty";

        printf("\nInside isched_send_irm_msg\n");

        if (msg->conn_fd < 0) {
                slurm_seterrno(ENOTCONN);
                return SLURM_ERROR;
        }


        offer_resp_msg.value = *(uint16_t *)(buf);

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

        offer_resp_msg.error_code = 0; 
        offer_resp_msg.error_msg = NULL;
  
        if (*(uint16_t *)(buf) == 500) {
           offer_resp_msg.error_code = 500;
           offer_resp_msg.error_msg = err_msg;
        }

        init_header(&header, &resp_msg, msg->flags);

        /*
         * Pack header into buffer for transmission
         */
        buffer = init_buf(BUF_SIZE);
        pack_header(&header, buffer); 

/*
         * Pack message into buffer
         */
        new_pack_msg(&resp_msg, &header, buffer);

//#if     _DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
//#endif
        /*
         * Send message
         */
        rc = _slurm_msg_sendto( msg->conn_fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

        if (rc < 0) {
           printf("\nProblem with sending the resource offer response msg back to iRM Daemon\n");
           rc = SLURM_ERROR;
        } else {
           printf("\nSend was successful\n");
           rc = SLURM_SUCCESS;
        }

        free_buf(buffer);
	xfree(offer_resp_msg.error_msg);
        printf("\nExiting isched_send_irm_msg\n");
        return rc;
}
