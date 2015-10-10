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
#include "slurmirmd.h"

#ifdef TESTING
   resource_offer_msg_t tc_offer;
   int val;
   char str[1000];
#endif

extern int send_custom_data(slurm_fd_t, int);


static unsigned int get_random(int num) {
   unsigned int value;
   do {
      value = rand() % 7;
   } while(value == num);
   return value;
}


int print(FILE *fp, char *str)
{
   if (fp == NULL)
      return -1;
   fprintf(fp, str);
   return 0;
}


//Connect to iRM daemon via a TCP connection
int _init_comm(char *host, uint16_t port, char *agent_name) {
   slurm_fd_t fd = -1;
   slurm_addr_t addr;
   //uint16_t port = 12345;
   //char *host = "127.0.0.1";

   slurm_set_addr(&addr, port, host);
   fd = slurm_init_msg_engine(&addr);

   if (fd < 0) {
      printf("\n[%s]: Failed to initialize communication engine. Daemon will shutdown shortly\n", agent_name);
      return -1;
   }
#ifdef IRM_DEBUG
   printf("\n[%s]: Successfully initialized communication engine\n", agent_name);
#endif
   return fd;
}


int _accept_msg_conn(slurm_fd_t fd, slurm_addr_t *cli_addr) {
   struct pollfd fds[1];
   int rc;

   fds[0].fd = fd;
   fds[0].events = POLLIN;
   while ((rc = poll(fds, 1, timeout)) < 0) {
      switch (errno) {
          case EAGAIN:
          case EINTR:
               return SLURM_SOCKET_ERROR;
          case EBADF:
          case ENOMEM:
          case EINVAL:
          case EFAULT:
               error("poll: %m");
               return SLURM_SOCKET_ERROR;
          default:
               error("poll: %m. Continuing...");
      }
   }

   if (rc == 0) { /* poll timed out */
      errno = ETIMEDOUT;
      printf("\nPoll timed out\n");
      return SLURM_SOCKET_ERROR;
   } else if (fds[0].revents & POLLIN) {
      return (slurm_accept_msg_conn(fd, cli_addr));
   }
   return SLURM_SOCKET_ERROR;
}


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
#if defined (IRM_DEBUG) 
        print(log_irm_agent, "\nInside protocol_init\n");
#endif
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
#if defined (IRM_DEBUG) 
	print(log_irm_agent, "\nWaiting to receive a negotiation start msg\n");
#endif
        /*
         * Receive a msg. slurm_msg_recvfrom() will read the message
         *  length and allocate space on the heap for a buffer containing
         *  the message.
         */
        if (_slurm_msg_recvfrom_timeout(fd, &buf, &buflen, 0, timeout) < 0) {
                print(log_irm_agent, "\nError in receiving\n");
                forward_init(&header.forward, NULL);
                rc = errno;
                goto total_return;
        }

//#if     _DEBUG
#ifdef IRM_DEBUG
        _print_data (buf, buflen);
#endif
        buffer = create_buf(buf, buflen);

        if (unpack_header(&header, buffer) == SLURM_ERROR) {
                print(log_irm_agent, "\nError in unpacking header\n");
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
	   #ifdef IRM_DEBUG
                print(log_irm_agent, "\nReceived a request to start negotiation from iScheduler.\n");
	   #endif
	   #ifdef TESTING
		sprintf(str, "\n%s\n", rpc_num2string(NEGOTIATION_START));
		printf(log_irm_agent, str);
	   #endif
                if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(&msg, buffer) != SLURM_SUCCESS)) {
                     print(log_irm_agent, "\nError in buffer size and unpacking of buffer into the msg structure\n");
                     rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
                     //free_buf(buffer);
                } else {
		   slurm_free_negotiation_start_msg(msg.data);
                   rc = SLURM_SUCCESS;
                }
                break;
           default:
                print(log_irm_agent, "\nUnexpected message\n");
        	free_buf(buffer);
                slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
        }

        free_buf(buffer);
        //if (rc != SLURM_SUCCESS) goto total_return;
#ifdef TESTING
	print(log_irm_agent, "\nCalling send_custom_data from the routine for protocol initialization\n");
	val = rand() % 2;
	if (val) {
	   val = 2;
	} else {
	   val = get_random(2);
	}
	val = 2;
	rc = send_custom_data(fd, val);
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
#ifdef IRM_DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif
        /*
         * Send message
         */
        rc = _slurm_msg_sendto( resp_msg.conn_fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

        if (rc < 0) {
           print(log_irm_agent, "\nProblem with sending the response for negotiation start msg to iScheduler\n");
           rc = SLURM_ERROR;
        } else {
	#ifdef IRM_DEBUG
           print(log_irm_agent, "\nSend was successful\n");
	#endif
	#ifdef TESTING
	   sprintf(str, "\n%s\n", rpc_num2string(RESPONSE_NEGOTIATION_START));
	   print(log_irm_agent, str);
	#endif
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
#if defined IRM_DEBUG 
        print(log_irm_agent, "\nExiting protocol_init\n");
#endif
        return rc;
}


/*Once we start using the error_msg data member of the resp msg structure we will have to use dynamic memory and negotiation_end_resp_msg_t 
  *resp instead of static */
int
protocol_fini (slurm_fd_t fd)
{
#ifdef IRM_DEBUG
        print(log_irm_agent, "\nInside protocol_fini\n");
#endif
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
	print(log_irm_agent, "\nCalling send_custom_data from the routine for protocol finalization\n");
	val = rand() % 2;
        if (val) {
           val = 3;
        } else {
           val = get_random(3);
        }
	val = 3;
        rc = send_custom_data(fd, val);
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
#ifdef IRM_DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif
        /*
         * Send message
         */
        rc = _slurm_msg_sendto( resp_msg.conn_fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

        if (rc < 0) {
           print(log_irm_agent, "\nProblem with sending the response for negotiation end msg to iScheduler\n");
           rc = SLURM_ERROR;
        } else {
	#ifdef IRM_DEBUG
           print(log_irm_agent, "\nSent the response to negotiation end msg successfully\n");
	#endif
	#ifdef TESTING
	   sprintf(str, "\n%s\n", rpc_num2string(RESPONSE_NEGOTIATION_END));
	   print(log_irm_agent, str);
	#endif
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
#if defined (IRM_DEBUG)
        print(log_irm_agent, "\nExiting protocol_fini\n");
#endif
        return rc;
}



int
wait_req_rsrc_offer (slurm_fd_t fd)/*, slurm_msg_t *msg, request_resource_offer_msg_t *req_msg)*/
{
#if defined (IRM_DEBUG) 
        print(log_irm_agent, "\nInside wait_req_rsrc_offer\n");
#endif
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
                print(log_irm_agent, "\nError in receiving\n");
                forward_init(&header.forward, NULL);
                rc = errno;
                goto total_return;
        }

//#if     _DEBUG
#ifdef IRM_DEBUG
        _print_data (buf, buflen);
#endif
        buffer = create_buf(buf, buflen);

        if (unpack_header(&header, buffer) == SLURM_ERROR) {
                print(log_irm_agent, "\nError in unpacking header\n");
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
	   #ifdef IRM_DEBUG
                print(log_irm_agent, "\nReceived a request for a resource offer from iScheduler.\n");
	   #endif
	   #ifdef TESTING
		sprintf(str, "\n%s\n", rpc_num2string(REQUEST_RESOURCE_OFFER));
		print(log_irm_agent, str);
	   #endif
                if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(&msg, buffer) != SLURM_SUCCESS)) {
                     print(log_irm_agent, "\nError in buffer size and unpacking of buffer into the msg structure\n");
                     rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
                     //free_buf(buffer);
                } else {
		   //req_msg = (request_resource_offer_msg_t *)msg.data;
		   slurm_free_request_resource_offer_msg(msg.data);
                   rc = SLURM_SUCCESS;
                }
                break;
	   case NEGOTIATION_END:
	#ifdef IRM_DEBUG
                print(log_irm_agent, "\nNegotiation end message received\n");
	#endif
	#ifdef TESTING
		sprintf(str, "\n%s\n", rpc_num2string(RESPONSE_NEGOTIATION_END));
		print(log_irm_agent, str);
	#endif
                rc = protocol_fini(fd);
		slurm_free_negotiation_end_msg(msg.data);
                print(log_irm_agent, "\nStopping agent\n");
                //stop_irm_agent();
                rc = SLURM_ERROR; //To influence the caller back in controller.c to shut down irm agent based on this return value
                break;
           default:
                print(log_irm_agent, "\nUnexpected message\n");
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
#if defined (IRM_DEBUG) 
        print(log_irm_agent, "\nExiting wait_req_rsrc_offer\n");
#endif
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
#if defined (IRM_DEBUG) 
        print(log_irm_agent, "\nInside slurm_submit_resource_offer\n");
#endif
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
#ifdef IRM_DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif
        /*
         * Send message
         */

/*        printf("\nEnter any number\n");
        scanf("%d", &ch);*/

#ifdef TESTING
	// print(log_irm_agent, "\nCalling send_custom_data from the routine for submitting resource offer\n");
	val = rand() % 2;
        if (!val) {
           val = get_random(1);
        }
	val = 1;
        rc = send_custom_data(fd, val);
#else

        rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );
#endif
    
        if (rc < 0) {
           print(log_irm_agent, "\nProblem with sending the resource offer to iScheduler\n");
           rc = errno;
           free_buf(buffer);
           goto total_return;           
        }
   #ifdef IRM_DEBUG 
        print(log_irm_agent, "[IRM_DAEMON]: Sent the offer. Waiting for a mapping from jobs to this offer\n");
   #endif
   #ifdef TESTING
	sprintf(str, "\n%s\n", rpc_num2string(RESOURCE_OFFER));
	print(log_irm_agent, str);
   #endif
        free_buf(buffer);

        resp_msg.conn_fd = fd;

        /*
         * Receive a msg. slurm_msg_recvfrom() will read the message
         *  length and allocate space on the heap for a buffer containing
         *  the message.
         */
        if (_slurm_msg_recvfrom_timeout(fd, &buf, &buflen, 0, timeout) < 0) {
                forward_init(&header.forward, NULL);
                print(log_irm_agent, "\n[IRM_DAEMON]: Did not receive correct number of bytes\n");
                print(log_irm_agent, "\n[IRM_DAEMON]: iRM Daemon closing\n");
                rc = errno;
                goto total_return;
        }

//#if     _DEBUG
#ifdef IRM_DEBUG
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
	   #ifdef IRM_DEBUG
	        sprintf(str, "\nResponse received from iScheduler for the resource offer is %d\n", ((resource_offer_resp_msg_t *)(resp_msg.data))->value);
                print(log_irm_agent, str);
	   #endif
                //memcpy(resp, resp_msg.data, sizeof(resource_offer_resp_msg_t));
		*resp = (resource_offer_resp_msg_t *)(resp_msg.data);
	   #ifdef TESTING
		sprintf(str, "\n%s, Error code = %d, Error msg = %s\n", rpc_num2string(RESPONSE_RESOURCE_OFFER), (*resp)->error_code, (*resp)->error_msg);
		print(log_irm_agent, str);
	   #endif
		//slurm_free_resource_offer_resp_msg(resp_msg.data);
                rc = SLURM_SUCCESS;
		//*resp = (resource_offer_response_msg_t *) resp_msg.data;
		break;
           case NEGOTIATION_END:
	   #ifdef IRM_DEBUG
                print(log_irm_agent, "\nNegotiation end message received\n");
	   #endif
	   #ifdef TESTING
	        sprintf(str, "\n%s\n", rpc_num2string(NEGOTIATION_END));
		print(log_irm_agent, str);
	   #endif
                rc = protocol_fini(fd);
		slurm_free_negotiation_end_msg(resp_msg.data);
		print(log_irm_agent, "\nStopping agent\n");
		//stop_irm_agent();
		rc = SLURM_ERROR; //To influence the caller back in controller.c to shut down irm agent based on this return value
		break;
	   default:
                print(log_irm_agent, "\nUnexpected message.\n");
        	free_buf(buffer);
		slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
	}

	//return SLURM_PROTOCOL_SUCCESS;
        //free_buf(buffer);

        free_buf(buffer);
total_return:
#if defined (IRM_DEBUG)
        print(log_irm_agent, "\nExiting slurm_submit_resource_offer\n");
#endif
        return rc;
}

int 
process_rsrc_offer_resp(resource_offer_resp_msg_t *resp, bool final_negotiation) 
{
   int input;
   if (final_negotiation)
      return SLURM_SUCCESS;
#ifdef TESTING
   input = rand() % 2;
#else
   print(log_irm_agent, "\nEnter 1/0 to accept/reject the Map:Jobs->offer sent by iScheduler\n");
   scanf("%d", &input);
#endif
   if (!input) 
      return ESLURM_MAPPING_FROM_JOBS_TO_OFFER_REJECT;
   return SLURM_SUCCESS;
}

int 
compute_feedback(status_report_msg_t *msg) 
{
#if defined (IRM_DEBUG) 
   print(log_feedback_agent, "\nInside compute_feedback\n");
#endif
   msg->value = 1;
#if defined (IRM_DEBUG) 
   print(log_feedback_agent, "\nExiting compute_feedback\n");
#endif
   return SLURM_SUCCESS;
}


int
send_feedback(slurm_fd_t fd, status_report_msg_t *req)
{
#if defined (IRM_DEBUG)
    print(log_feedback_agent, "\nInside send_feedback\n");
#endif
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
#ifdef IRM_DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif
        /*
         * Send message
         */

        /*printf("\nEnter any number\n");
        scanf("%d", &ch);*/

#ifdef TESTING
	//print(log_feedback_agent, "\nCalling send_custom_data from the routine to send feedback/status report\n");
	val = rand() % 2;
	if(val) {
	   val = 5;
	} else {
	   val = get_random(5);
	}
	val = 5;
        rc = send_custom_data(fd, val);
#else

        rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );
#endif

        if (rc < 0) {
           print(log_feedback_agent, "\nProblem with sending the periodic feedback to iScheduler\n");
           rc = errno;
        } else {
	#if defined (IRM_DEBUG) 
           print(log_feedback_agent, "\nSend was successful\n");
	#endif
	#ifdef TESTING
	   sprintf(str, "\n%s\n", rpc_num2string(STATUS_REPORT));
	   print(log_feedback_agent, str);
	#endif
           rc = SLURM_SUCCESS;
        }
#ifdef IRM_DEBUG
   print(log_feedback_agent, "[FEEDBACK_AGENT]: Sent the feedback\n");
#endif
   free_buf(buffer);
   if (rc != SLURM_SUCCESS) {
   } else {
      rc = 0;
   }
#if defined (IRM_DEBUG) 
   printf("\nExiting send_feedback\n");
#endif
   return rc;
}


void 
*schedule_loop(void *args) 
{
#if defined (IRM_DEBUG)
   print(log_ug_agent, "\nInside schedule_loop\n");
#endif
   slurm_fd_t fd = -1;
   slurm_fd_t client_fd = -1;
   slurm_addr_t cli_addr;
   int ret_val = SLURM_SUCCESS;
   
   fd = _init_comm("127.0.0.1", 12435, "URGENT_JOBS_AGENT");

   if (fd == -1) {
      print(log_ug_agent, "\n[URGENT_JOBS_AGENT]: Unsuccessful initialization of communication engine\n");
      return NULL;
   }

   while(!stop_agent_urgent_job) {
      client_fd = _accept_msg_conn(fd, &cli_addr);

      if (client_fd != SLURM_SOCKET_ERROR) {
#if defined (IRM_DEBUG) 
         print(log_ug_agent, "\n[URGENT_JOBS_AGENT]: Accepted a connection from iScheduler's urgent jobs agent. Communications can now start\n");
#endif
      } else {
         print(log_ug_agent, "\n[URGENT_JOBS_AGENT]: Unable to receive any connection request from iScheduler's urgent jobs agent. Shutting down this agent.\n");
	 break;
      }

      ret_val = recv_send_urgent_job(client_fd);
      if (stop_agent_urgent_job) { 
         print(log_ug_agent, "\nStopping the agent for processing urgent jobs\n");
         break;
      }
#if defined (IRM_DEBUG) 
      print(log_ug_agent, "\nFinished the transaction for this urgent job successfully\n");
#endif
   }
#if defined (IRM_DEBUG) 
   print(log_ug_agent, "\nExiting schedule_loop\n");
#endif
   return NULL;
}


int
recv_send_urgent_job(slurm_fd_t fd)
{
#if defined (IRM_DEBUG) 
        print(log_ug_agent, "\nInside recv_send_urgent_job\n");
#endif
        char *buf = NULL;
        size_t buflen = 0;
        header_t header;
        int rc;
        //void *auth_cred = NULL;
	slurm_msg_t msg;
	slurm_msg_t resp_msg;
	urgent_job_resp_msg_t resp;
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
                print(log_ug_agent, "\nError in receiving\n");
                forward_init(&header.forward, NULL);
                rc = errno;
                goto total_return;
        }

//#if     _DEBUG
#ifdef IRM_DEBUG
        _print_data (buf, buflen);
#endif
        buffer = create_buf(buf, buflen);

        if (unpack_header(&header, buffer) == SLURM_ERROR) {
                print(log_ug_agent, "\nError in unpacking header\n");
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
           case URGENT_JOB:
	   #ifdef IRM_DEBUG
                print(log_ug_agent, "\nReceived an urgent job from iScheduler.\n");
	   #endif
	   #ifdef TESTING
	  	sprintf(str, "\n%s\n", rpc_num2string(URGENT_JOB));
		print(log_ug_agent, str);
	   #endif
                if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(&msg, buffer) != SLURM_SUCCESS)) {
                     print(log_ug_agent, "\nError in buffer size and unpacking of buffer into the msg structure\n");
                     rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
                     //free_buf(buffer);
                } else {
		   //req_msg = (request_resource_offer_msg_t *)msg.data;
		   print(log_ug_agent, "\nLaunching the urgent job now\n");
		   slurm_free_urgent_job_msg(msg.data);
                   rc = SLURM_SUCCESS;
                }
                break;
           default:
                print(log_ug_agent, "\nUnexpected message\n");
		free_buf(buffer);
                slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
        }

        free_buf(buffer);
        if (rc != SLURM_SUCCESS) goto total_return;
#ifdef TESTING
	//print(log_ug_agent, "\nCalling send_custom_data from the routine to send back response for urgent job\n");
	val = rand() % 2;
	if(val) {
	   val = 4;
	} else {
	   val = get_random(4);
	}
	val = 4;
        rc = send_custom_data(fd, val);
#else
	slurm_msg_t_init(&resp_msg);
	resp_msg.conn_fd = fd;

	forward_init(&resp_msg.forward, NULL);
        resp_msg.ret_list = NULL;
        resp_msg.forward_struct = NULL;


        resp_msg.msg_type = RESPONSE_URGENT_JOB;
        resp_msg.data     = &resp;

	resp.value = 0;
	resp.error_code = 0;
	resp.error_msg = (char *)NULL;

        init_header(&header, &resp_msg, resp_msg.flags);

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
#ifdef IRM_DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));	
#endif
 /*
         * Send message
         */
        rc = _slurm_msg_sendto( resp_msg.conn_fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

        if (rc < 0) {
           print(log_ug_agent, "\nProblem with sending the response for urgent job msg to iScheduler\n");
           rc = SLURM_ERROR;
        } else {
#if defined (IRM_DEBUG) 
           print(log_ug_agent, "\nSend was successful\n");
#endif
#ifdef TESTING
	   sprintf(str, "\%s\n", rpc_num2string(RESPONSE_URGENT_JOB));
	   print(log_ug_agent, str);
#endif
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
#if defined (IRM_DEBUG)
        print(log_ug_agent, "\nExiting recv_send_urgent_job\n");
#endif
        return rc;
}
