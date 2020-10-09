/**
 * \file
 *       Labscim Contiki Rtimer Implementation.
 * \author
 	 	 Guilherme Luiz Moritz <moritz@utfpr.edu.br>
 */

#ifndef RTIMER_ARCH_H_
#define RTIMER_ARCH_H_

#include "contiki.h"
#include "labscim_protocol.h"
#include "labscim_socket.h"
#include <stdint.h>

#define RTIMER_ARCH_SECOND CLOCK_CONF_SECOND

#define rtimer_arch_now() clock_time()

void labscim_rtimer_interrupt();
uint32_t rtimer_busy_wait_poll_prepare(rtimer_clock_t t0, rtimer_clock_t max_timeout);
bool rtimer_busy_wait_poll_loop(uint32_t seq_number);

extern buffer_circ_t* gNodeOutputBuffer;

#define US_TO_RTIMERTICKS(US)  (US)

#define RTIMERTICKS_TO_US(T)   (T)

#define RTIMERTICKS_TO_US_64(T)  (T)

#define CONTIKI_RTIMER_TIME_EVENT (0x1C00001C)
#define CONTIKI_RTIMER_BUSY_WAIT_EVENT (0xB10000B1)
#define CONTIKI_ETIMER_TIME_EVENT (0x37100371)

/** \brief Busy-wait until a condition. Start time is t0, max wait time is max_time */

#define RTIMER_BUSYWAIT_UNTIL_ABS(cond, t0, max_time)				\
  ({																\
	bool timeout=false;												\
	uint32_t seq_no;												\
	if(!(cond)) 													\
	{																\
		seq_no = rtimer_busy_wait_poll_prepare(t0, max_time);		\
		if(seq_no > 0)												\
		{															\
			do														\
			{														\
				protocol_yield(gNodeOutputBuffer);					\
				timeout = rtimer_busy_wait_poll_loop(seq_no); 		\
			}while(!timeout && (!(cond)));							\
			if(!timeout)											\
			{														\
				cancel_time_event(gNodeOutputBuffer, seq_no);		\
			}														\
		}															\
	}																\
})

/** \brief Busy-wait until a condition for at most max_time */
#define RTIMER_BUSYWAIT_UNTIL(cond, max_time)       \
  ({                                                \
    rtimer_clock_t t0 = RTIMER_NOW();               \
    RTIMER_BUSYWAIT_UNTIL_ABS(cond, t0, max_time);  \
  })

/** \brief Busy-wait for a fixed duration */
#define RTIMER_BUSYWAIT(duration) RTIMER_BUSYWAIT_UNTIL(0, duration)




#endif /* RTIMER_ARCH_H_ */
