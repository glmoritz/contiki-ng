/*
 * labscim_helper.h
 *
 *  Created on: 20 de ago de 2020
 *      Author: root
 */

#ifndef EXAMPLES_6TISCH_SIMPLE_NODE_LABSCIM_HELPER_H_
#define EXAMPLES_6TISCH_SIMPLE_NODE_LABSCIM_HELPER_H_

#include <stdint.h>
#include "labscim_socket.h"
#include <stdarg.h>

uint64_t LabscimSignalRegister(uint8_t* signal_name);


void LabscimSignalEmitDouble(uint64_t id, double value);
void LabscimSignalEmitChar(uint64_t id, char* value, uint64_t size);
void LabscimSignalSubscribe(uint64_t id);

double LabscimExponentialRandomVariable(double mean);

int labscim_printf(const char *fmt, ...);


#endif /* EXAMPLES_6TISCH_SIMPLE_NODE_LABSCIM_HELPER_H_ */
