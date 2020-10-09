/*
 * labscim_dbg.c
 *
 *  Created on: 8 de jun de 2020
 *      Author: root
 */
#include "dbg.h"
#include "labscim_protocol.h"
#include "labscim_socket.h"
#include "labscim_log_levels.h"

#define DBG_PRINT_BUFFER_SIZE (256)
uint8_t gByteBuffer[DBG_PRINT_BUFFER_SIZE];
uint8_t gCursor=0;
extern buffer_circ_t* gNodeOutputBuffer;

void message_is_completed()
{
	gByteBuffer[gCursor]=0;
	print_message(gNodeOutputBuffer, LOGLEVEL_INFO, gByteBuffer,gCursor+1);
	gCursor = 0;
}


void dbg_char_in(unsigned char c)
{
	if(gCursor<DBG_PRINT_BUFFER_SIZE-1)
	{
		gByteBuffer[gCursor++]=c;
		if(c==0)
		{
			message_is_completed();
		}
		if(c==10) //newline
		{
			message_is_completed();
		}
	}
	if(gCursor==DBG_PRINT_BUFFER_SIZE-1)
	{
		message_is_completed();
	}
}

unsigned int dbg_send_bytes(const unsigned char *seq, unsigned int len)
{
	uint8_t i;
	for(i=0;i<len;i++)
	{
		dbg_char_in(seq[i]);
	}
	return len;
}
/**
 * \brief Print a character to debug output
 * \param c Character to print
 * \return Printed character
 */
int dbg_putchar(int c)
{
	dbg_char_in((unsigned char)c);
	return c;
}



