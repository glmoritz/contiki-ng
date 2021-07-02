/*
 * Copyright (c) 2002, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the Contiki OS
 *
 */

/**
 * \ingroup platform
 *
 * \defgroup labscim_platform LabSCim platform
 *
 * Platform running in the host (Windows or Linux) environment to connect to LabSCim.
 *
 * Used mainly for simulations.
 * @{
 */


#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>

#ifdef __CYGWIN__
#include "net/wpcap-drv.h"
#endif /* __CYGWIN__ */

#include "contiki.h"
#include "contiki-conf.h"

#include "labscim_linked_list.h"
#include "labscim_protocol.h"
#include "labscim_socket.h"
#include "labscim_contiking_setup.h"
#include "labscim_sun_radio.h"

#include "net/netstack.h"

#include "dev/serial-line.h"
#include "dev/button-hal.h"
#include "dev/gpio-hal.h"
#include "dev/leds.h"

#include "net/ipv6/uip.h"
#include "net/ipv6/uip-debug.h"
#include "net/queuebuf.h"

#if NETSTACK_CONF_WITH_IPV6
#include "net/ipv6/uip-ds6.h"
#endif /* NETSTACK_CONF_WITH_IPV6 */

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "Native"
#define LOG_LEVEL LOG_LEVEL_MAIN

#define SERVER_PORT (9608)
#define SERVER_ADDRESS "127.0.0.1"
#define SOCK_BUFFER_SIZE (512)
struct labscim_ll gCommands;

buffer_circ_t* gNodeInputBuffer;
buffer_circ_t* gNodeOutputBuffer;

/*---------------------------------------------------------------------------*/
/**
 * \name Labscim Platform Configuration
 *
 * @{
 */

/*
 * Defines the maximum number of file descriptors monitored by the platform
 * main loop.
 */
#ifdef SELECT_CONF_MAX
#define SELECT_MAX SELECT_CONF_MAX
#else
#define SELECT_MAX 8
#endif

/*
 * Defines the timeout (in msec) of the select operation if no monitored file
 * descriptors becomes ready.
 */
#ifdef SELECT_CONF_TIMEOUT
#define SELECT_TIMEOUT SELECT_CONF_TIMEOUT
#else
#define SELECT_TIMEOUT 1000
#endif

/*
 * Adds the STDIN file descriptor to the list of monitored file descriptors.
 */
#ifdef SELECT_CONF_STDIN
#define SELECT_STDIN SELECT_CONF_STDIN
#else
#define SELECT_STDIN 1
#endif
/** @} */
/*---------------------------------------------------------------------------*/

static const struct select_callback *select_callback[SELECT_MAX];
static int select_max = 0;


#ifdef PLATFORM_CONF_MAC_ADDR
static uint8_t mac_addr[] = PLATFORM_CONF_MAC_ADDR;
#else /* PLATFORM_CONF_MAC_ADDR */
uint8_t mac_addr[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
#endif /* PLATFORM_CONF_MAC_ADDR */




extern void labscim_set_time(uint64_t time);
extern void labscim_time_event(struct labscim_time_event* msg);

uint32_t gBootReceived=0;
uint8_t gIsCoordinator=0;
uint32_t gProcessing=0;

void labscim_protocol_boot(struct labscim_protocol_boot* msg)
{
	struct contiki_node_setup* cns = (struct contiki_node_setup*)msg->message;
	memcpy((void*)mac_addr,(void*)cns->mac_addr,sizeof(linkaddr_t));
	labscim_set_time(cns->startup_time);
	gBootReceived = 1;
	gIsCoordinator = cns->tsch_coordinator;
	free(msg);
	return;
}


/*---------------------------------------------------------------------------*/
int
select_set_callback(int fd, const struct select_callback *callback)
{
	int i;
	if(fd >= 0 && fd < SELECT_MAX) {
		/* Check that the callback functions are set */
		if(callback != NULL &&
				(callback->set_fd == NULL || callback->handle_fd == NULL)) {
			callback = NULL;
		}

		select_callback[fd] = callback;

		/* Update fd max */
		if(callback != NULL) {
			if(fd > select_max) {
				select_max = fd;
			}
		} else {
			select_max = 0;
			for(i = SELECT_MAX - 1; i > 0; i--) {
				if(select_callback[i] != NULL) {
					select_max = i;
					break;
				}
			}
		}
		return 1;
	}
	return 0;
}
/*---------------------------------------------------------------------------*/
#if SELECT_STDIN
static int
stdin_set_fd(fd_set *rset, fd_set *wset)
{
	FD_SET(STDIN_FILENO, rset);
	return 1;
}

static int (*input_handler)(unsigned char c);

void native_uart_set_input(int (*input)(unsigned char c)) {
	input_handler = input;
}


static void
stdin_handle_fd(fd_set *rset, fd_set *wset)
{
	char c;
	if(FD_ISSET(STDIN_FILENO, rset)) {
		if(read(STDIN_FILENO, &c, 1) > 0) {
			input_handler(c);
		}
	}
}
const static struct select_callback stdin_fd = {
		stdin_set_fd, stdin_handle_fd
};
#endif /* SELECT_STDIN */
/*---------------------------------------------------------------------------*/

/*------------------*/

#ifdef LABSCIM_REMOTE_SOCKET //socket communication

static int
socket_set_fd(fd_set *rset, fd_set *wset)
{
	FD_SET(gNodeInputBuffer->socket, rset);
	return 1;
}

static void
socket_handle_fd(fd_set *rset, fd_set *wset)
{
	if(FD_ISSET(gNodeInputBuffer->socket, rset))
	{
		labscim_socket_handle_input(gNodeInputBuffer, &gCommands);
	}
}

#endif


void socket_process_command(struct labscim_protocol_header* hdr)
{
#ifdef LABSCIM_LOG_COMMANDS
	char log[128];
#endif
	switch(hdr->labscim_protocol_code)
	{
	case LABSCIM_PROTOCOL_BOOT:
	{
#ifdef LABSCIM_LOG_COMMANDS
		sprintf(log,"seq%4d\tPROTOCOL_BOOT\n",hdr->sequence_number);
		labscim_log(log, "pro ");
#endif
		labscim_protocol_boot((struct labscim_protocol_boot*)(hdr));
		break;
	}
	case LABSCIM_TIME_EVENT:
	{
#ifdef LABSCIM_LOG_COMMANDS
		sprintf(log,"seq%4d\tTIME_EVENT\n",hdr->sequence_number);
		labscim_log(log, "pro ");
#endif
		labscim_time_event((struct labscim_time_event*)(hdr));
		break;
	}
	case LABSCIM_RADIO_RESPONSE:
	{
#ifdef LABSCIM_LOG_COMMANDS
		sprintf(log,"seq%4d\tRADIO_RESPONSE\n",hdr->sequence_number);
		labscim_log(log, "pro ");
#endif
		labscim_set_time(((struct labscim_radio_response*)(hdr))->current_time);
		labscim_radio_incoming_command((struct labscim_radio_response*)(hdr));
		break;
	}
	default:
	{
		perror("Unhandled Labscim Command\n");
		free(hdr);
	}
	}
}

void socket_process_all_commands()
{
	void* cmd;
	//process returned commands (if any)
	do
	{
		cmd = labscim_ll_pop_front(&gCommands);
		if(cmd!=NULL)
		{
			struct labscim_protocol_header* hdr = (struct labscim_protocol_header*)cmd;
			gProcessing = 1;
			socket_process_command(hdr);
		}
	}while(cmd!=NULL);
}

void* socket_pop_command(uint32_t command, uint32_t sequence_number)
{
	void* cmd;
	struct labscim_ll_node* it = gCommands.head;
	do
	{
		if(it!=NULL)
		{
			struct labscim_protocol_header* hdr = (struct labscim_protocol_header*)it->data;
			if(((hdr->labscim_protocol_code==command)&&(sequence_number == 0))||(hdr->request_sequence_number==sequence_number)||((command==0)&&(sequence_number==0)))
			{
				cmd = labscim_ll_pop_node(&gCommands,it);
				return cmd;
			}
			it = it->next;
		}

	}while(it!=NULL);
	return NULL;
}

void* socket_wait_for_command(uint32_t command, uint32_t sequence_number)
{
	void* cmd = NULL;
	cmd = socket_pop_command(command, sequence_number);


	while(cmd==NULL)
	{

#ifdef LABSCIM_REMOTE_SOCKET //socket communication
		fd_set fdr;
		fd_set fdw;
		int retval;
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = SELECT_TIMEOUT;
		FD_ZERO(&fdr);
		FD_ZERO(&fdw);
		socket_set_fd(&fdr, &fdw);

		retval = select(gNodeInputBuffer->socket+1, &fdr, &fdw, NULL, NULL);
		if(retval < 0) {
			if(errno != EINTR) {
				perror("select");
			}
		} else if(retval > 0) {
			/* timeout => retval == 0 */
			socket_handle_fd(&fdr, &fdw);
			cmd = socket_pop_command(command,sequence_number);
		}
#else
		//shared memory communication
		pthread_mutex_lock(gNodeInputBuffer->mutex.mutex);
		labscim_socket_handle_input(gNodeInputBuffer, &gCommands);

		while(gCommands.count == 0)
		{
			pthread_cond_wait(gNodeInputBuffer->mutex.more, gNodeInputBuffer->mutex.mutex);
			labscim_socket_handle_input(gNodeInputBuffer, &gCommands);
		}
		cmd = socket_pop_command(command,sequence_number);
		pthread_cond_signal(gNodeInputBuffer->mutex.less);
		pthread_mutex_unlock(gNodeInputBuffer->mutex.mutex);
#endif
	}
	return cmd;
}

#ifdef LABSCIM_REMOTE_SOCKET //socket communication

const static struct select_callback socket_fd = {
		socket_set_fd, socket_handle_fd
};

#endif



/*------------------*/



static void
set_lladdr(void)
{
	linkaddr_t addr;

	memset(&addr, 0, sizeof(linkaddr_t));
#if NETSTACK_CONF_WITH_IPV6
	memcpy(addr.u8, mac_addr, sizeof(addr.u8));
#else
	int i;
	for(i = 0; i < sizeof(linkaddr_t); ++i) {
		addr.u8[i] = mac_addr[7 - i];
	}
#endif
	linkaddr_set_node_addr(&addr);
}
/*---------------------------------------------------------------------------*/
#if NETSTACK_CONF_WITH_IPV6
static void
set_global_address(void)
{
	uip_ipaddr_t ipaddr;
	const uip_ipaddr_t *default_prefix = uip_ds6_default_prefix();

	/* Assign a unique local address (RFC4193,
     http://tools.ietf.org/html/rfc4193). */
	uip_ip6addr_copy(&ipaddr, default_prefix);

	/* Assumes that the uip_lladdr is set */
	uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
	uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

	LOG_INFO("Added global IPv6 address ");
	LOG_INFO_6ADDR(&ipaddr);
	LOG_INFO_("\n");

	/* set the PREFIX::1 address to the IF */
	uip_ip6addr_copy(&ipaddr, default_prefix);
	ipaddr.u8[15] = 1;
	uip_ds6_defrt_add(&ipaddr, 0);
}
#endif
/*---------------------------------------------------------------------------*/
int contiki_argc = 0;
char **contiki_argv;

uint8_t* gNodeName;
uint8_t* gServerAddress;
uint64_t gServerPort;
uint64_t gBufferSize;

/*---------------------------------------------------------------------------*/
void
platform_process_args(int argc, char**argv)
{
	/* crappy way of remembering and accessing argc/v */
	contiki_argc = argc;
	contiki_argv = argv;
	uint64_t c;

	/* native under windows is hardcoded to use the first one or two args */
	/* for wpcap configuration so this needs to be "removed" from         */
	/* contiki_args (used by the native-border-router) */
#ifdef __CYGWIN__
	contiki_argc--;
	contiki_argv++;
#ifdef UIP_FALLBACK_INTERFACE
	contiki_argc--;
	contiki_argv++;
#endif
#endif


	while ((c = getopt (argc, argv, "a:p:b:n:")) != -1)
		switch (c)
		{
		case 'a':
			gServerAddress = optarg;
			break;
		case 'b':
			if(optarg!=NULL)
			{
				gBufferSize = atoi(optarg);
			}
			else
			{
				gBufferSize = SOCK_BUFFER_SIZE;
			}
			break;
		case 'p':
			if(optarg!=NULL)
			{
				gServerPort = atoi(optarg);
			}
			else
			{
				gServerPort = SERVER_PORT;
			}
			break;
		case 'n':
			gNodeName = optarg;
			break;
		case '?':
			fprintf (stderr,"Unknown option character `\\x%x'.\n",	optopt);
		}
}
/*---------------------------------------------------------------------------*/
void
platform_init_stage_one()
{
	gpio_hal_init();
	button_hal_init();
	leds_init();
	return;
}
/*---------------------------------------------------------------------------*/
void
platform_init_stage_two()
{
	uint8_t *inbuffername,*outbuffername;
	inbuffername = (uint8_t*)malloc(sizeof(uint8_t)*strlen(gNodeName)+4);
	if(inbuffername==NULL)
	{
		perror("\nMalloc\n");
		return;
	}
	memcpy(inbuffername+1,gNodeName,strlen(gNodeName));
	memcpy(inbuffername+strlen(gNodeName)+1,"in",2);
	inbuffername[0] = '/';
	inbuffername[strlen(gNodeName)+3] = 0;

	outbuffername = (uint8_t*)malloc(sizeof(uint8_t)*strlen(gNodeName)+5);
	if(outbuffername==NULL)
	{
		perror("\nMalloc\n");
		return;
	}
	memcpy(outbuffername+1,gNodeName,strlen(gNodeName));
	memcpy(outbuffername+strlen(gNodeName)+1,"out",3);
	outbuffername[0] = '/';
	outbuffername[strlen(gNodeName)+4] = 0;

	gNodeOutputBuffer = (buffer_circ_t*) malloc( sizeof(buffer_circ_t) );
	if(gNodeOutputBuffer==NULL)
	{
		perror("\nMalloc\n");
		return;
	}
	labscim_buffer_init(gNodeOutputBuffer, outbuffername, gBufferSize, 0);
	labscim_ll_init_list(&gCommands);
#ifdef LABSCIM_REMOTE_SOCKET //socket communication
	gNodeInputBuffer = gNodeOutputBuffer;
	labscim_socket_connect(gServerAddress,gServerPort, gNodeInputBuffer);
#else
	gNodeInputBuffer = (buffer_circ_t*) malloc( sizeof(buffer_circ_t) );
	if(gNodeInputBuffer==NULL)
	{
		perror("\nMalloc\n");
		return;
	}
	labscim_buffer_init(gNodeInputBuffer, inbuffername, gBufferSize, 1);
	node_is_ready(gNodeOutputBuffer);
#endif
	while(!gBootReceived)
	{

#ifdef LABSCIM_REMOTE_SOCKET //socket communication
		fd_set fdr;
		fd_set fdw;
		int retval;
		struct timeval tv;

		tv.tv_sec = 0;
		tv.tv_usec = SELECT_TIMEOUT;

		FD_ZERO(&fdr);
		FD_ZERO(&fdw);

		socket_set_fd(&fdr, &fdw);

		retval = select(gNodeInputBuffer->socket+1, &fdr, &fdw, NULL, NULL);
		if(retval < 0) {
			if(errno != EINTR) {
				perror("select");
			}
		} else if(retval > 0) {
			/* timeout => retval == 0 */
			socket_handle_fd(&fdr, &fdw);
			socket_process_all_commands();
		}
#else
	//shared memory communication
	pthread_mutex_lock(gNodeInputBuffer->mutex.mutex);

	labscim_socket_handle_input(gNodeInputBuffer, &gCommands);

	while(gCommands.count == 0)
	{
		pthread_cond_wait(gNodeInputBuffer->mutex.more, gNodeInputBuffer->mutex.mutex);
		labscim_socket_handle_input(gNodeInputBuffer, &gCommands);
	}
	pthread_cond_signal(gNodeInputBuffer->mutex.less);
	pthread_mutex_unlock(gNodeInputBuffer->mutex.mutex);
	socket_process_all_commands();
#endif
	}

	set_lladdr();
	serial_line_init();

	if (NULL == input_handler) {
		native_uart_set_input(serial_line_input_byte);
	}
}
/*---------------------------------------------------------------------------*/

void
platform_init_stage_three()
{
#if NETSTACK_CONF_WITH_IPV6
#ifdef __CYGWIN__
	process_start(&wpcap_process, NULL);
#endif

	set_global_address();

#endif /* NETSTACK_CONF_WITH_IPV6 */

	/* Make standard output unbuffered. */
	setvbuf(stdout, (char *)NULL, _IONBF, 0);
}

/*---------------------------------------------------------------------------*/
void
platform_main_loop()
{

	struct labscim_ll ScheduledEtimers;
	labscim_ll_init_list(&ScheduledEtimers);
#if SELECT_STDIN
	select_set_callback(STDIN_FILENO, &stdin_fd);

#ifdef LABSCIM_REMOTE_SOCKET //shared memory communication
	select_set_callback(gNodeOutputBuffer->socket, &socket_fd);
#endif

#else
	select_set_callback(0, &socket_fd);
#endif /* SELECT_STDIN */
	while(1) {
#ifdef LABSCIM_REMOTE_SOCKET //shared memory communication
		fd_set fdr;
		fd_set fdw;
		int maxfd;
		int i;
		struct timeval tv;
#endif
		int retval;

		retval = process_run();
		if(retval==0)
		{
			if(gProcessing)
			{
				uint16_t ctrue=1;
				clock_time_t c = etimer_next_expiration_time();
				if(c <= clock_time())
				{
					//timer was delayed by interrupt - schedule an event to now
					c = clock_time() + 1;
				}

				clock_time_t now = clock_time();
				uint32_t found=0;
				if(c>0)
				{
					//find if this etimer is not already scheduled
					struct labscim_ll_node* iter = ScheduledEtimers.head;
					while(iter!=NULL)
					{
						if(*((clock_time_t*)(iter->data)) > c)
						{
							iter = iter->previous;
							break;
						}
						if(*((clock_time_t*)(iter->data)) == c)
						{
							found = 1;
							break;
						}
						iter = iter->next;
					}

					//if not scheduled then schedule
					if(!found)
					{
						clock_time_t* new_timer = (clock_time_t*)malloc(sizeof(clock_time_t));
						if(new_timer == NULL)
						{
							perror("Malloc\n");
							return;
						}


						*new_timer = c;
						labscim_ll_insert_before(&ScheduledEtimers,iter,new_timer);
						set_time_event(gNodeOutputBuffer, CONTIKI_ETIMER_TIME_EVENT, ctrue, c - clock_time());
					}

					//clear all past events
					clock_time_t* first = (clock_time_t*)labscim_ll_peek_front(&ScheduledEtimers);
					while(first!=NULL)
					{
						if(*first < now)
						{
							free(labscim_ll_pop_front(&ScheduledEtimers));
						}
						else
						{
							break;
						}
						first = (clock_time_t*)labscim_ll_peek_front(&ScheduledEtimers);
					}
				}
				protocol_yield(gNodeOutputBuffer);
				gProcessing = 0;
			}
		}

#ifdef LABSCIM_REMOTE_SOCKET //socket communication
		tv.tv_sec = 0;
		tv.tv_usec = retval ? 1 : SELECT_TIMEOUT;
		FD_ZERO(&fdr);
		FD_ZERO(&fdw);
		maxfd = 0;
		for(i = 0; i <= select_max; i++) {
			if(select_callback[i] != NULL && select_callback[i]->set_fd(&fdr, &fdw)) {
				maxfd = i;
			}
		}

		retval = select(maxfd + 1, &fdr, &fdw, NULL, NULL);
		if(retval < 0) {
			if(errno != EINTR) {
				perror("select");
			}
		} else if(retval > 0) {
			/* timeout => retval == 0 */
			for(i = 0; i <= maxfd; i++) {
				if(select_callback[i] != NULL) {
					select_callback[i]->handle_fd(&fdr, &fdw);
				}
			}
			socket_process_all_commands();
		}
		//etimer_request_poll();
#else
		//shared memory communication
		pthread_mutex_lock(gNodeInputBuffer->mutex.mutex);

		labscim_socket_handle_input(gNodeInputBuffer, &gCommands);

		while( (gCommands.count == 0) && ( retval==0 ) )
		{
			pthread_cond_wait(gNodeInputBuffer->mutex.more, gNodeInputBuffer->mutex.mutex);
			labscim_socket_handle_input(gNodeInputBuffer, &gCommands);
		}
		pthread_cond_signal(gNodeInputBuffer->mutex.less);
		pthread_mutex_unlock(gNodeInputBuffer->mutex.mutex);
		socket_process_all_commands();
#endif
	}
	return;
}

/*---------------------------------------------------------------------------*/
void
log_message(char *m1, char *m2)
{
	fprintf(stderr, "%s%s\n", m1, m2);
}
/*---------------------------------------------------------------------------*/
void
uip_log(char *m)
{
	fprintf(stderr, "%s\n", m);
}
/*---------------------------------------------------------------------------*/
/** @} */

