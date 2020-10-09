/*
 * Copyright (c) 2005, Swedish Institute of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "dev/watchdog.h"
#include <stdlib.h>
#include "labscim_socket.h"


extern buffer_circ_t* gNodeInputBuffer;
extern buffer_circ_t* gNodeOutputBuffer;
void socket_process_command(struct labscim_protocol_header* hdr);
void* socket_wait_for_command(uint32_t command, uint32_t sequence_number);

uint32_t gWatchdogCalled = 0;



/*---------------------------------------------------------------------------*/
void
watchdog_start(void)
{
}
/*---------------------------------------------------------------------------*/
void
watchdog_periodic(void)
{
	// if someone call us twice without sending a command it is because they are busy waiting for a time that wont happen without a yield
	// (a.k.a TSCH busy wait)
	// (ugly code requires ugly hacks)
	if(!gWatchdogCalled)
	{
		gWatchdogCalled = 1;
	}
	else
	{
		protocol_yield(gNodeOutputBuffer);
		struct labscim_protocol_header* resp;
		resp =  (struct labscim_protocol_header*)socket_wait_for_command(0, 0);
		//TODO: this behavior is wrong. right behavior: if this function is running from interrupt context, all other interrupt events
		//TODO: must be delayed, and non interrupt must wait until the end of interrupt context
		//TODO: but for now we just believe that this wont be happening
		socket_process_command(resp);
	}
}

void watchdog_signal(void)
{
	//a command was sent: reset
	gWatchdogCalled = 0;
}

/*---------------------------------------------------------------------------*/
void
watchdog_stop(void)
{
}
/*---------------------------------------------------------------------------*/
void
watchdog_reboot(void)
{
	// Death by watchdog.
	exit(-1);
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
void
watchdog_init(void)
{
	labscim_set_send_command_callback(watchdog_signal);
}

