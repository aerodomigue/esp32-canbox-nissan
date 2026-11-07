/**
 * @file RadioSend.h
 * @brief Toyota/RAV4 CANBox UART output for FYT and DUDU head units
 * 
 * Declares the main function for sending vehicle data to the Android head unit
 * using 0x2E frames with per-field units and the selected receiver contract.
 */

#ifndef RADIO_SEND_H
#define RADIO_SEND_H

/**
 * @brief Process and send all pending vehicle data updates to the radio
 * 
 * This function should be called from the main loop. It handles:
 * - Calibrated steering and lighting updates (200 ms)
 * - Door updates (250 ms or on change)
 * - RPM, speed, consumption, temperatures and trip data at their own intervals
 * - Source availability/freshness checks before numeric publication
 *
 * Incoming UART bytes are currently drained; request replies are not implemented.
 * Head-unit supply voltage uses its internal MCU rather than this external UART.
 */
void processRadioUpdates();

#endif
