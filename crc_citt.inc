#ifndef    CRC_CITT_INC
#define    CRC_CITT_INC
        nolist

;**********************************************************************
;                                                                     *
; Description: Macros to implement byte at a time ^16+^12+^5+1,       *
;              "CITT CRC16" - HDLC, X.25 16 bit CRC generation        *
;                                                                     *
;    Author:   Chris White (whitecf@bcs.org.uk)                       *
;    Company:  Monitor Computing Services Ltd.                        *
;                                                                     *
;**********************************************************************
;                                                                     *
;    Copyright (C) 2011  Monitor Computing Services Ltd.              *
;                                                                     *
;    This program is free software; you can redistribute it and/or    *
;    modify it under the terms of the GNU General Public License      *
;    as published by the Free Software Foundation; either version 2   *
;    of the License, or any later version.                            *
;                                                                     *
;    This program is distributed in the hope that it will be useful,  *
;    but WITHOUT ANY WARRANTY; without even the implied warranty of   *
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
;    GNU General Public License for more details.                     *
;                                                                     *
;    You should have received a copy of the GNU General Public        *
;    License (http://www.gnu.org/copyleft/gpl.html) along with this   *
;    program; if not, write to:                                       *
;       The Free Software Foundation Inc.,                            *
;       59 Temple Place - Suite 330,                                  *
;       Boston, MA  02111-1307,                                       *
;       USA.                                                          *
;                                                                     *
;**********************************************************************


;**********************************************************************
;                                                                     *
; Macro:     CrcHDLC                                                  *
;                                                                     *
;            Macro for ^16+^12+^5+1, "CITT CRC16" - HDLC, X.25        *
;                                                                     *
; Arguments: newData    - data byte (destroyed)                       *
;            crcLow     - CRC accumulator low byte                    *
;            crcHigh    - CRC accumulator high byte                   *
;                                                                     *
;**********************************************************************
CrcHDLC     macro   newData, crcLow, crcHigh

    ; oldCRC ^= newData
    ; Modified to  - newData ^= oldCRC
    movf    crcLow,W
    xorwf   newData,F

    ; oldCRC  = (oldCRC >> 8) | (oldCRC << 8)
    ; Modified to - oldCRC  = (oldCRC >> 8) | newData << 8)
    movf    crcHigh,W
    movwf   crcLow
    movf    newData,W
    movwf   crcHigh

    ; oldCRC ^= (oldCRC & 0xFF00) << 4
    swapf   crcHigh,W
    andlw   0xF0
    xorwf   crcHigh,F

    ; oldCRC ^= oldCRC >> 12
    swapf   crcHigh,W
    andlw   0x0F
    xorwf   crcLow,F

    ; oldCRC ^= (oldCRC & 0xFF00) >> 5
    swapf   crcHigh,F
    bcf     STATUS,C
    btfsc   crcHigh,0
    bsf     STATUS,C
    rrf     crcHigh,W
    andlw   0xF8
    xorwf   crcLow,F
    rrf     crcHigh,W
    andlw   0x07
    swapf   crcHigh,F
    xorwf   crcHigh,F

    endm


    list
#endif
