/**
 * \file
 *         Labscim code for the Contiki real-time module rtimer
 * \author
 *         Guilherme Luiz Moritz - moritz@utfpr.edu.br
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include "sys/rtimer.h"
#include "sys/clock.h"
#include "labscim_socket.h"
#include "platform.h"


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

extern buffer_circ_t* gNodeInputBuffer;
extern buffer_circ_t* gNodeOutputBuffer;

uint32_t gRunningSchedule=0;

/*---------------------------------------------------------------------------*/
void labscim_rtimer_interrupt()
{
	gRunningSchedule = 0;
	rtimer_run_next();
}
/*---------------------------------------------------------------------------*/
void
rtimer_arch_init(void)
{
	//nothing to do here
}

/*---------------------------------------------------------------------------*/
void
rtimer_arch_schedule(rtimer_clock_t t)
{
  uint16_t cfalse = 0;
  //rtimer_clock_t c, now;
  //now = RTIMER_NOW();

  /*
   * New value must be at least 1 tick in the future.
   */
//  if((int64_t)(t - now) < 1) {
//    t = now + 1;
//  }
  //c = t - RTIMER_NOW();
  if(gRunningSchedule>0)
  {
	  cancel_time_event(gNodeOutputBuffer, gRunningSchedule);
  }
  gRunningSchedule = set_time_event(gNodeOutputBuffer, CONTIKI_RTIMER_TIME_EVENT, cfalse, t);
}
/*---------------------------------------------------------------------------*/

uint32_t rtimer_busy_wait_poll_prepare(rtimer_clock_t t0, rtimer_clock_t max_timeout)
{
	uint16_t cfalse = 0;
	struct labscim_protocol_header* resp;
	uint32_t seq;
	if(t0 + max_timeout > RTIMER_NOW())
	{
		seq = set_time_event(gNodeOutputBuffer, CONTIKI_RTIMER_BUSY_WAIT_EVENT, cfalse, t0 + max_timeout);
		return seq;
	}
	else
	{
		return 0;
	}
}

bool rtimer_busy_wait_poll_loop(uint32_t seq_number)
{
	struct labscim_protocol_header* resp;
	bool timeout = false;
	resp =  (struct labscim_protocol_header*)socket_wait_for_command(0, 0);
	if(resp->request_sequence_number == seq_number)
	{
		timeout = true;
		free(resp);
	}
	else
	{
		socket_process_command(resp);
	}
	return timeout;
}


