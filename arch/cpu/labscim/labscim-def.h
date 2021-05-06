/*Labscim Definitions - author: Guilherme Luiz Moritz - <moritz@utfpr.edu.br>
 *  This work is based on native-def.h and cc13xx-cc26xx-def.h from Contiki-NG
 *  Original Disclamer is reproduced below:
 *   *
 * */

/*
 * Copyright (c) 2018, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
#ifndef LABSCIM_DEF_H_
#define LABSCIM_DEF_H_
/*---------------------------------------------------------------------------*/
#define GPIO_HAL_CONF_ARCH_SW_TOGGLE     1
#define GPIO_HAL_CONF_PORT_PIN_NUMBERING 0
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* TSCH related defines */

/* 2 bytes header, 4 bytes CRC */
#define LABSCIM_SUN_RADIO_PHY_OVERHEAD 6ull
/* 3 bytes preamble, 3 bytes sync */
#define LABSCIM_SUN_RADIO_PHY_HEADER_LEN 3ull
/* The default data rate is 50 kbps */
#define LABSCIM_SUN_RADIO_BIT_RATE 50000ull

///* 1 len byte, 2 bytes CRC */
//#define CC26XX_RADIO_PHY_OVERHEAD     3
///* 4 bytes preamble, 1 byte sync */
//#define CC26XX_RADIO_PHY_HEADER_LEN   5
///* The fixed data rate is 250 kbps */
//#define CC26XX_RADIO_BIT_RATE  250000

#if LABSCIM_RADIO_SUN
#define RADIO_PHY_HEADER_LEN LABSCIM_SUN_RADIO_PHY_HEADER_LEN
#define RADIO_PHY_OVERHEAD   LABSCIM_SUN_RADIO_PHY_OVERHEAD
#define RADIO_BIT_RATE       LABSCIM_SUN_RADIO_BIT_RATE

/* The TSCH default slot length of 10ms is too short, use custom one instead */
#ifndef TSCH_CONF_DEFAULT_TIMESLOT_TIMING
#define TSCH_CONF_DEFAULT_TIMESLOT_TIMING tsch_timing_labscim_sun_50kbps
#endif /* TSCH_CONF_DEFAULT_TIMESLOT_TIMING */

/* Symbol for the custom TSCH timeslot timing template */
#define TSCH_CONF_ARCH_HDR_PATH "labscim_sun_50kbps_tsch.h"

#else
#error "For now, LABSCIM_RADIO_SUN is the only option for Labscim Radio"
//#define RADIO_PHY_HEADER_LEN CC26XX_RADIO_PHY_HEADER_LEN
//#define RADIO_PHY_OVERHEAD   CC26XX_RADIO_PHY_OVERHEAD
//#define RADIO_BIT_RATE       CC26XX_RADIO_BIT_RATE
#endif

#define RADIO_BYTE_AIR_TIME  (1000000ull / (RADIO_BIT_RATE / 8ull))

/* Delay between GO signal and SFD */
#define RADIO_DELAY_BEFORE_TX ((unsigned)US_TO_RTIMERTICKS(RADIO_PHY_HEADER_LEN * RADIO_BYTE_AIR_TIME))
/* Delay between GO signal and start listening.
 * This value is so small because the radio is constantly on within each timeslot. */
#define RADIO_DELAY_BEFORE_RX ((unsigned)US_TO_RTIMERTICKS(15))
/* Delay between the SFD finishes arriving and it is detected in software. */
#define RADIO_DELAY_BEFORE_DETECT ((unsigned)US_TO_RTIMERTICKS(352))

/* Timer conversion; radio is running at 4 MHz */
#define RADIO_TIMER_SECOND   4000000u
#if (RTIMER_SECOND % 256) || (RADIO_TIMER_SECOND % 256)
#error RADIO_TO_RTIMER macro must be fixed!
#endif
#define RADIO_TO_RTIMER(X)   (X)
#define USEC_TO_RADIO(X)     (X)

/* Do not turn off TSCH within a timeslot: not enough time */
#define TSCH_CONF_RADIO_ON_DURING_TIMESLOT 1

/* Disable TSCH frame filtering */
#define TSCH_CONF_HW_FRAME_FILTERING  0

/* Use hardware timestamps */
#ifndef TSCH_CONF_RESYNC_WITH_SFD_TIMESTAMPS
#define TSCH_CONF_RESYNC_WITH_SFD_TIMESTAMPS 1
#define TSCH_CONF_TIMESYNC_REMOVE_JITTER 0
#endif

/* 10 times per second */
#ifndef TSCH_CONF_CHANNEL_SCAN_DURATION
#define TSCH_CONF_CHANNEL_SCAN_DURATION (1800*CLOCK_SECOND)
#endif

/* Increase this from the default 100 to improve TSCH association speed on this platform */
#ifndef TSCH_CONF_ASSOCIATION_POLL_FREQUENCY
#define TSCH_CONF_ASSOCIATION_POLL_FREQUENCY 1000
#endif

/* Slightly reduce the TSCH guard time (from 2200 usec to 1800 usec) to make sure
 * the CC26xx radio has sufficient time to start up. */
#ifndef TSCH_CONF_RX_WAIT
#define TSCH_CONF_RX_WAIT 1800
#endif

/* Path to CMSIS header */

/* Path to headers with implementation of mutexes, atomic and memory barriers */
//#define MUTEX_CONF_ARCH_HEADER_PATH          "mutex-cortex.h"
//#define ATOMIC_CONF_ARCH_HEADER_PATH         "atomic-cortex.h"
//#define MEMORY_BARRIER_CONF_ARCH_HEADER_PATH "memory-barrier-cortex.h"
/*---------------------------------------------------------------------------*/
//#define GPIO_HAL_CONF_ARCH_HDR_PATH          "dev/gpio-hal-arch.h"
#define GPIO_HAL_CONF_PORT_PIN_NUMBERING     0
/*---------------------------------------------------------------------------*/





#endif /* LABSCIM_DEF_H_ */
/*---------------------------------------------------------------------------*/
