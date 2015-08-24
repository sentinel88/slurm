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
#include "src/common/xmalloc.h"

#define timeout (30 * 1000)

//#define TESTING 1

extern int send_custom_data(slurm_fd_t);

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
protocol_init (slurm_fd_t fd)
{
        printf("\nInside protocol_init\n");
        char *buf = NULL;
        size_t buflen = 0;
        header_t header;
        int rc;
        slurm_msg_t msg;
        slurm_msg_t resp_msg;
        Buf buffer;
        negotiation_start_resp_msg_t resp;
	//char err_msg[256] = "Negotiation cannot be started";

        xassert(fd >= 0);

        slurm_msg_t_init(&msg);
        msg.conn_fd = fd;
	printf("\nWaiting to receive a negotiation start msg\n");
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
        msg.protocol_version = header.version;
        msg.msg_type = header.msg_type;
        msg.flags = header.flags;

        switch(msg.msg_type) {
           case NEGOTIATION_START:
                printf("\nReceived a request to start negotiation from iScheduler.\n");
                if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(&msg, buffer) != SLURM_SUCCESS)) {
                     printf("\nError in buffer size and unpacking of buffer into the msg structure\n");
                     rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
                     //free_buf(buffer);
                } else {
		   slurm_free_negotiation_start_msg(msg.data);
                   rc = SLURM_SUCCESS;
                }
                break;
           default:
                printf("\nUnexpected message\n");
        	free_buf(buffer);
                slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
        }

        free_buf(buffer);
        //if (rc != SLURM_SUCCESS) goto total_return;
#ifdef _TESTING
	rc = send_custom_data(fd);
#else
        slurm_msg_t_init(&resp_msg);

	resp_msg.conn_fd = fd;
        resp_msg.msg_type = RESPONSE_NEGOTIATION_START;
        resp_msg.data     = &resp;
        resp_msg.protocol_version = msg.protocol_version;
        resp_msg.address  = msg.address;
        resp_msg.flags = msg.flags;
        resp_msg.forward = msg.forward;
        resp_msg.forward_struct = msg.forward_struct;
        resp_msg.ret_list = msg.ret_list;
        resp_msg.orig_addr = msg.orig_addr;

	resp.error_code = 0;
	resp.error_msg = (char *)NULL;

	/*if (*(uint16_t *)(buf) == 500) {
           resp.error_code = 500;
           resp.error_msg = err_msg;
        }*/

        init_header(&header, &resp_msg, resp_msg.flags);

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
        rc = _slurm_msg_sendto( resp_msg.conn_fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

        if (rc < 0) {
           printf("\nProblem with sending the response for negotiation start msg to iScheduler\n");
           rc = SLURM_ERROR;
        } else {
           printf("\nSend was successful\n");
           rc = SLURM_SUCCESS;
        }

	free_buf(buffer);
#endif
total_return:
        destroy_forward(&header.forward);
        slurm_seterrno(rc);
        if (rc != SLURM_SUCCESS) {
        } else {
                rc = 0;
        }
//#endif
        printf("\nExiting protocol_init\n");
        return rc;
}


/*Once we start using the error_msg data member of the resp msg structure we will have to use dynamic memory and negotiation_end_resp_msg_t 
  *resp instead of static */
int
protocol_fini (slurm_fd_t fd)
{
        printf("\nInside protocol_fini\n");
        char *buf = NULL;
        size_t buflen = 0;
        header_t header;
        int rc;
        //slurm_msg_t msg;
        slurm_msg_t resp_msg;
        Buf buffer;
        negotiation_end_resp_msg_t resp;
	//char err_msg[256] = "Unable to terminate negotiation";

        xassert(fd >= 0);
/*
        slurm_msg_t_init(&msg);
        msg.conn_fd = fd;
	printf("\nWaiting to receive a negotiation end msg\n");
        *
         * Receive a msg. slurm_msg_recvfrom() will read the message
         *  length and allocate space on the heap for a buffer containing
         *  the message.
         */
  /*      if (_slurm_msg_recvfrom_timeout(fd, &buf, &buflen, 0, timeout) < 0) {
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


        *
         * Unpack message body
         */
    /*    msg.protocol_version = header.version;
        msg.msg_type = header.msg_type;
        msg.flags = header.flags;

        switch(msg.msg_type) {
           case NEGOTIATION_END:
                printf("\nReceived a request to start negotiation from iScheduler.\n");
                if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(&msg, buffer) != SLURM_SUCCESS)) {
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

        free_buf(buffer);*/
        //if (rc != SLURM_SUCCESS) goto total_return;
#ifdef TESTING
	rc = send_custom_data(fd);
#else
        slurm_msg_t_init(&resp_msg);
        resp_msg.data     = &resp;
        forward_init(&resp_msg.forward, NULL);
        resp_msg.ret_list = NULL;
        resp_msg.forward_struct = NULL;
        resp_msg.msg_type = RESPONSE_NEGOTIATION_END;
	resp_msg.conn_fd = fd;

        /*resp_msg.msg_type = RESPONSE_NEGOTIATION_END;
        resp_msg.data     = &resp;
        resp_msg.protocol_version = msg->protocol_version;
        resp_msg.address  = msg->address;
        resp_msg.flags = msg->flags;
        resp_msg.forward = msg->forward;
        resp_msg.forward_struct = msg->forward_struct;
        resp_msg.ret_list = msg->ret_list;
        resp_msg.orig_addr = msg->orig_addr;*/

	resp.error_code = 0;
	resp.error_msg = (char *)NULL;

	/*if (*(uint16_t *)(buf) == 500) {
           resp.error_code = 500;
           resp.error_msg = err_msg;
        }*/

        init_header(&header, &resp_msg, resp_msg.flags);

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
        rc = _slurm_msg_sendto( resp_msg.conn_fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

        if (rc < 0) {
           printf("\nProblem with sending the response for negotiation end msg to iScheduler\n");
           rc = SLURM_ERROR;
        } else {
           printf("\nSend was successful\n");
           rc = SLURM_SUCCESS;
        }

	free_buf(buffer);
	xfree(resp.error_msg);
#endif
total_return:
        destroy_forward(&header.forward);
        slurm_seterrno(rc);
        if (rc != SLURM_SUCCESS) {
        } else {
                rc = 0;
        }
        printf("\nExiting protocol_fini\n");
        return rc;
}



int
wait_req_rsrc_offer (slurm_fd_t fd)/*, slurm_msg_t *msg, request_resource_offer_msg_t *req_msg)*/
{
        printf("\nInside wait_req_rsrc_offer\n");
        char *buf = NULL;
        size_t buflen = 0;
        header_t header;
        int rc;
        //void *auth_cred = NULL;
	slurm_msg_t msg;
        Buf buffer;

        xassert(fd >= 0);

        slurm_msg_t_init(&msg);
        msg.conn_fd = fd;

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
        msg.protocol_version = header.version;
        msg.msg_type = header.msg_type;
        msg.flags = header.flags;

        switch(msg.msg_type) {
           case REQUEST_RESOURCE_OFFER:
                printf("\nReceived a request for a resource offer from iScheduler.\n");
                if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(&msg, buffer) != SLURM_SUCCESS)) {
                     printf("\nError in buffer size and unpacking of buffer into the msg structure\n");
                     rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
                     //free_buf(buffer);
                } else {
		   //req_msg = (request_resource_offer_msg_t *)msg.data;
		   slurm_free_request_resource_offer_msg(msg.data);
                   rc = SLURM_SUCCESS;
                }
                break;
	   case NEGOTIATION_END:
                printf("\nNegotiation end message received\n");
                rc = protocol_fini(fd);
		slurm_free_negotiation_end_msg(msg.data);
                printf("\nStopping agent\n");
                //stop_irm_agent();
                rc = SLURM_ERROR; //To influence the caller back in controller.c to shut down irm agent based on this return value
                break;
           default:
                printf("\nUnexpected message\n");
		free_buf(buffer);
                slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
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
		        resource_offer_resp_msg_t **resp)
{
        printf("\nInside slurm_submit_resource_offer\n");
        int rc;
        int ch;
        slurm_msg_t req_msg;
        slurm_msg_t resp_msg;

        Buf buffer;
        header_t header;
        char *buf = NULL;
        size_t buflen = 0;
        //int timeout = 20 * 1000;

        req->value = 1;   // For the time being the resource offer is just a value of 1 being sent in the message.
	//req->error_code = 0;
	//req->error_msg = (char *)NULL;

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

        printf("\nEnter any number\n");
        scanf("%d", &ch);

#ifdef TESTING
	rc = send_custom_data(fd);
#else

        rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );
#endif
    
        if (rc < 0) {
           printf("\nProblem with sending the resource offer to iScheduler\n");
           rc = errno;
           free_buf(buffer);
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
                //memcpy(resp, resp_msg.data, sizeof(resource_offer_resp_msg_t));
		*resp = (resource_offer_resp_msg_t *)(resp_msg.data);
		printf("\nError code = %d, Error msg = %s\n", (*resp)->error_code, (*resp)->error_msg);
		//slurm_free_resource_offer_resp_msg(resp_msg.data);
                rc = SLURM_SUCCESS;
		//*resp = (resource_offer_response_msg_t *) resp_msg.data;
		break;
           case NEGOTIATION_END:
                printf("\nNegotiation end message received\n");
                rc = protocol_fini(fd);
		slurm_free_negotiation_end_msg(resp_msg.data);
		printf("\nStopping agent\n");
		//stop_irm_agent();
		rc = SLURM_ERROR; //To influence the caller back in controller.c to shut down irm agent based on this return value
		break;
	   default:
                printf("\nUnexpected message.\n");
        	free_buf(buffer);
		slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
	}

	//return SLURM_PROTOCOL_SUCCESS;
        //free_buf(buffer);

        free_buf(buffer);
total_return:
        printf("\nExiting slurm_submit_resource_offer\n");
        return rc;
}

int 
process_rsrc_offer_resp(resource_offer_resp_msg_t *resp, bool final_negotiation) 
{
   int input;
   if (final_negotiation)
      return SLURM_SUCCESS;
   printf("\nEnter 1/0 to accept/reject the Map:Jobs->offer sent by iScheduler\n");
   scanf("%d", &input);
   if (!input) 
      return ESLURM_MAPPING_FROM_JOBS_TO_OFFER_REJECT;
   return SLURM_SUCCESS;
}

int 
compute_feedback(status_report_msg_t *msg) 
{
   printf("\nInside compute_feedback\n");
   msg->value = 1;
   printf("\nExiting compute_feedback\n");
   return SLURM_SUCCESS;
}


int
send_feedback(slurm_fd_t fd, status_report_msg_t *req)
{
    printf("\nInside send_feedback\n");
    int rc;
    slurm_msg_t req_msg;

    Buf buffer;
    header_t header;
    char *buf = NULL;
    size_t buflen = 0;
    //int timeout = 20 * 1000;

    //req->error_code = 0;
    //req->error_msg = (char *)NULL;

    slurm_msg_t_init(&req_msg);

    forward_init(&req_msg.forward, NULL);
    req_msg.ret_list = NULL;
    req_msg.forward_struct = NULL;


    req_msg.msg_type = STATUS_REPORT;
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

        /*printf("\nEnter any number\n");
        scanf("%d", &ch);*/

#ifdef _TESTING
        rc = send_custom_data(fd);
#else

        rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );
#endif

        if (rc < 0) {
           printf("\nProblem with sending the periodic feedback to iScheduler\n");
           rc = errno;
        } else {
           printf("\nSend was successful\n");
           rc = SLURM_SUCCESS;
        }

   printf("[FEEDBACK_AGENT]: Sent the feedback\n");
   free_buf(buffer);
   if (rc != SLURM_SUCCESS) {
   } else {
      rc = 0;
   }
   printf("\nExiting send_feedback\n");
   return rc;
}

