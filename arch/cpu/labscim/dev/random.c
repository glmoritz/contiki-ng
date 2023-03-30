/*
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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
 /**
 * \addtogroup labscim-cpu
 * @{
 *
 * \defgroup labscim Pseudo Random Number Generator (PRNG) 
 * @{
 * 
 * This file overrides os/lib/random.c. Note that the file name must
 * match the original file for the override to work.
 *
 * \file
 *        Implementation of Pseudo Random Number Generator for labscim
 * \author
 * Guilherme Luiz Moritz moritz@utfpr.edu.br
 */
/*---------------------------------------------------------------------------*/
#include <contiki.h>
#include "labscim_socket.h"
/*---------------------------------------------------------------------------*/
#include <stdint.h>
/*---------------------------------------------------------------------------*/
extern buffer_circ_t* gNodeOutputBuffer;
void* socket_wait_for_command(uint32_t command, uint32_t sequence_number);

/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/**
 * \brief   Generates a new random number using the PRNG.
 * \return  The random number.
 */
unsigned short
random_rand(void)
{
	unsigned short ret = 0;
	struct labscim_protocol_header* resp;
	union random_number parameter1;
	union random_number parameter2;
    union random_number unused;

	parameter1.int_number = 0;
    parameter2.int_number = 65534;
	unused.double_number = 0;
	uint32_t sequence_number = get_random(gNodeOutputBuffer, 14 /*intuniform*/, parameter1, parameter2, unused);
	do{
		resp =  (struct labscim_protocol_header*)socket_wait_for_command(0, 0);
		if(resp->request_sequence_number == sequence_number)
		{
			ret =  ((struct labscim_signal_get_random_response*)resp)->result.int_number;
			free(resp);
			break;
		}
		else
		{
			socket_process_command(resp);
		}
	}while(1); //ugly?
	return ret;
}


/*---------------------------------------------------------------------------*/
/**
 * \brief       Initialize the PRNG.
 * \param seed  Seed for the PRNG.
 */
void
random_init(unsigned short seed)
{
    //omnet must handle it 
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
