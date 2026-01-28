        nolist
#ifndef    CRC_CITT_INC
#define    CRC_CITT_INC

;**********************************************************************
;                                                                     *
; Description: Macros to implement byte at a time ^16+^12+^5+1,       *
;              "CITT CRC16" - HDLC, X.25 16 bit CRC generation        *
;                                                                     *
; Author: Chris White (whitecf69@gmail.com)                           *
;                                                                     *
; Copyright (C) 2011 by Monitor Computing Services Limited, licensed  *
; under CC BY-NC-SA 4.0. To view a copy of this license, visit        *
; https://creativecommons.org/licenses/by-nc-sa/4.0/                  *
;                                                                     *
; This program is distributed in the hope that it will be useful, but *
; WITHOUT ANY WARRANTY; without even the implied warranty of          *
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
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


#endif
    list
