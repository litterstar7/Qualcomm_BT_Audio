#ifndef __CHARGERCOMMS_H__
#define __CHARGERCOMMS_H__
/** \file */
#if TRAPSET_CHARGERCOMMS

/**
 *  \brief         Transmit a message to a charger using 2-wire Charger Comms.
 *         Every message must contain a one octet header, followed by up to 14
 *  octets extra payload.
 *         The header is primarily populated by the system, the structure is as
 *  follows:
 *         0: Reserved (set to 0)
 *         D: Destination (2 bits)
 *            msb    lsb
 *             |      |
 *             00DD0000
 *               ^
 *               |
 *               |
 *               |   
 *             destination
 *        Destinations:
 *             0: Case
 *             1: Right Device
 *             2: Left Device
 *             3: Reserved
 *         Every transmit request will be followed with a
 *  MESSAGE_CHARGERCOMMS_STATUS containing transmission status.
 *         
 *  \param length             The total length of the packet (including the header) to be
 *  transmitted in octets.
 *             The minimum length is therefore 1, to hold the header and the
 *  maximum is 15 octets
 *             (1 octet header, 14 octet payload).
 *             
 *  \param data             The data to be transmitted, including the header.
 *             
 *  \return           Boolean to indicate whether the request was accepted. If True a
 *  MESSAGE_CHARGERCOMMS_STATUS will follow
 *           containing the status of the transmission.
 *           
 * 
 * \ingroup trapset_chargercomms
 */
bool ChargerCommsTransmit(uint16 length, uint8 * data);
#endif /* TRAPSET_CHARGERCOMMS */
#endif
