/*
 * labscim-socket.h
 *
 *  Created on: 27 de mai de 2020
 *      Author: root
 */

#ifndef LABSCIM_SOCKET_H_
#define LABSCIM_SOCKET_H_

#include "labscim_protocol.h"
#include "labscim_linked_list.h"

#ifndef LABSCIM_REMOTE_SOCKET //shared memory communication
#include "shared_mutex.h"
#endif


#define LABSCIM_MIN(x,y) ((x)>(y)?(y):(x))
#define LABSCIM_MAX(x,y) ((x)>(y)?(x):(y))

typedef struct {
	uint64_t  wr_offset;
	uint64_t  rd_offset;
	size_t  size;
	uint8_t data[];
} buffer_circ_memory;
#define FIXED_SIZEOF_BUFFER_CIRC_MEMORY (2*sizeof(uint8_t*)+sizeof(size_t))

typedef struct {
  buffer_circ_memory* mem;
#ifndef LABSCIM_REMOTE_SOCKET //shared memory communication
  char* name;
  shared_mutex_t mutex;
#else
  uint64_t socket;
#endif
} buffer_circ_t;


//public API

#if IS_LABSCIM_CLIENT

    uint32_t protocol_yield(buffer_circ_t* buf);
    uint32_t set_time_event(buffer_circ_t* buf, uint32_t time_event_id, uint16_t is_relative, uint64_t time_us);
    uint32_t cancel_time_event(buffer_circ_t* buf, uint32_t cancel_sequence_number);
    uint32_t print_message(buffer_circ_t* buf, uint16_t message_type, uint8_t* message, uint32_t msglen);
    uint32_t radio_command(buffer_circ_t* buf, uint16_t radio_command, uint8_t* radio_struct, uint32_t radio_struct_len);
    int32_t labscim_socket_connect(uint8_t* server_address, uint32_t server_port, buffer_circ_t* buf);
    uint32_t node_is_ready(buffer_circ_t* buf);

#else

    uint32_t protocol_boot(buffer_circ_t* buf, void* message, size_t message_size);
    uint32_t time_event(buffer_circ_t* buf, uint32_t sequence_number, uint32_t time_event_id, uint64_t current_time_us);
    uint32_t radio_response(buffer_circ_t* buf, uint16_t radio_response, uint64_t current_time, void* radio_struct, size_t radio_struct_len, uint32_t sequence_number);
    uint32_t signal_register_response(buffer_circ_t* buf, uint32_t sequence_number, uint64_t signal_id);
    int32_t labscim_socket_connect(uint32_t server_port, buffer_circ_t* buf );
    uint32_t signal_register(buffer_circ_t* buf, uint8_t* signal_name);
    uint32_t signal_emit(buffer_circ_t* buf, uint64_t signal_id, double value);

#endif

void labscim_set_send_command_callback(void (*Callback)(void) );

int32_t labscim_socket_disconnect(buffer_circ_t* buf);
void labscim_socket_handle_input(buffer_circ_t* buf, struct labscim_ll* CommandsToExecute);
size_t labscim_buffer_direct_input(buffer_circ_t* buf, void* data, size_t size);
void labscim_buffer_init(buffer_circ_t* buf, char* buffer_name, size_t MemorySize, uint8_t clear);
void labscim_buffer_deinit(buffer_circ_t* buf, uint8_t del);

#ifdef LABSCIM_LOG_COMMANDS
	void labscim_log(char* data, char* ident);
#endif

#endif /* LABSCIM_SOCKET_H_ */
