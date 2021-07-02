/*
 * labscim_contiking_setup.h
 *
 *  Created on: 5 de jun de 2020
 *      Author: root
 */

#ifndef LABSCIM_CONTIKING_SETUP_H_
#define LABSCIM_CONTIKING_SETUP_H_


struct contiki_node_setup
{
    uint8_t mac_addr[8];
    uint64_t startup_time;
    uint8_t output_logs;
    uint8_t tsch_coordinator;
}__attribute__((packed));


#endif /* LABSCIM_CONTIKING_SETUP_H_ */

