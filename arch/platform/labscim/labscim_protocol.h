/*
 * labscim_protocol.h
 *
 *  Created on: 27 de mai de 2020
 *      Author: root
 */

#ifndef LABSCIM_PROTOCOL_H_
#define LABSCIM_PROTOCOL_H_

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "labscim_protocol.h"

//WARNING - this protocol can only be used with client-servers with the same endianess
#define LABSCIM_PROTOCOL_MAGIC_NUMBER (0x96089608)

/**
 * This is the labscim_protocol_header
 */
struct labscim_protocol_header
{
	uint32_t labscim_protocol_magic_number; /**< All LabSCim messages start with the #LABSCIM_PROTOCOL_MAGIC_NUMBER number*/
	uint16_t labscim_protocol_code; /**< Message code, Must be initialized with #LABSCIM_PROTOCOL_BOOT number*/
	uint16_t message_size; /**< The message size, including all headers*/
	uint32_t sequence_number; /**< The sequence number, which should be initialized with an unique number. responses will contain the same sequence number as its requests*/
	uint32_t request_sequence_number; /**< The sequence number of the request that generated this response, 0 if this message is not a response for any request*/
} __attribute__((packed));

//Server->Client Messages

#define LABSCIM_PROTOCOL_BOOT (0x9999)
/**
 * When a LabSCim node boots, it keeps waiting for this message in order to start its execution. It is the first message that should be sent from omnet upon process spawn, right after the bootstrap message on omnet code.
 */
struct labscim_protocol_boot
{
	struct labscim_protocol_header hdr;
	uint8_t message[]; /**< Variable length message, which can be used to pass configuration information from omnet to node, upon boot*/
} __attribute__((packed));
#define FIXED_SIZEOF_STRUCT_LABSCIM_PROTOCOL_BOOT (sizeof(struct labscim_protocol_header))

#define LABSCIM_TIME_EVENT (0xA1A1)
/**
 * This message indicates progression of time to the node. It must be sent when a timer has expired (which should unblock one or more threads) of prior packet reception (which should not unblock anything).
 * A timer is programmed by the node using the #labscim_set_time_event message
 */
struct labscim_time_event
{
	struct labscim_protocol_header hdr;
	uint32_t time_event_id; /**< The number that was passed to omnet when this time_event was requested via #labscim_set_time_event message (to identify the timer event)*/
	uint64_t current_time_us; /**< the current simulation time, in microseconds*/
} __attribute__((packed));

/**
 * This message is a radio response
 */
#define LABSCIM_RADIO_RESPONSE (0xA3A3)
struct labscim_radio_response
{
	struct labscim_protocol_header hdr;
	uint16_t radio_response_code; /**< This number is a radio specific command (check the radio protocol for this specific model)*/
	uint64_t current_time; /**< This number is a radio specific command (check the radio protocol for this specific model)*/
	uint8_t radio_struct[]; /**< This is a command specific struct (check the radio protocol for this specific model)*/
} __attribute__((packed));
#define FIXED_SIZEOF_STRUCT_LABSCIM_RADIO_RESPONSE (sizeof(struct labscim_protocol_header)+sizeof(uint16_t)+sizeof(uint64_t))

/**
 * This message is the response for a registration of an omnet signal that can be used for result gathering
 */
#define LABSCIM_SIGNAL_REGISTER_RESPONSE (0xA6A7)
struct labscim_signal_register_response
{
    struct labscim_protocol_header hdr;
    uint64_t signal_id; /**< This is the id of the registered signal*/
} __attribute__((packed));


//Client->Server Messages

/**
 * This message indicates to omnet that this wakeup round has finished. It must be sent when all threads on the node are blocked waiting for a message or blocked waiting for a timer
 * Upon reception, the simulation proceeds with the next event and the not MUST not send any messages prior to receiving a new message from omnet
 */
#define LABSCIM_PROTOCOL_YIELD (0x9898)
struct labscim_protocol_yield
{
	struct labscim_protocol_header hdr;
} __attribute__((packed));

/**
 * This message programs a #labscim_time_event on omnet simulation engine.
 */
#define LABSCIM_SET_TIME_EVENT (0xA0A0)
struct labscim_set_time_event
{
	struct labscim_protocol_header hdr;
	uint32_t time_event_id; /**< This number will be reported back to the node (via #labscim_time_event::time_event_id field) when the event expires*/
	uint16_t is_relative; /**< If set to zero, labscim_set_time_event::time_us will be interpreted as an absolute time. The message will have no effect if the time is in the simulation past. If set to 1, the message will expire #labscim_set_time_event::time_us microsseconds in the simulation future*/
	uint64_t time_us; /**< The number of microsseconds to wait until wakeup, in microsseconds or the absolute time to wakeup in microsseconds, depending on the value passed in #labscim_set_time_event::is_relative*/
} __attribute__((packed));


/**
 * This message cancels a #labscim_time_event on omnet simulation engine.
 */
#define LABSCIM_CANCEL_TIME_EVENT (0xB0B0)
struct labscim_cancel_time_event
{
	struct labscim_protocol_header hdr;
	uint32_t cancel_sequence_number; /**< The sequence number, of the labscim_set_time_event to be canceled*/
} __attribute__((packed));


/**
 * This message prints a message on omnet log for this node
 */
#define LABSCIM_PRINT_MESSAGE (0xA2A2)
struct labscim_print_message
{
	struct labscim_protocol_header hdr;
	uint16_t message_type; /**< This number corresponds to the different omnet log levels, one day I shall enumerate them*/
	uint8_t message[]; /**< Variable length message to be printed*/
} __attribute__((packed));
#define FIXED_SIZEOF_STRUCT_LABSCIM_PRINT_MESSAGE (sizeof(struct labscim_protocol_header)+sizeof(uint16_t))


/**
 * This message sends a radio command to the omnet radio
 */
#define LABSCIM_RADIO_COMMAND (0xA4A4)
struct labscim_radio_command
{
	struct labscim_protocol_header hdr;
	uint16_t radio_command; /**< This number is a radio specific command (check the radio protocol for this specific model)*/
	uint8_t radio_struct[]; /**< This is a command specific struct (check the radio protocol for this specific model)*/
} __attribute__((packed));
#define FIXED_SIZEOF_STRUCT_LABSCIM_RADIO_COMMAND (sizeof(struct labscim_protocol_header)+sizeof(uint16_t))


/**
 * This message is sent by the node (only when using shared memory) to indicate that it is already able to start processing
 */
#define LABSCIM_NODE_IS_READY (0xA5A5)
struct labscim_node_is_ready
{
    struct labscim_protocol_header hdr;
} __attribute__((packed));


/**
 * This message is registers an omnet signal that can be used for result gathering
 */
#define LABSCIM_SIGNAL_REGISTER (0xA6A6)
struct labscim_signal_register
{
    struct labscim_protocol_header hdr;
    uint8_t signal_name[]; /**< This is the name of the signal to be registered*/
} __attribute__((packed));
#define FIXED_SIZEOF_STRUCT_LABSCIM_SIGNAL_REGISTER (sizeof(struct labscim_protocol_header))

/**
 * This message emits a omnet signal
 */
#define LABSCIM_SIGNAL_EMIT (0xA7A7)
struct labscim_signal_emit
{
    struct labscim_protocol_header hdr;
    uint64_t signal_id; /**< This is the id of the registered signal*/
    double value; /**< This is the value to be emitted*/
} __attribute__((packed));



#endif /* LABSCIM_PROTOCOL_H_ */
