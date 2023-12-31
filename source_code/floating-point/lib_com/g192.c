/*====================================================================================
    EVS Codec 3GPP TS26.443 Nov 13, 2018. Version 12.11.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/


#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <stdint.h>
#else
#include <Winsock2.h>

typedef unsigned short     uint16_t;
typedef signed short       int16_t;
typedef unsigned int       uint32_t;
typedef signed int         int32_t;
typedef unsigned __int64   uint64_t;
typedef signed __int64     int64_t;
#endif
#include "options.h"
#include "g192.h"


#ifdef _MSC_VER
#pragma warning( disable : 4996 )
#endif

#define G192_SYNC_GOOD_FRAME (unsigned short) 0x6B21
#define G192_SYNC_BAD_FRAME  (unsigned short) 0x6B20
#define G192_BIT0            (unsigned short) 0x007F
#define G192_BIT1            (unsigned short) 0x0081
#define RTP_HEADER_PART1     (short)22               /* magic number by network simulator */

#ifndef MAX_BITS_PER_FRAME
#define MAX_BITS_PER_FRAME   2560
#endif

/*
 * Structures
 */

/* main handle */
struct __G192
{
    FILE * file;
};

/*
 * Functions
 */

G192_ERROR
G192_Reader_Open(G192_HANDLE* phG192, FILE * filename)
{
    /* create handle */
    *phG192 = (G192_HANDLE) calloc(1, sizeof(struct __G192) );
    if ( *phG192 == NULL )
    {
        return G192_MEMORY_ERROR;
    }
    memset(*phG192, 0, sizeof(struct __G192));

    /* associate file stream */
    (*phG192)->file = filename;
    if( (*phG192)->file == NULL )
    {
        G192_Reader_Close(phG192);
        return G192_FILE_NOT_FOUND;
    }
    return G192_NO_ERROR;
}

G192_ERROR
G192_ReadVoipFrame_compact(G192_HANDLE const hG192,
                           unsigned char * const serial,
                           short * const num_bits,
                           unsigned short * const rtpSequenceNumber,
                           unsigned int * const rtpTimeStamp,
                           unsigned int * const rcvTime_ms)
{
    short short_serial [MAX_BITS_PER_FRAME];
    G192_ERROR err;
    short i;

    err = G192_ReadVoipFrame_short(hG192, short_serial, num_bits, rtpSequenceNumber, rtpTimeStamp, rcvTime_ms);
    if(err != G192_NO_ERROR)
        return err;

    for(i=0; i<*num_bits; i++)
    {
        unsigned char bit = (short_serial[i] == G192_BIT1) ? 1 : 0;
        unsigned char bitinbyte = bit << (7- (i&0x7));
        if(!(i&0x7))
            serial[i>>3] = 0;
        serial[i>>3] |= bitinbyte;
    }
    return G192_NO_ERROR;
}

G192_ERROR
G192_ReadVoipFrame_short(G192_HANDLE const hG192,
                         short * const serial,
                         short * const num_bits,
                         unsigned short * const rtpSequenceNumber,
                         unsigned int * const rtpTimeStamp,
                         unsigned int * const rcvTime_ms)
{
    uint32_t rtpPacketSize;
    uint16_t rtpPacketHeaderPart1;
    uint32_t ssrc;
    uint16_t rtpPayloadG192[2];
    uint16_t rtpPayloadSize;

    /* RTP packet size */
    if(fread(&rtpPacketSize, sizeof(rtpPacketSize), 1, hG192->file) != 1)
    {
        if(feof( hG192->file) != 0)
        {
            return G192_EOF;
        }
        fprintf(stderr, "RTP Packet Size could't be read\n");

        return G192_READ_ERROR;
    }

    if(rtpPacketSize <= 12)
    {
        fprintf(stderr, "RTP Packet size too small: %ud\n", rtpPacketSize);
        return G192_INVALID_DATA;
    }

    /* RTP packet arrival time */
    if(fread(rcvTime_ms, sizeof(*rcvTime_ms), 1, hG192->file) != 1)
    {
        if(feof( hG192->file) != 0)
        {
            return G192_EOF;
        }
        fprintf(stderr, "Reception Time in ms could't be read\n");

        return G192_READ_ERROR;
    }

    /* RTP packet header (part without sequence number) */
    if(fread(&rtpPacketHeaderPart1, sizeof(rtpPacketHeaderPart1), 1, hG192->file) != 1)
    {
        if(feof( hG192->file) != 0)
        {
            return G192_EOF;
        }
        fprintf(stderr, "RTP Header couldn't be read\n");

        return G192_READ_ERROR;
    }

    if(rtpPacketHeaderPart1 != RTP_HEADER_PART1)
    {
        fprintf(stderr, "Unexpected RTP Packet header\n");
        return G192_INVALID_DATA;
    }

    /* RTP sequence number */
    if(fread(rtpSequenceNumber, sizeof(*rtpSequenceNumber), 1, hG192->file) != 1)
    {
        if(feof( hG192->file) != 0)
        {
            return G192_EOF;
        }
        fprintf(stderr, "RTP Sequence Number be read\n");

        return G192_READ_ERROR;
    }
    *rtpSequenceNumber = ntohs(*rtpSequenceNumber);

    /* RTP timestamp */
    if(fread(rtpTimeStamp, sizeof(*rtpTimeStamp), 1, hG192->file) != 1)
    {
        if(feof( hG192->file) != 0)
        {
            return G192_EOF;
        }
        fprintf(stderr, "RTP Timestamp could't be read\n");

        return G192_READ_ERROR;
    }
    *rtpTimeStamp = ntohl(*rtpTimeStamp);

    /* RTP ssrc */
    if(fread(&ssrc, sizeof(ssrc), 1, hG192->file) != 1)
    {
        if(feof( hG192->file) != 0)
        {
            return G192_EOF;
        }
        fprintf(stderr, "RTP SSRC could't be read\n");

        return G192_READ_ERROR;
    }

    /* RTP payload size */
    rtpPayloadSize = rtpPacketSize - 12;
    if(rtpPayloadSize <= 2)
    {
        fprintf(stderr, "RTP payload size too small: %u\n", rtpPayloadSize);
        return G192_INVALID_DATA;
    }

    /* RTP payload */
    if(fread(rtpPayloadG192, sizeof(short), 2, hG192->file) != 2)
    {
        if(feof( hG192->file) != 0)
        {
            return G192_EOF;
        }
        fprintf(stderr, "Premature end of file, cannot read G.192 header\n");
        return G192_READ_ERROR;
    }
    if(rtpPayloadG192[0] != G192_SYNC_GOOD_FRAME)
    {
        fprintf(stderr, "G192_SYNC_WORD missing from RTP payload!");
        return G192_INVALID_DATA;
    }
    *num_bits = rtpPayloadG192[1];
    if(*num_bits == 0u || *num_bits + 2u != rtpPayloadSize || *num_bits > MAX_BITS_PER_FRAME)
    {
        fprintf(stderr, "error in parsing RTP payload: rtpPayloadSize=%u nBits=%d",
                rtpPayloadSize, *num_bits);
        return G192_INVALID_DATA;
    }
    if( fread(serial, sizeof(short), *num_bits, hG192->file) != (unsigned short)*num_bits)
    {
        fprintf(stderr, "Premature end of file, cannot read G.192 payload\n");
        return G192_READ_ERROR;
    }

    return G192_NO_ERROR;
}

G192_ERROR
G192_Reader_Close(G192_HANDLE* phG192)
{
    if(phG192 == NULL || *phG192 == NULL)
        return G192_NO_ERROR;

    free( *phG192 );
    *phG192 = NULL;
    phG192 = NULL;
    return G192_NO_ERROR;
}

