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
#include "ischeduler.h"

#define MAX_NEGOTIATION_ATTEMPTS 5

#ifdef TESTING
   extern resource_offer_resp_msg_t tc_offer_resp;
   int val;
   char str[1000];
#endif

bool stop_agent_sleep = false;
static pthread_mutex_t term_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  term_cond = PTHREAD_COND_INITIALIZER;

static unsigned int get_random(int num) {
   unsigned int value;
   do {
      value = rand() % 7;
   } while(value == num);
   return value;
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


int print(FILE *fp, char *str) 
{
   if (fp == NULL)
      return -1;
   fprintf(fp, "\n");
   fprintf(fp, str);
   fprintf(fp, "\n");
   return 0;
}


void stop_sleep_agent(void)
{
        pthread_mutex_lock(&term_lock);
        stop_agent_sleep = true;
#ifdef ISCHED_DEBUG
	printf("\nStopping timer agent\n");
#endif
        pthread_cond_signal(&term_cond);
        pthread_mutex_unlock(&term_lock);
}


static void _my_sleep(int secs)
{
        struct timespec ts = {0, 0};
        struct timeval now;

        gettimeofday(&now, NULL);
        ts.tv_sec = now.tv_sec + secs;
        ts.tv_nsec = now.tv_usec * 1000;
        pthread_mutex_lock(&term_lock);
        if (!stop_agent_sleep)
                pthread_cond_timedwait(&term_cond, &term_lock, &ts);
        pthread_mutex_unlock(&term_lock);
}



//Connect to iRM daemon via a TCP connection
int _connect_to_irmd(char *host, uint16_t port, bool *flag, int sleep_interval, char *agent_name) {
   slurm_fd_t fd = -1;
   slurm_addr_t irm_address;
   //port = 12345;
   //host = "127.0.0.1";

   _slurm_set_addr_char(&irm_address, port, host);
   //while (!(*flag)) {
   while (!stop_agent_sleep) {
      fd = slurm_open_msg_conn(&irm_address);
      if (fd < 0) {
         printf("\n[%s]: Failed to contact iRM daemon.\n", agent_name);
         //return -1;
      } else {
         printf("\n[%s]: Successfully connected with iRM daemon.\n", agent_name);
         break;
      }
      _my_sleep(sleep_interval);
      //sleep(sleep_interval);
   }
   /*if (!stop_agent)
      printf("\n[IRM_AGENT]: Successfully connected to iRM daemon\n");*/
   return fd;
}



int 
protocol_init(slurm_fd_t fd) 
{
#if defined (ISCHED_DEBUG) 
    print(log_irm_agent, "\nInside protocol_init\n");
#endif
    int rc;
    char ch;
    slurm_msg_t req_msg;
    slurm_msg_t resp_msg;
    negotiation_start_msg_t req;

    Buf buffer;
    header_t header;
    char *buf = NULL;
    size_t buflen = 0;

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
#ifdef ISCHED_DEBUG
    _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif
    /*
     * Send message
     */

    /*printf("\nPress enter\n");
    scanf("%c", &ch);*/

#ifdef TESTING
    val = rand() % 2;
    if(val) {
       val = 3;
    } else {
       val = get_random(3);
    }
    val = 3;
    rc = send_custom_data(fd, val);
#else
    rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
			    get_buf_offset(buffer),
			    SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );
#endif
    if (rc < 0) {
       print(log_irm_agent, "\nProblem with sending the negotiation start message to iRM\n");
       rc = errno;
       free_buf(buffer);
       goto total_return;
    }
#ifdef ISCHED_DEBUG
    print(log_irm_agent, "[iSCHED]: Sent the negotiation start msg. Waiting for a response.\n");
#endif
#ifdef TESTING
    print(log_irm_agent, rpc_num2string(NEGOTIATION_START));
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
	print(log_irm_agent, "\n[iSCHED]: Did not receive the correct response to start negotiation.\n");
	print(log_irm_agent, "\n[iSCHED]: Need to exit.\n");
	rc = errno;
	goto total_return;
    }

////#if     _DEBUG
#ifdef ISCHED_DEBUG
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


    switch (resp_msg.msg_type) {
       case RESPONSE_NEGOTIATION_START:
	#ifdef ISCHED_DEBUG
	    sprintf(str, "\nResponse received from iRM for the start of negotiation. Value is %d\n", ((negotiation_start_resp_msg_t *)(resp_msg.data))->value);
	    print(log_irm_agent, str);
	#endif
	    if ( ((negotiation_start_resp_msg_t *)(resp_msg.data))->value == 500) {
	#ifdef TESTING
	    sprintf(str, "%s[Negative]\n", rpc_num2string(RESPONSE_NEGOTIATION_START));
	    print(log_irm_agent, str);
	#endif
	       rc = SLURM_ERROR;
	    } else {
	#ifdef TESTING
	    sprintf(str, "%s[Positive]\n", rpc_num2string(RESPONSE_NEGOTIATION_START));
	    print(log_irm_agent, str);
	#endif
	       rc = SLURM_SUCCESS;
	    }
	    slurm_free_negotiation_start_resp_msg(resp_msg.data);
	    break;
       default:
	    print(log_irm_agent, "\nUnexpected message.\n");
       	    free_buf(buffer);
	    slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
    }

    free_buf(buffer);
total_return:
#if defined (ISCHED_DEBUG) 
    print(log_irm_agent, "\nExiting protocol_init\n");
#endif
    return rc;
}


int 
protocol_fini(slurm_fd_t fd) 
{
#if defined (ISCHED_DEBUG) 
    print(log_irm_agent, "\nInside protocol_fini\n");
#endif
    int rc;
    int ch;
    slurm_msg_t req_msg;
    slurm_msg_t resp_msg;
    negotiation_end_msg_t req;

    Buf buffer;
    header_t header;
    char *buf = NULL;
    size_t buflen = 0;

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
#ifdef ISCHED_DEBUG
    _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif
    /*
     * Send message
     */

   /* printf("\nEnter any number\n");
    scanf("%d", &ch);*/

#ifdef TESTING
    val = rand() % 2;
    if(val) {
       val = 4;
    } else {
       val = get_random(4);
    }
    val = 4;
    rc = send_custom_data(fd, val);
#else
    rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
			    get_buf_offset(buffer),
			    SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );
#endif
    if (rc < 0) {
       print(log_irm_agent, "\nProblem with sending the negotiation end message to iRM\n");
       rc = errno;
       free_buf(buffer);
       goto total_return;
    }
#ifdef ISCHED_DEBUG
    print(log_irm_agent, "[iSCHED]: Sent the end msg. Waiting for a response.\n");
#endif
#ifdef TESTING
    print(log_irm_agent, rpc_num2string(NEGOTIATION_END));
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
        print(log_irm_agent, "\n[iSCHED]: Did not receive the correct response to end negotiation.\n");
        print(log_irm_agent, "\n[iSCHED]: Need to exit.\n");
        rc = errno;
        goto total_return;
    }

///#if     _DEBUG
#ifdef ISCHED_DEBUG
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
	#ifdef ISCHED_DEBUG
	    sprintf(str, "\nResponse received from iRM for the end of negotiation. Value is %d\n", ((negotiation_end_resp_msg_t *)(resp_msg.data))->value);
            print(log_irm_agent, str);
	#endif
	    if ( ((negotiation_end_resp_msg_t *)(resp_msg.data))->value == 500) {
	#ifdef TESTING
            sprintf(str, "%s[Negative]\n", rpc_num2string(RESPONSE_NEGOTIATION_END));
            print(log_irm_agent, str);
        #endif
               rc = SLURM_ERROR;
            } else {
	#ifdef TESTING
            sprintf(str, "%s[Positive]\n", rpc_num2string(RESPONSE_NEGOTIATION_END));
            print(log_irm_agent, str);
        #endif
               rc = SLURM_SUCCESS;
            }
	    slurm_free_negotiation_end_resp_msg(resp_msg.data);
            break;
       default:
            print(log_irm_agent, "\nUnexpected message.\n");
       	    free_buf(buffer);
            slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
    }
#ifdef ISCHED_DEBUG
    print(log_irm_agent, "[iSCHED]: Received the response for the end msg.\n");
#endif

    free_buf(buffer);
total_return:
#ifdef ISCHED_DEBUG
    print(log_irm_agent, "\nExiting protocol_fini\n");
#endif
    return rc;
}


int
schedule_urgent_jobs(void) 
{
#if defined (ISCHED_DEBUG) 
   print(log_ug_agent, "\nInside schedule_urgent_jobs\n");
#endif
   int sleep_interval = 20;
   pthread_t thread_id;
   pthread_attr_t thread_attr;

   /*if (pthread_attr_setdetachstate
            (&thread_attr, PTHREAD_CREATE_DETACHED))
                fatal("pthread_attr_setdetachstate %m");
   */
   while(!stop_ug_agent) {
#if defined (ISCHED_DEBUG) 
      print(log_ug_agent, "\nInside the threading loop\n");
      sprintf(str, "\nstop_ug_agent = %d\n", stop_ug_agent);	
      print(log_ug_agent, str);
#endif
      slurm_attr_init(&thread_attr);
      if (pthread_create(&thread_id, &thread_attr, ping_agent, NULL)) {
           print(log_ug_agent, "pthread_create error %m");
      } 
      pthread_join(thread_id, NULL);
      slurm_attr_destroy(&thread_attr);
      _my_sleep(sleep_interval);
   }   

#if defined (ISCHED_DEBUG) 
   print(log_ug_agent, "\nExiting schedule_urgent_jobs\n");
#endif
   return SLURM_SUCCESS;
}


void 
*schedule_loop(void *args) 
{
   print(log_ug_agent, "\nInside the schedule_loop routine of the agent for urgent jobs\n");
   int ret_val = SLURM_SUCCESS;
   ret_val = schedule_urgent_jobs(); 
   print(log_ug_agent, "\nExiting the routine schedule_loop of the agent for urgent jobs\n");
   return NULL;
}



int
send_recv_urgent_job(slurm_fd_t fd, slurm_msg_t *resp_msg)
{
#if defined (ISCHED_DEBUG) 
        print(log_ug_agent, "\nInside send_recv_urgent_job\n");
#endif
        int rc;
	int ret_val = SLURM_SUCCESS;
        slurm_msg_t req_msg;
	//slurm_msg_t resp_msg;
	urgent_job_msg_t msg;
        Buf buffer;
        header_t header;
	char *buf = NULL;
	size_t buflen = 0;

        msg.value = 1;   // For the time being the request resource offer is just a value of 1 being sent in the message.

	//sleep(5);

        slurm_msg_t_init(&req_msg);
        slurm_msg_t_init(resp_msg);

        forward_init(&req_msg.forward, NULL);
        req_msg.ret_list = NULL;
        req_msg.forward_struct = NULL;


        req_msg.msg_type = URGENT_JOB;
        req_msg.data     = &msg;

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

//#if    / _DEBUG
#ifdef ISCHED_DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif
        /*
         * Send message
         */

#ifdef TESTING
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
           print(log_ug_agent, "\nProblem with sending the urgent job to iRM\n");
           rc = errno;
           free_buf(buffer);
           goto total_return;
        }
#ifdef ISCHED_DEBUG
        print(log_ug_agent, "[PING_AGENT]: Sent the urgent job. Waiting for a response.\n");
#endif
#ifdef TESTING
	print(log_ug_agent, rpc_num2string(URGENT_JOB));
#endif
    	free_buf(buffer);

    	resp_msg->conn_fd = fd;

    	/*
     	 * Receive a msg. slurm_msg_recvfrom() will read the message
     	 *  length and allocate space on the heap for a buffer containing
     	 *  the message.
     	 */
    	if (_slurm_msg_recvfrom_timeout(fd, &buf, &buflen, 0, timeout) < 0) {
           forward_init(&header.forward, NULL);
           print(log_ug_agent, "\n[PING_AGENT]: Did not receive the correct response for the urgent job.\n");
           print(log_ug_agent, "\n[PING_AGENT]: Need to exit.\n");
           rc = errno;
           goto total_return;
    	}

//#if    / _DEBUG
#ifdef ISCHED_DEBUG
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
    	resp_msg->protocol_version = header.version;
    	resp_msg->msg_type = header.msg_type;
    	resp_msg->flags = header.flags;

    	if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(resp_msg, buffer) != SLURM_SUCCESS)) {
           rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
           free_buf(buffer);
           goto total_return;
    	}

/*      if (rc == SLURM_SOCKET_ERROR)
            return SLURM_ERROR; */

    	switch (resp_msg->msg_type) {
        /*case RESPONSE_SLURM_RC:
                rc = ((return_code_msg_t *) resp_msg.data)->return_code;
if (rc)
                slurm_seterrno_ret(rc);
            *resp = NULL;
            break;*/
           case RESPONSE_URGENT_JOB:
#ifdef ISCHED_DEBUG
	      sprintf(str, "\nResponse received from iRM for the urgent job. Value is %d\n", ((urgent_job_resp_msg_t *)(resp_msg->data))->value);
              print(log_ug_agent, str);
#endif
	#ifdef TESTING
	      print(log_ug_agent, rpc_num2string(RESPONSE_URGENT_JOB));
	#endif
              slurm_free_urgent_job_resp_msg(resp_msg->data);
              rc = SLURM_SUCCESS;
              break;
           default:
              print(log_ug_agent, "\nUnexpected message.\n");
              free_buf(buffer);
              slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
    	}
#ifdef ISCHED_DEBUG
    	print(log_ug_agent, "[PING_AGENT]: Received the response for the urgent job msg.\n");
#endif

    	free_buf(buffer);
total_return:

#if defined (ISCHED_DEBUG) 
        print(log_ug_agent, "\nExiting send_recv_urgent_job\n");
#endif
        return rc;
}



int
request_resource_offer (slurm_fd_t fd)
{
#if defined (ISCHED_DEBUG)
        print(log_irm_agent, "\nInside request_resource_offer\n");
#endif
        int rc;
        int ch;
	int ret_val = SLURM_SUCCESS;
        slurm_msg_t req_msg;
        request_resource_offer_msg_t req;
        Buf buffer;
        header_t header;

        req.value = 1;   // For the time being the request resource offer is just a value of 1 being sent in the message.

        //printf("\nJust enter a number to start preparing to send a request for resource offer\n");
        //scanf("%d", &ch);

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

//#if    / _DEBUG
#ifdef ISCHED_DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif
        /*
         * Send message
         */

#ifdef TESTING
	val = rand() % 2;
	if(!val) {
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
           print(log_irm_agent, "\nProblem with sending the request resource offer to iRM\n");
           rc = errno;
        } else {
	#ifdef ISCHED_DEBUG
           print(log_irm_agent, "[IRM_AGENT]: Sent the request for a resource offer.\n");
	#endif
	#ifdef TESTING
	   print(log_irm_agent, rpc_num2string(REQUEST_RESOURCE_OFFER));
           rc = SLURM_SUCCESS;
        }

        free_buf(buffer);
#if defined (ISCHED_DEBUG) 
print(log_irm_agent, "\nExiting request_resource_offer\n");
#endif
        return rc;
}



/*
 * receive_resource_offer - Receive a resource offer from iRM
 */
//Similar to slurm_receive_msg
int
receive_resource_offer (slurm_fd_t fd, slurm_msg_t *msg) 
{
#if defined (ISCHED_DEBUG)
        print(log_irm_agent, "\nInside isched_recv_rsrc_offer\n");
#endif
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
                print(log_irm_agent, "\nError in receiving\n");
                forward_init(&header.forward, NULL);
                rc = errno;
                goto total_return;
        }

///#if     _DEBUG
#ifdef ISCHED_DEBUG
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
        msg->protocol_version = header.version;
        msg->msg_type = header.msg_type;
        msg->flags = header.flags;

        switch(msg->msg_type) {
           case RESOURCE_OFFER:  // Do not free msg->data as we need the complete resource offer msg back in the caller for further processing
	     #ifdef ISCHED_DEBUG
                print(log_irm_agent, "\nReceived a resource offer from iRM daemon\n");
	     #endif
                if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(msg, buffer) != SLURM_SUCCESS)) {
                     printf("\nError in buffer size and unpacking of buffer into the msg structure\n");
                     rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
                } else {
                   rc = SLURM_SUCCESS;
                }
                break;
           default:
		print(log_irm_agent, "\nUnexpected Message\n");
       	        free_buf(buffer);
                slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
        }

        free_buf(buffer);
        if (rc == SLURM_SUCCESS) {
	   sprintf(str, "\n[IRM_AGENT]: Received a resource offer from iRM daemon which is %d\n", ( (resource_offer_msg_t *) (msg->data))->value);
           print(log_irm_agent, str);
        }
total_return:
        destroy_forward(&header.forward);

        slurm_seterrno(rc);
        if (rc != SLURM_SUCCESS) {
        } else {
                rc = 0;
        }
#if defined (ISCHED_DEBUG) 
        print(log_irm_agent, "\nExiting isched_recv_rsrc_offer\n");
#endif
        return rc;
}

int
process_resource_offer (resource_offer_msg_t *msg, uint16_t *buf_val, int *attempts)
{
        int input = -1;
        int choice = 0;

	if (*attempts == MAX_NEGOTIATION_ATTEMPTS) {
	   print(log_irm_agent, "\nThis is the final negotiation attempt hence we cannot reject this resource offer. Accepting the offer and sending back a mapping to iRM\n");
	   *attempts = 0;
	   *buf_val = 1;
	   return 0;
	}
	if (msg->error_code == ESLURM_MAPPING_FROM_JOBS_TO_OFFER_REJECT) {
	   print(log_irm_agent, "\niRM has rejected the previous mapping and sent us a new resource offer for a fresh mapping.\n");
        } else {
	   if (!msg->negotiation) {
	      *attempts = 1;
	#ifdef TESTING
	      choice = rand() % 2;
	#else      
              print(log_irm_agent, "\nIs the job queue empty?? if yes, then we send a negative response for the resource offer. Enter 1/0 for empty/non-empty job queue\n");
              scanf("%d", &choice);
	#endif
	   } 
	}
 
        if (!choice) {
	#ifdef TESTING
	   input = rand() % 2;
	#else
           print(log_irm_agent, "\nEnter your choice 1/0 on whether to accept/reject the resource offer and 2 to end the negotiation\n");
           scanf("%d", &input);
	#endif
           if (input == 1) {
              print(log_irm_agent, "\nSuccessfully mapped jobs to this offer and sending the list of jobs to be launched\n");
              *buf_val = 1;
           } else if (input == 0){
              print(log_irm_agent, "\nOffer not accepted. Sending back a negative response\n");
              *buf_val = 0;
           } else if (input == 2) {
	      print(log_irm_agent, "\nGoing to end this negotiation\n");
              *buf_val = 400;
           } else {
	      print(log_irm_agent, "\nWrong choice. Sending back a positive response to the resource offer\n");
              *buf_val = 1;
	   }
        } else {
           *buf_val = 500; 
        }

        return 0;
}

int send_resource_offer_resp(slurm_msg_t *msg, char *buf) 
{
        header_t header;
        int rc;
        resource_offer_resp_msg_t offer_resp_msg;
        slurm_msg_t resp_msg;
        Buf buffer;
        char err_msg[256] = "Job queue is empty";
#if defined (ISCHED_DEBUG) 
        print(log_irm_agent, "\nInside isched_send_irm_msg\n");
#endif
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

/* Be careful when using slurm_strerror to initialize the error msg data member of messages. This function returns a pointer into a statically
   allocated string array holding the string representations of these errors. Do not attempt slurm_xfree of the msg via slurm_free_.... call
   without first setting the error_msg pointer to NULL. This will result in trying to free a statically allocated memory resulting in
   segmentation fault */
  
        if (*(uint16_t *)(buf) == 500) {
           offer_resp_msg.error_code = ESLURM_INVASIVE_JOB_QUEUE_EMPTY;
           offer_resp_msg.error_msg = slurm_strerror(ESLURM_INVASIVE_JOB_QUEUE_EMPTY);
        } else if (*(uint16_t *)(buf) == 0) {
	   offer_resp_msg.error_code = ESLURM_RESOURCE_OFFER_REJECT;
	   offer_resp_msg.error_msg = slurm_strerror(ESLURM_RESOURCE_OFFER_REJECT);
        } else {
	   offer_resp_msg.error_code = SLURM_SUCCESS;
	   offer_resp_msg.error_msg = NULL;
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
#ifdef ISCHED_DEBUG
        _print_data (get_buf_data(buffer),get_buf_offset(buffer));
#endif

   #ifdef TESTING
      val = rand() % 2;
      if(val) {
	 val = 2;
      } else {
         val = get_random(2);
      }
      val = 2;
      memcpy(&tc_offer_resp, &offer_resp_msg, sizeof(resource_offer_resp_msg_t));
      rc = send_custom_data(msg->conn_fd, val);
   #else
        /*
         * Send message
         */
        rc = _slurm_msg_sendto( msg->conn_fd, get_buf_data(buffer),
                                get_buf_offset(buffer),
                                SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );
   #endif
        if (rc < 0) {
           print(log_irm_agent, "\nProblem with sending the resource offer response msg back to iRM Daemon\n");
           rc = SLURM_ERROR;
        } else {
	#ifdef ISCHED_DEBUG
           print(log_irm_agent, "\nSend was successful\n");
	#endif
	#ifdef TESTING
	   sprintf(str, "\n%s\n", rpc_num2string(RESPONSE_RESOURCE_OFFER));
	   print(log_irm_agent, str);
	#endif
           rc = SLURM_SUCCESS;
        }

        free_buf(buffer);
#if defined (ISCHED_DEBUG) 
        print(log_irm_agent, "\nExiting isched_send_irm_msg\n");
#endif
        return rc;
}

int 
receive_feedback(slurm_fd_t fd, slurm_msg_t *msg) 
{
#if defined (ISCHED_DEBUG) 
    print(log_feedback_agent, "\nInside receive_feedback\n");
#endif
    char *buf = NULL;
    size_t buflen = 0;
    header_t header;
    int rc;
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
	    print(log_feedback_agent, "\nError in receiving\n");
	    forward_init(&header.forward, NULL);
	    rc = errno;
	    goto total_return;
    }

    //#if     _DEBUG
#ifdef ISCHED_DEBUG
    _print_data (buf, buflen);
#endif
    buffer = create_buf(buf, buflen);

    if (unpack_header(&header, buffer) == SLURM_ERROR) {
	    print(log_feedback_agent, "\nError in unpacking header\n");
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
       case STATUS_REPORT:  // Do not free msg->data as we need the complete status report msg back in the caller for further processing
	#ifdef ISCHED_DEBUG
	    print(log_feedback_agent, "\nReceived a status report from the feedback agent of iRM daemon\n");
	#endif
	#ifdef TESTING
	    sprintf(str, "\n%s\n", rpc_num2string(STATUS_REPORT));
	    print(log_irm_agent, str);
	#endif
	    if ((header.body_length > remaining_buf(buffer)) || (unpack_msg(msg, buffer) != SLURM_SUCCESS)) {
		 print(log_feedback_agent, "\nError in buffer size and unpacking of buffer into the msg structure\n");
		 rc = ESLURM_PROTOCOL_INCOMPLETE_PACKET;
		 //free_buf(buffer);
	    } else {
	       //memcpy(res_off_msg, msg.data, sizeof(resource_offer_msg_t));
	       rc = SLURM_SUCCESS;
	    }
	    break;
       default:
	    print(log_feedback_agent, "\nUnexpected Message\n");
	    free_buf(buffer);
	    slurm_seterrno_ret(SLURM_UNEXPECTED_MSG_ERROR);
    }

    free_buf(buffer);
    //if (rc != SLURM_SUCCESS) goto total_return;
    if (rc == SLURM_SUCCESS) {
#if defined (ISCHED_DEBUG) 
       sprintf(str, "\n[FEEDBACK_AGENT]: Received a status report from the feedback agent of iRM daemon which is %d\n", ( (status_report_msg_t *) (msg->data))->value);
       print(log_feedback_agent, str);
#endif
    }
total_return:
    destroy_forward(&header.forward);

    slurm_seterrno(rc);
    if (rc != SLURM_SUCCESS) {
    } else {
	    rc = 0;
    }
#if defined (ISCHED_DEBUG) 
    print(log_feedback_agent, "\nExiting receive_feedback\n");
#endif
    return rc;
}

int
process_feedback(status_report_msg_t *msg)
{
#if defined (ISCHED_DEBUG)
    print(log_feedback_agent, "\nEntering process_feedback\n");
    print(log_feedback_agent, "\nExiting process_feedback\n");
#endif
    return SLURM_SUCCESS;
}
