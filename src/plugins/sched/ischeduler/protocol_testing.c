/*****************************************************************************\
 *  protocol_testing.c - Code for doing dummy testing for msgs sent towards iRM 
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
#include "src/common/forward.h"

#define timeout 30*1000
#define RANDOM 2345

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
send_custom_data(slurm_fd_t fd)
{
    printf("\nInside send_custom_data\n");
    int rc;
    char ch;
    int choice = -1;
    slurm_msg_t msg;
    
    request_resource_offer_msg_t msg1;
    resource_offer_resp_msg_t msg2;
    negotiation_start_msg_t msg3;
    negotiation_end_msg_t msg4;
    urgent_job_msg_t msg5;
    return_code_msg_t msg6;

    Buf buffer;
    header_t header;
    char *buf = NULL;
    size_t buflen = 0;

    slurm_msg_t_init(&msg);

    forward_init(&msg.forward, NULL);
    msg.ret_list = NULL;
    msg.forward_struct = NULL;

    printf("\nMenu for all the possible message types you can send\n");
    printf("1. REQUEST_RESOURCE_OFFER\n");
    printf("2. RESPONSE_RESOURCE_OFFER\n");
    printf("3. NEGOTIATION_START\n");
    printf("4. NEGOTIATION_END\n");
    printf("5. URGENT_JOB\n");
    printf("6. RANDOM MSG\n"); 
    printf("\nEnter your choice of the message from the below options\n");
    scanf("%d", &choice);

    switch(choice) {
	case 1:
	     msg.msg_type = REQUEST_RESOURCE_OFFER;
	     msg1.value = 1;
	     msg.data = &msg1;
	     break;
	case 2:
	     msg.msg_type = RESPONSE_RESOURCE_OFFER;
	     msg2.value = 1;
	     msg2.error_code = 0;
	     msg2.error_msg = (char *) NULL;
	     msg.data = &msg2;
	     break;
	case 3:
	     msg.msg_type = NEGOTIATION_START;
	     msg3.value = 1;
	     msg.data = &msg3;
	     break;
	case 4:
	     msg.msg_type = NEGOTIATION_END;
	     msg4.value = 1;
	     msg.data = &msg4;
	     break;
	case 5:
	     msg.msg_type = URGENT_JOB;
	     msg5.value = 0;
	     msg.data = &msg5;
	     break;
	case 6:
	default:
	     msg.msg_type = RESPONSE_SLURM_RC;
	     msg6.return_code = 100;
	     msg.data = &msg6;
    }

    init_header(&header, &msg, msg.flags);

    /*
     * Pack header into buffer for transmission
     */
    buffer = init_buf(BUF_SIZE);
    pack_header(&header, buffer);

    /*
     * Pack message into buffer
     */
    new_pack_msg(&msg, &header, buffer);

//#if     _DEBUG
    _print_data (get_buf_data(buffer),get_buf_offset(buffer));
//#endif

    /*
     * Send message
     */

   /* printf("\nPress enter\n");
    scanf("%c", &ch);*/

    rc = _slurm_msg_sendto( fd, get_buf_data(buffer),
                            get_buf_offset(buffer),
                            SLURM_PROTOCOL_NO_SEND_RECV_FLAGS );

    if (rc < 0) {
       printf("\nProblem with sending the message to iRM\n");
       rc = SLURM_ERROR;
       free_buf(buffer);
       goto total_return;
    } else {
       printf("[iSCHED]: Sent the msg.\n");
       rc = SLURM_SUCCESS;
    }

    free_buf(buffer);
total_return: printf("\nExiting send_custom_data\n");
    return rc;
}
