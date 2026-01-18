/**
 * @file RadioSend.h
 * @brief VW-protocol radio communication interface
 * 
 * Declares the main function for sending vehicle data to the Android head unit
 * using the VW Polo UART protocol.
 */

#ifndef RADIO_SEND_H
#define RADIO_SEND_H

/**
 * @brief Process and send all pending vehicle data updates to the radio
 * 
 * This function should be called from the main loop. It handles:
 * - Protocol handshake with the head unit
 * - Steering wheel angle updates (100ms interval)
 * - Dashboard data: RPM, speed, battery, temperature, fuel (400ms interval)
 * - Door status updates (on change only)
 */
void processRadioUpdates();

#endif
