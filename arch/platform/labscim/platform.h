/*
 * platform.h
 *
 *  Created on: 16 de jun de 2020
 *      Author: root
 */

#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <stdint.h>
#include "labscim_protocol.h"

void socket_process_command(struct labscim_protocol_header* hdr);
void socket_process_all_commands();
void* socket_pop_command(uint32_t command, uint32_t sequence_number);
void* socket_wait_for_command(uint32_t command, uint32_t sequence_number);


#endif /* PLATFORM_H_ */
