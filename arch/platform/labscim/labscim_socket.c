
#include <sys/stat.h>        /* For mode constants */


#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h> //Specified in man 2 open
#include <sys/stat.h>
#include <arpa/inet.h>

#include <fcntl.h> // open function
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "labscim_socket.h"
#include "labscim_protocol.h"
#include "labscim_linked_list.h"
#include "shared_mutex.h"

#ifdef LABSCIM_LOG_COMMANDS
	FILE * gLogger=NULL;
#endif

//internal processing functions
static void labscim_buffer_process(buffer_circ_t* buf,struct labscim_ll* CommandsToExecute);
static size_t labscim_buffer_peek(buffer_circ_t* buf,void *data, size_t size);
static inline size_t labscim_buffer_available(buffer_circ_t* buf);
static inline size_t labscim_buffer_used(buffer_circ_t* buf);
static int32_t labscim_buffer_retrieve(buffer_circ_t* buf,void *data, uint32_t size);
static void labscim_socket_send(buffer_circ_t* buf, void* data, size_t size);
static void labscim_buffer_purge(buffer_circ_t* buf);
static inline uint32_t labscim_protocol_get_new_sequence_number();
static void* labscim_map_shared_memory(char* memory_name, size_t memory_size, uint8_t clear);
static void labscim_unmap_shared_memory(char* memory_name, void* memory, size_t memory_size, uint8_t del);

static uint32_t gSequenceNumber = 1;
static void (*gCommandSentCallback)(void) = NULL;

#define BUFF_END(buf) (buf->mem->data+buf->mem->size)

static inline uint32_t labscim_protocol_get_new_sequence_number()
{
	return gSequenceNumber++;
}

void labscim_set_send_command_callback(void (*Callback)(void) )
{
	gCommandSentCallback = Callback;
}



static void* labscim_map_shared_memory(char* memory_name, size_t memory_size, uint8_t clear)
{
	int64_t fd;


	// Open existing shared memory object, or create one.
	// Two separate calls are needed here, to mark fact of creation
	// for later initialization of pthread mutex.
	fd = shm_open(memory_name, O_RDWR, 0666);
	if((errno != ENOENT) && clear)
	{
		if (close(fd)) {
			perror("close");
			return NULL;
		}
		if (shm_unlink(memory_name))
		{
			perror("shm_unlink");
			return NULL;
		}
	}
	if ((errno == ENOENT) || clear)
	{
		fd = shm_open(memory_name, O_RDWR|O_CREAT, 0666);
	}
	if (fd == -1) {
		perror("shm_open");
		return NULL;
	}

	/* Truncate memory size */
	if (ftruncate(fd, memory_size) != 0 )
	{
		perror("Error creating shared memory");
		return NULL;
	}

	/* Map shared memory object */
	void * rptr = mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (rptr == MAP_FAILED)
	{
		perror("mmap");
		return NULL;
	}
	else
	{
		return rptr;
	}
}

static void labscim_unmap_shared_memory(char* memory_name, void* memory, size_t memory_size, uint8_t del)
{
	if(del)
	{
		/* Destroy shared memory */
		shm_unlink(memory_name);
	}
	else
	{
		/* Unmap shared memory */
		munmap(memory, memory_size);
	}
}

void labscim_buffer_init(buffer_circ_t* buf, char* buffer_name, size_t MemorySize, uint8_t clear)
{
#ifdef LABSCIM_REMOTE_SOCKET //socket communication, local buffer
	buf->mem = (buffer_circ_memory*)malloc(FIXED_SIZEOF_BUFFER_CIRC_MEMORY+sizeof(uint8_t)*MemorySize);
	if(buf->mem == NULL)
	{
		perror("\n Malloc error \n");
		return;
	}
#else
	char* mutex_name;
	uint64_t name_size = strlen(buffer_name);
	buf->name = (char*)malloc(name_size+1);
	if(buf->name == NULL)
	{
		perror("\n Malloc error \n");
		return;
	}
	mutex_name = (char*)malloc(name_size+6); //mutex appended to the end
	if(mutex_name == NULL)
	{
		free(buf->name);
		perror("\n Malloc error \n");
		return;
	}
	memcpy(buf->name,buffer_name,name_size+1);
	memcpy(mutex_name,buffer_name,name_size);
	memcpy(mutex_name+name_size,"mutex",6);
	mutex_name[name_size+5]=0;
	buf->mutex = shared_mutex_init(mutex_name, clear);
	if (buf->mutex.mutex == NULL)
	{
		perror("shared mutex\n");
		free(buf->name);
		return;
	}
	free(mutex_name);
	buf->mem = (buffer_circ_memory*)labscim_map_shared_memory(buf->name, FIXED_SIZEOF_BUFFER_CIRC_MEMORY+sizeof(uint8_t)*MemorySize, clear);
	if(buf->mem == NULL)
	{
		perror("shared buffer\n");
		free(buf->name);
		return;
	}
#endif
	if(clear)
	{
		buf->mem->size = MemorySize;
		buf->mem->wr_offset=0;
		buf->mem->rd_offset=0;
	}
}

void labscim_buffer_deinit(buffer_circ_t* buf, uint8_t del)
{
#ifdef LABSCIM_REMOTE_SOCKET //socket communication, local buffer
	if(buf->data != NULL)
	{
		free(buf->data);
		buf->data = NULL;
	}
#else
	if(buf!=NULL)
	{
	    if(buf->mem!=NULL)
	    {
	        labscim_unmap_shared_memory(buf->name, buf->mem, buf->mem->size,del);
	    }
	    if(buf->name!=NULL)
	    {
	        free(buf->name);
	    }
	    if(del)
	    {
	        shared_mutex_destroy(buf->mutex);
	    }
	    else
	    {
	        shared_mutex_close(buf->mutex);
	    }
	}
#endif
}

#if IS_LABSCIM_CLIENT

uint32_t node_is_ready(buffer_circ_t* buf)
{
    struct labscim_node_is_ready rd;
    rd.hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
    rd.hdr.labscim_protocol_code = LABSCIM_NODE_IS_READY;
    rd.hdr.message_size = sizeof(struct labscim_node_is_ready);
    rd.hdr.sequence_number = labscim_protocol_get_new_sequence_number();
    rd.hdr.request_sequence_number = 0;
    labscim_socket_send(buf, (void *)&rd, sizeof(struct labscim_node_is_ready));
    return rd.hdr.sequence_number;
}

uint32_t protocol_yield(buffer_circ_t* buf)
{
	struct labscim_protocol_yield py;
	py.hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	py.hdr.labscim_protocol_code = LABSCIM_PROTOCOL_YIELD;
	py.hdr.message_size = sizeof(struct labscim_protocol_yield);
	py.hdr.sequence_number = labscim_protocol_get_new_sequence_number();
	py.hdr.request_sequence_number = 0;
	labscim_socket_send(buf, (void *)&py, sizeof(struct labscim_protocol_yield));
	return py.hdr.sequence_number;
}

uint32_t set_time_event(buffer_circ_t* buf, uint32_t time_event_id, uint16_t is_relative, uint64_t time_us)
{
	struct labscim_set_time_event ste;
	ste.hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	ste.hdr.labscim_protocol_code = LABSCIM_SET_TIME_EVENT;
	ste.hdr.message_size = sizeof(struct labscim_set_time_event);
	ste.hdr.sequence_number = labscim_protocol_get_new_sequence_number();
	ste.hdr.request_sequence_number = 0;
	ste.time_event_id = time_event_id;
	ste.is_relative = is_relative?1:0;
	ste.time_us = time_us;
	labscim_socket_send(buf, (void *)&ste, sizeof(struct labscim_set_time_event));
	return ste.hdr.sequence_number;
}

uint32_t cancel_time_event(buffer_circ_t* buf, uint32_t cancel_sequence_number)
{
	struct labscim_cancel_time_event cte;
	cte.hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	cte.hdr.labscim_protocol_code = LABSCIM_CANCEL_TIME_EVENT;
	cte.hdr.message_size = sizeof(struct labscim_cancel_time_event);
	cte.hdr.sequence_number = labscim_protocol_get_new_sequence_number();
	cte.hdr.request_sequence_number = 0;
	cte.cancel_sequence_number = cancel_sequence_number;
	labscim_socket_send(buf, (void *)&cte, sizeof(struct labscim_cancel_time_event));
	return cte.hdr.sequence_number;
}

uint32_t print_message(buffer_circ_t* buf, uint16_t message_type, uint8_t* message, uint32_t msglen)
{
	uint32_t ret;
	ret = labscim_protocol_get_new_sequence_number();
	struct labscim_print_message* pm;
	if(msglen==0)
	{
		return 0;
	}
	if(message[msglen-1]!=0)
	{
		msglen++;
	}
	pm = (struct labscim_print_message*)malloc(FIXED_SIZEOF_STRUCT_LABSCIM_PRINT_MESSAGE + msglen);
	if(pm==NULL)
	{
		perror("\nMalloc error \n");
		return 0;
	}
	pm->hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	pm->hdr.labscim_protocol_code = LABSCIM_PRINT_MESSAGE;
	pm->hdr.message_size = FIXED_SIZEOF_STRUCT_LABSCIM_PRINT_MESSAGE + msglen;
	pm->hdr.sequence_number = ret;
	pm->hdr.request_sequence_number = 0;
	pm->message_type = message_type;
	memcpy(pm->message, message, msglen);
	pm->message[msglen-1]=0;
	labscim_socket_send(buf, (void *)pm, FIXED_SIZEOF_STRUCT_LABSCIM_PRINT_MESSAGE + msglen);
	free(pm);
	return ret;
}


uint32_t signal_register(buffer_circ_t* buf, uint8_t* signal_name)
{
	uint32_t ret;
	struct labscim_signal_register* sr;
	int64_t msglen = strlen(signal_name)+1;
	ret = labscim_protocol_get_new_sequence_number();
	sr = (struct labscim_signal_register*)malloc(FIXED_SIZEOF_STRUCT_LABSCIM_SIGNAL_REGISTER + msglen);
	if(sr==NULL)
	{
		perror("\nMalloc error \n");
		return 0;
	}
	sr->hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	sr->hdr.labscim_protocol_code = LABSCIM_SIGNAL_REGISTER;
	sr->hdr.message_size = FIXED_SIZEOF_STRUCT_LABSCIM_SIGNAL_REGISTER + msglen;
	sr->hdr.sequence_number = ret;
	sr->hdr.request_sequence_number = 0;
	memcpy(sr->signal_name, signal_name, msglen);
	labscim_socket_send(buf, (void *)sr, FIXED_SIZEOF_STRUCT_LABSCIM_SIGNAL_REGISTER + msglen);
	free(sr);
	return ret;
}

uint32_t signal_emit(buffer_circ_t* buf, uint64_t signal_id, double value)
{
	struct labscim_signal_emit se;
	se.hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	se.hdr.labscim_protocol_code = LABSCIM_SIGNAL_EMIT;
	se.hdr.message_size = sizeof(struct labscim_signal_emit);
	se.hdr.sequence_number = labscim_protocol_get_new_sequence_number();
	se.hdr.request_sequence_number = 0;
	se.signal_id = signal_id;
	se.value = value;
	labscim_socket_send(buf, (void *)&se, sizeof(struct labscim_signal_emit));
	return se.hdr.sequence_number;
}

uint32_t radio_command(buffer_circ_t* buf, uint16_t radio_command, uint8_t* radio_struct, uint32_t radio_struct_len)
{
	struct labscim_radio_command* rc;
	uint32_t ret;
	if(radio_struct_len==0)
	{
		return 0;
	}
	rc = malloc(FIXED_SIZEOF_STRUCT_LABSCIM_RADIO_COMMAND + radio_struct_len);
	if(rc==NULL)
	{
		perror("\nMalloc error \n");
		return 0;
	}
	rc->hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	rc->hdr.labscim_protocol_code = LABSCIM_RADIO_COMMAND;
	rc->hdr.message_size = FIXED_SIZEOF_STRUCT_LABSCIM_RADIO_COMMAND + radio_struct_len;
	ret = labscim_protocol_get_new_sequence_number();
	rc->hdr.sequence_number = ret;
	rc->hdr.request_sequence_number = 0;
	rc->radio_command = radio_command;
	memcpy(rc->radio_struct, radio_struct, radio_struct_len);
	labscim_socket_send(buf, (void *)rc, FIXED_SIZEOF_STRUCT_LABSCIM_RADIO_COMMAND + radio_struct_len);
	free(rc);
	return ret;
}

uint32_t get_random(buffer_circ_t* buf, uint8_t distribution_type, union random_number param_1, union random_number param_2, union random_number param_3)
{
	struct labscim_get_random gr;
	gr.hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	gr.hdr.labscim_protocol_code = LABSCIM_GET_RANDOM;
	gr.hdr.message_size = sizeof(struct labscim_get_random);
	gr.hdr.sequence_number = labscim_protocol_get_new_sequence_number();
	gr.hdr.request_sequence_number = 0;
	gr.distribution_type = distribution_type;
	gr.param_1 = param_1;
	gr.param_2 = param_2;
	gr.param_3 = param_3;	
	labscim_socket_send(buf, (void *)&gr, sizeof(struct labscim_get_random));
	return gr.hdr.sequence_number;
}

int32_t labscim_socket_connect(uint8_t* server_address, uint32_t server_port, buffer_circ_t* buf)
{

#ifdef LABSCIM_REMOTE_SOCKET //socket communication, local buffer
	struct sockaddr_in serv_addr;
	int64_t ret=-1;
	serv_addr.sin_family = AF_INET;
	//TODO: Port must be a command line parameter
	serv_addr.sin_port = htons(server_port);
	// Convert IPv4 and IPv6 addresses from text to binary form
	//TODO: Address must be a command line parameter
	if(inet_pton(AF_INET, server_address, &serv_addr.sin_addr)<=0)
	{
		perror("\nInvalid address/ Address not supported \n");
		return -1;
	}
	do
	{
		if ((buf->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			perror("\n Socket creation error \n");
		}
		ret = connect(buf->socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
		if(ret<0)
		{
			perror("\nConnection Attempt timeout \n");
			close(buf->socket);
			buf->socket = -1;
			usleep(1*1000*1000);
		}
	}while(ret<0);
	return buf->socket;
#endif
}

#else

uint32_t protocol_boot(buffer_circ_t* buf, void* message, size_t message_size)
{
	struct labscim_protocol_boot* pb;
	uint32_t ret;
	pb = (struct labscim_protocol_boot*)malloc(FIXED_SIZEOF_STRUCT_LABSCIM_PROTOCOL_BOOT + message_size);
	if(pb==NULL)
	{
		perror("\nMalloc error \n");
		return 0;
	}
	pb->hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	pb->hdr.labscim_protocol_code = LABSCIM_PROTOCOL_BOOT;
	pb->hdr.message_size = FIXED_SIZEOF_STRUCT_LABSCIM_PROTOCOL_BOOT + message_size;
	ret = labscim_protocol_get_new_sequence_number();
	pb->hdr.sequence_number = ret;
	pb->hdr.request_sequence_number = 0;
	memcpy(pb->message, message, message_size);
	labscim_socket_send(buf, (void *)pb, FIXED_SIZEOF_STRUCT_LABSCIM_PROTOCOL_BOOT + message_size);
	free(pb);
	return ret;
}

uint32_t time_event(buffer_circ_t* buf, uint32_t sequence_number, uint32_t time_event_id, uint64_t current_time_us)
{
	struct labscim_time_event te;
	te.hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	te.hdr.labscim_protocol_code = LABSCIM_TIME_EVENT;
	te.hdr.message_size = sizeof(struct labscim_time_event);
	te.hdr.sequence_number = labscim_protocol_get_new_sequence_number();
	te.hdr.request_sequence_number = sequence_number;
	te.time_event_id = time_event_id;
	te.current_time_us = current_time_us;
	labscim_socket_send(buf, (void *)&te, sizeof(struct labscim_time_event));
	return te.hdr.sequence_number;
}


uint32_t radio_response(buffer_circ_t* buf, uint16_t radio_response, uint64_t current_time, void* radio_struct, size_t radio_struct_len, uint32_t sequence_number)
{
	struct labscim_radio_response* rr;

	rr = (struct labscim_radio_response*)malloc(FIXED_SIZEOF_STRUCT_LABSCIM_RADIO_RESPONSE + radio_struct_len);
	if(rr==NULL)
	{
		perror("\nMalloc error \n");
		return 0;
	}
	rr->hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	rr->hdr.labscim_protocol_code = LABSCIM_RADIO_RESPONSE;
	rr->hdr.message_size = FIXED_SIZEOF_STRUCT_LABSCIM_RADIO_RESPONSE + radio_struct_len;
	rr->hdr.sequence_number = labscim_protocol_get_new_sequence_number();
	rr->hdr.request_sequence_number = sequence_number;
	rr->radio_response_code = radio_response;
	rr->current_time = current_time;
	memcpy(rr->radio_struct, radio_struct, radio_struct_len);
	labscim_socket_send(buf, (void *)rr, FIXED_SIZEOF_STRUCT_LABSCIM_RADIO_RESPONSE + radio_struct_len);
	free(rr);
	return sequence_number;
}

uint32_t signal_register_response(buffer_circ_t* buf, uint32_t sequence_number, uint64_t signal_id)
{
	struct labscim_signal_register_response rr;
	rr.hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	rr.hdr.labscim_protocol_code = LABSCIM_SIGNAL_REGISTER_RESPONSE;
	rr.hdr.message_size = sizeof(struct labscim_signal_register_response);
	rr.hdr.sequence_number = labscim_protocol_get_new_sequence_number();
	rr.hdr.request_sequence_number = sequence_number;
	rr.signal_id = signal_id;
	labscim_socket_send(buf, (void *)&rr, sizeof(struct labscim_signal_register_response));
	return rr.hdr.sequence_number;
}

uint32_t send_random(buffer_circ_t* buf, union random_number result, uint64_t sequence_number)
{
	struct labscim_signal_get_random_response grr;
	grr.hdr.labscim_protocol_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	grr.hdr.labscim_protocol_code = LABSCIM_GET_RANDOM_RESPONSE;
	grr.hdr.message_size = sizeof(struct labscim_signal_get_random_response);
	grr.hdr.sequence_number = labscim_protocol_get_new_sequence_number();
	grr.hdr.request_sequence_number = sequence_number;
	grr.result = result;
	labscim_socket_send(buf, (void *)&grr, sizeof(struct labscim_signal_get_random_response));
	return grr.hdr.sequence_number;
}

int32_t labscim_socket_connect(uint32_t server_port, buffer_circ_t* buf )
{
#ifdef LABSCIM_REMOTE_SOCKET //socket communication, local buffer

	uint64_t opt_true = 1;
	uint64_t opt_false = 0;
	int64_t ret;
	uint64_t addrlen;
	int64_t MasterSocket;

	struct sockaddr_in process_address;

	//create a master socket
	if( (MasterSocket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
	{
		return -1;
	}

	//set master socket to allow multiple connections ,
	//this is just a good habit, it will work without this
	ret =  setsockopt(MasterSocket, SOL_SOCKET, SO_REUSEADDR, (uint8_t*)&opt_true, sizeof(opt_true));
	if(ret<0)
	{
		return ret;
	}


	//set master socket to operate in dual stack mode
	//ret =  setsockopt(MasterSocket, SOL_SOCKET, IPV6_V6ONLY, (uint8_t*)&opt_false, sizeof(opt_false));
	//if(ret<0)
	//{
	//    return ret;
	//}

	//type of socket created
	process_address.sin_family = AF_INET;
	process_address.sin_addr.s_addr = INADDR_ANY;
	process_address.sin_port = htons( server_port );

	//bind the socket
	ret = bind(MasterSocket, (struct sockaddr *)&process_address, sizeof(process_address));
	if(ret<0)
	{
		return ret;
	}

	//try to specify maximum of 3 pending connections for the master socket
	ret = listen(MasterSocket, 3);
	if(ret<0)
	{
		return ret;
	}

	//accept the incoming connection
	addrlen = sizeof(process_address);

	buf->socket = accept(MasterSocket, (struct sockaddr *)&process_address, (socklen_t*)&addrlen);
	if(buf->socket<0)
	{
		return buf->socket;
	}

	ret = close(MasterSocket);
	if(ret<0)
	{
		return ret;
	}
	return buf->socket;

#endif
}
#endif

#ifdef LABSCIM_LOG_COMMANDS
char filename[32];
void labscim_log(char* data, char* ident)
{
    struct timeval start;
    gettimeofday(&start, NULL);

	if(gLogger==NULL)
	{
		sprintf(filename,"log%d.txt", getpid());
		gLogger = fopen(filename, "w+");
	}
	FILE* string = gLogger;
	fprintf(string,"%ld %s ",(start.tv_sec * 1000000 + start.tv_usec)%1000000000, ident);
	fprintf(string,"%s",data);
	fflush(gLogger);
}
#endif


#ifdef LABSCIM_LOG_COMMANDS
void labscim_cmd_log(void* data, char* ident)
{
	struct timeval start;

	gettimeofday(&start, NULL);

	struct labscim_protocol_header* cmd = (struct labscim_protocol_header*)data;
//	if(cmd->labscim_protocol_code!=LABSCIM_PRINT_MESSAGE)
//	{
//		return;
//	}

	if(gLogger==NULL)
	{
		sprintf(filename,"log%d.txt", getpid());
		gLogger = fopen(filename, "w+");
	}
	FILE* string = gLogger;

	fprintf(string,"%ld %s ",(start.tv_sec * 1000000 + start.tv_usec)%1000000000, ident);

	if(cmd->sequence_number == 134169)
	{
		cmd->sequence_number++;
		cmd->sequence_number--;
	}


	switch(cmd->labscim_protocol_code)
	{
	case LABSCIM_PROTOCOL_BOOT:
	{
		fprintf(string,"seq%7d\tPROTOCOL_BOOT\tsize:%4d",cmd->sequence_number,cmd->message_size);
		break;
	}
	case LABSCIM_TIME_EVENT:
	{
		struct labscim_time_event* te = (struct labscim_time_event*)cmd;
		fprintf(string,"seq%7d\tTIME_EVENT\tsize:%4d\tid:0x%08x\treq seq no:%4d\ttime:%9ld",cmd->sequence_number,cmd->message_size,te->time_event_id,te->hdr.request_sequence_number,te->current_time_us);
		break;
	}
	case LABSCIM_RADIO_RESPONSE:
	{
		struct labscim_radio_response* rr = (struct labscim_radio_response*)cmd;
		fprintf(string,"seq%7d\tRADIO_RESPONSE\tsize:%4d\tresponse code:0x%04x\trequest seq no:%04d",cmd->sequence_number,cmd->message_size,rr->radio_response_code, rr->hdr.request_sequence_number);
		break;
	}
	case LABSCIM_PROTOCOL_YIELD:
	{
		fprintf(string,"seq%7d\tPROTOCOL_YIELD\tsize:%4d",cmd->sequence_number,cmd->message_size);
		break;
	}
	case LABSCIM_SET_TIME_EVENT:
	{
		struct labscim_set_time_event* ste = (struct labscim_set_time_event*)cmd;
		fprintf(string,"seq%7d\tSET_TIME_EVENT\tsize:%4d\tid:0x%08x\ttime: %s%9ld",cmd->sequence_number,cmd->message_size,ste->time_event_id,ste->is_relative?"+":" ",ste->time_us);
		break;
	}
	case LABSCIM_CANCEL_TIME_EVENT:
	{
		struct labscim_cancel_time_event* cte = (struct labscim_cancel_time_event*)cmd;
		fprintf(string,"seq%7d\tCANCEL_TIME_EVT\tsize:%4d\tseq_no:%4d",cmd->sequence_number,cmd->message_size,cte->cancel_sequence_number);
		break;
	}
	case LABSCIM_PRINT_MESSAGE:
	{
		struct labscim_print_message* pm = (struct labscim_print_message*)cmd;
		//char* msg = (char*)malloc(pm->hdr.message_size - FIXED_SIZEOF_STRUCT_LABSCIM_PRINT_MESSAGE + 1);
		//memcpy(msg, pm->message,pm->hdr.message_size - FIXED_SIZEOF_STRUCT_LABSCIM_PRINT_MESSAGE);
		//msg[pm->hdr.message_size - FIXED_SIZEOF_STRUCT_LABSCIM_PRINT_MESSAGE]=0;
		fprintf(string,"seq%7d\tPRINT_MESSAGE\tsize:%4d\t",cmd->sequence_number,cmd->message_size);
		fwrite(pm->message,sizeof(char),pm->hdr.message_size - FIXED_SIZEOF_STRUCT_LABSCIM_PRINT_MESSAGE - 1,string);
		break;
	}
	case LABSCIM_RADIO_COMMAND:
	{
		struct labscim_radio_command* rc = (struct labscim_radio_command*)cmd;
		fprintf(string,"seq%7d\tRADIO_COMMAND\tsize:%4d\tcmd:0x%04x",cmd->sequence_number,cmd->message_size,rc->radio_command);
		break;
	}
	}
	if(cmd->labscim_protocol_code!=LABSCIM_PRINT_MESSAGE)
	{
		fprintf(string,"\n");
	}
	fflush(string);
}
#endif

static void labscim_socket_send(buffer_circ_t* buf, void* data, size_t size)
{
	size_t written=0;
#ifdef LABSCIM_LOG_COMMANDS
	labscim_cmd_log(data, "out ");
#endif
#ifdef LABSCIM_REMOTE_SOCKET
	//send
	if( send(buf->socket, data, size, 0) != size )
	{
		perror("send");
	}
#else
	//write to the shared memory, protected by a producer/consumer mutex
	pthread_mutex_lock(buf->mutex.mutex);

	while (labscim_buffer_available(buf) < size)
		pthread_cond_wait(buf->mutex.less, buf->mutex.mutex);

	labscim_buffer_direct_input(buf, data, size);

	if(gCommandSentCallback!=NULL)
	{
		gCommandSentCallback();
	}

	pthread_cond_signal(buf->mutex.more);
	pthread_mutex_unlock(buf->mutex.mutex);
#endif
}

size_t labscim_buffer_direct_input(buffer_circ_t* buf, void* data, size_t size)
{
	size_t max_writesize;
	size_t write_size;
	if(buf->mem->data == NULL)
	{
		/* check your buffer parameter */
		return 0;
	}
	if(buf->mem->wr_offset >= buf->mem->rd_offset)
	{
		max_writesize = buf->mem->size - buf->mem->wr_offset;
	}
	else
	{
		max_writesize = buf->mem->rd_offset - buf->mem->wr_offset - 1;
	}
	write_size = LABSCIM_MIN(max_writesize,size);
	memcpy(buf->mem->data + buf->mem->wr_offset, data,write_size);
	buf->mem->wr_offset += write_size;
	if(buf->mem->wr_offset == buf->mem->size) //warp
	{
		buf->mem->wr_offset = 0;
		return write_size + labscim_buffer_direct_input(buf, data+write_size, size-write_size);
	}
	return write_size;
}


void labscim_socket_handle_input(buffer_circ_t* buf, struct labscim_ll* CommandsToExecute)
{
#ifdef LABSCIM_REMOTE_SOCKET //shared memory communication
	size_t valread, readsize;

	if(buf->mem->data == NULL) {
		/* check your buffer parameter */
		return;
	}
	if(buf->mem->wr_offset >= buf->mem->rd_offset)
	{
		readsize = buf->mem->size - buf->mem->wr_offset;
	}
	else
	{
		readsize = buf->mem->rd_offset - buf->mem->wr_offset - 1;
	}
	if ((valread = read( buf->socket , buf->mem->data + buf->mem->wr_offset, readsize)) == 0)
	{
		perror("\n Connection Lost \n");
		while(1);
		return;
	}

	buf->mem->wr_offset += valread;

	if(buf->mem->wr_offset == buf->mem->size) //warp
	{
		buf->mem->wr_offset = 0;
	}
#endif
	labscim_buffer_process(buf, CommandsToExecute);
}


static void labscim_buffer_purge(buffer_circ_t* buf)
{
	//this function should never be called in TCP environment
	const uint32_t labscim_magic_number = LABSCIM_PROTOCOL_MAGIC_NUMBER;
	const uint8_t labscim_magic_byte = *((uint8_t*)&labscim_magic_number);
	if(buf->mem->rd_offset < buf->mem->wr_offset)
	{
		while(buf->mem->rd_offset < buf->mem->wr_offset)
		{
			if(*(buf->mem->data + buf->mem->rd_offset) != labscim_magic_byte)
			{
				buf->mem->rd_offset++;
			}
			else
			{
				if(*((uint32_t*)(buf->mem->data + buf->mem->rd_offset))==labscim_magic_number)
				{
					break;
				}
				else
				{
					buf->mem->rd_offset++;
				}
			}
		}
	}
	else
	{
		while(buf->mem->rd_offset < buf->mem->size)
		{
			uint32_t magic;
			if(*(buf->mem->data + buf->mem->rd_offset) != labscim_magic_byte)
			{
				buf->mem->rd_offset++;
			}
			else
			{
				labscim_buffer_peek(buf, &magic,sizeof(uint32_t));
				if(magic==labscim_magic_number)
				{
					break;
				}
				else
				{
					buf->mem->rd_offset++;
				}
			}
		}
		if(buf->mem->rd_offset == buf->mem->size)
		{
			buf->mem->rd_offset = 0;
			labscim_buffer_purge(buf); //is this recursive call too ugly?
		}
	}
}

static void labscim_buffer_process(buffer_circ_t* buf, struct labscim_ll* CommandsToExecute)
{
	struct labscim_protocol_header hdr;
	size_t bytes_available = labscim_buffer_used(buf);

	while(bytes_available >= sizeof(struct labscim_protocol_header))
	{
		if(labscim_buffer_peek(buf, &hdr,sizeof(hdr))==sizeof(hdr))
		{
			if(hdr.labscim_protocol_magic_number != LABSCIM_PROTOCOL_MAGIC_NUMBER)
			{
				labscim_buffer_purge(buf);
			}
			else //if(hdr.labscim_protocol_magic_number != LABSCIM_PROTOCOL_MAGIC_NUMBER)
			{
				if(hdr.message_size > buf->mem->size)
				{
					//this message is too big to be received
					uint8_t byte;
					labscim_buffer_retrieve(buf, &byte, sizeof(uint8_t));
					labscim_buffer_purge(buf);
				}
				if(bytes_available >= hdr.message_size)
				{
					void* msg;
					msg = malloc(hdr.message_size);
					if(msg == NULL)
					{
						perror("\nMalloc error\n");
						return;
					}
					labscim_buffer_retrieve(buf,msg,hdr.message_size);
					labscim_ll_insert_at_back(CommandsToExecute,msg);
#ifdef LABSCIM_LOG_COMMANDS
					labscim_cmd_log(msg, "in  ");
#endif
				}
				else
				{
				    break;
				}
			}
		} //if(labscim_buffer_peek(&hdr,sizeof(hdr))==sizeof(hdr))
		bytes_available = labscim_buffer_used(buf);
	} //while(bytes_available > sizeof(struct labscim_protocol_header))
	return;
}

static size_t labscim_buffer_peek(buffer_circ_t* buf, void *data, size_t size)
{
	if(buf->mem->data == NULL) {
		/* check your buffer parameter */
		return(0);
	}
	size = LABSCIM_MIN(size,labscim_buffer_used(buf));
	if(buf->mem->rd_offset + size < buf->mem->size)
	{
		//no warp
		memcpy(data, buf->mem->data + buf->mem->rd_offset, size);
	}
	else
	{
		//warp
		size_t sz1 = buf->mem->size - buf->mem->rd_offset;
		memcpy(data, buf->mem->data + buf->mem->rd_offset, sz1);
		memcpy(data+sz1,buf->mem->data,size-sz1);
	}
	return size;
}

static int32_t labscim_buffer_retrieve(buffer_circ_t* buf,void *data, uint32_t size)
{
	size_t rd = labscim_buffer_peek(buf, data,size);
	buf->mem->rd_offset= (buf->mem->rd_offset + rd) % buf->mem->size;
	return rd;
}

static inline size_t labscim_buffer_available(buffer_circ_t* buf)
{
	if(buf->mem->wr_offset >= buf->mem->rd_offset)
	{
		return (buf->mem->size-buf->mem->wr_offset+buf->mem->rd_offset);
	}
	else
	{
		return buf->mem->rd_offset - buf->mem->wr_offset;
	}
}

static inline size_t labscim_buffer_used(buffer_circ_t* buf)
{
    return buf->mem->size - labscim_buffer_available(buf);
}


int32_t labscim_socket_disconnect(buffer_circ_t* buf)
{
#ifdef LABSCIM_REMOTE_SOCKET //socket communication, local buffer
	if(buf->socket>0)
	{
		close(buf->socket);
	}
#endif
	return 0;
}


