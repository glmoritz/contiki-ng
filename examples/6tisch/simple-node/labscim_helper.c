#include "labscim_socket.h"
extern buffer_circ_t* gNodeOutputBuffer;
void socket_process_command(struct labscim_protocol_header* hdr);
void* socket_wait_for_command(uint32_t command, uint32_t sequence_number);

uint64_t LabscimSignalRegister(uint8_t* signal_name)
{
	uint64_t ret = 0;
	struct labscim_protocol_header* resp;
	uint32_t sequence_number = signal_register(gNodeOutputBuffer, signal_name);
	do{
		resp =  (struct labscim_protocol_header*)socket_wait_for_command(0, 0);
		if(resp->request_sequence_number == sequence_number)
		{
			ret =  ((struct labscim_signal_register_response*)resp)->signal_id;
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

void LabscimSignalEmit(uint64_t id, double value)
{
	signal_emit(gNodeOutputBuffer, id, value);
}
