#ifndef __OMNET_RADIO_MODE_H
#define __OMNET_RADIO_MODE_H

typedef enum {
    /**
     * The radio is turned off, frame reception or transmission is not
     * possible, power consumption is zero, radio mode switching is slow.
     */
    RADIO_MODE_OFF,

    /**
     * The radio is sleeping, frame reception or transmission is not possible,
     * power consumption is minimal, radio mode switching is fast.
     */
    RADIO_MODE_SLEEP,

    /**
     * The radio is prepared for frame reception, frame transmission is not
     * possible, power consumption is low when receiver is idle and medium
     * when receiving.
     */
    RADIO_MODE_RECEIVER,

    /**
     * The radio is prepared for frame transmission, frame reception is not
     * possible, power consumption is low when transmitter is idle and high
     * when transmitting.
     */
    RADIO_MODE_TRANSMITTER,

    /**
     * The radio is prepared for simultaneous frame reception and transmission,
     * power consumption is low when transceiver is idle, medium when receiving
     * and high when transmitting.
     */
    RADIO_MODE_TRANSCEIVER,

    /**
     * The radio is switching from one mode to another, frame reception or
     * transmission is not possible, power consumption is minimal.
     */
    RADIO_MODE_SWITCHING    // this radio mode must be the very last
} RadioMode;

#endif
