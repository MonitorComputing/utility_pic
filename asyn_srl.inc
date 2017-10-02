    nolist

;**********************************************************************
;                                                                     *
; Description: Macros to implement asynchronous (or synchronous if    *
;              SYNC_SERIAL is defined), byte orientated (1 start,     *
;              8 data, no parity, 1 stop) serial interface.           *
;              Actual reception and transmission is achieved by       *
;              'service' subroutines.  Except when synchronous        *
;              exchange is taking place these will be called more     *
;              than once for each bit, i.e. if called as part of a    *
;              10 KHz interrupt and running at 5K baud each routine   *
;              would be called twice for each bit.                    *
;              Exchange of data observes RS232 conventions of 'least  *
;              significant bit' first, 0 equals line high, and 1      *
;              equals line low.                                       *
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
;                                                                     *
;  These macros do not employ 'returns' and so their instances can be *
;  employed 'inline':                                                 *
;                                                                     *
;    EnableRx - Enable receive hardware.                              *
;    EnableTx - Enable transmit hardware.                             *
;    InitRx   - Initialise receivesoftware.                           *
;    InitTx   - Initialise transmit software.                         *
;    TxBreak  - Cause transmission of a break.                        *
;    IsTxIdle - Test if no transmission in progress.                  *
;                                                                     *
;  These macros employ 'returns' and so their instances must be       *
;  invoked as subroutine calls:                                       *
;                                                                     *
;    ServiceRx - Service actual receptions of data.                   *
;    ServiceTx - Service actual transmission of data.                 *
;    SerialRx  - Get most recently received byte, if available.       *
;    SrlRxInd  - Save most recently received byte, if available.      *
;    SerialTx  - Start transmission of byte, if transmitter not busy. *
;    SrlTxInd  - Start transmission of byte, if transmitter not busy. *
;                                                                     *
; Each instance of a serial interface requires -                      *
;                                                                     *
; Variables:                                                          *
;     Receive:                                                        *
;            rxStat     - Receive status variable                     *
;            rxTimer    - Receive timer variable                      *
;            rxBitCount - Receive bits counter                        *
;            rxReg      - Receive data shift register                 *
;            rxBffr     - Received data buffer (loaded from rxReg)    *
;            RXTRIS     - Receive port direction register             *
;            RXPORT     - Receive port data register                  *
;     Transmit:                                                       *
;            txStat     - Transmit status variable                    *
;            txTimer    - Transmit timer variable                     *
;            txBitCount - Transmit bits counter                       *
;            txReg      - Transmit data shift register                *
;            txBffr     - Transmit data buffer (loaded into txReg)    *
;            TXTRIS     - Transmit port direction register            *
;            TXPORT     - Transmit port data register                 *
;                                                                     *
; Constants:                                                          *
;     Receive:                                                        *
;            CYCRXINI - Number of cycles per bit for first bit        *
;            CYCRXBIT - Number of cycles per bit                      *
;            RXFLG    - Receive buffer full status bit                *
;            RXERR    - Receive error status bit                      *
;            RXBRK    - Received break status bit                     *
;            RXSTOP   - Seeking stop bit                              *
;            RXBIT    - Receive port bit to use                       *
;     Transmit:                                                       *
;            CYCTXBIT - Number of cycles per bit                      *
;            TXFLG    - Transmit buffer clear status bit              *
;            TXBRK    - Send break status bit                         *
;            TXBIT    - Transmit port bit to use                      *
;                                                                     *
; Switches:  SYNC_SERIAL - 'Synchronous mode', no bit timing needed   *
;            CLKD_SERIAL - 'Clocked serial', external bit timing      *
;            SYNC_TXCD   - 'Tx collison detect', check last Tx bit    *
;                                                                     *
; Notes: If the macro 'SerialTx' is employed and will be interrupted  *
;        the interrupt code must preserve the contents of the FSR     *
;        register.                                                    *
;        If an interface is running half duplex the following         *
;        constants and variables can be equated in order to reduce    *
;        memory use,                                                  *
;                                                                     *
;            RXFLG      == TXFLG                                      *
;            RXBRK      == TXBRK                                      *
;            rxStat     == txStat                                     *
;            rxTimer    == txTimer                                    *
;            rxBitCount == txBitCount                                 *
;            rxReg      == txReg                                      *
;            rxBffr     == txBffr                                     *
;                                                                     *
;        Also as there are only four status bits so two interfaces    *
;        can share the same status variable.                          *
;                                                                     *
;        If an interface is operating in synchronous mode,            *
;        i.e. SYNC_SERIAL defined when macros instanced, then the     *
;        following will not be used and dummy values should be passed *
;        as macro arguments; CYCRXINI, CYCRXBIT, CYCTXBIT, rxTimer,   *
;        txTimer.                                                     *
;                                                                     *
;**********************************************************************


#ifdef SYNC_SERIAL
#ifndef CLKD_SERIAL
#define CLKD_SERIAL
#endif
#endif


;**********************************************************************
; Constants                                                           *
;**********************************************************************

RX_BITS     EQU  9 ; 1 start (not counted), 8 data, no parity, 1 stop
TX_BITS     EQU 10 ; 1 start, 8 data, no parity, 1 stop
BRK_BITS    EQU 12 ; Number of bits to send for break (> TX_BITS)
BRKSTP_BITS EQU  2 ; Number of stop bits to send at end of break
MSB         EQU  7 ; Bit number of most significant bit

RX_BUSY     EQU  0 ; Returned to indicate data being received
RX_IDLE     EQU  1 ; Returned to indicate no data being received
RX_BRK      EQU  2 ; Returned to indicate break has been received

TX_BUSY     EQU  0 ; Returned to indicate data being transmitted
TX_IDLE     EQU  1 ; Returned to indicate no data being transmitted
TX_CLSN     EQU  3 ; Returned to indicate detection of Tx collision


;**********************************************************************
;                                                                     *
; Macro:     EnableRx                                                 *
;                                                                     *
;            Macro to enable serial receive physical interface.       *
;                                                                     *
; Arguments: RXTRIS - Receive port direction register                 *
;            RXPORT - Receive port data register                      *
;            RXBIT  - Receive port bit to use                         *
;                                                                     *
;**********************************************************************
EnableRx    macro   RXTRIS, RXPORT, RXBIT

    BANKSEL OPTION_REG
    bsf     RXTRIS,RXBIT    ; Set direction of Rx bit as input
    BANKSEL TMR0
    bsf     RXPORT,RXBIT    ; Set Rx bit (in case using Port A)

    endm


;**********************************************************************
;                                                                     *
; Macro:     EnableTx                                                 *
;                                                                     *
;            Macro to enable serial transmit physical interface.      *
;                                                                     *
; Arguments: TXTRIS - Transmit port direction register                *
;            TXPORT - Transmit port data register                     *
;            TXBIT  - Transmit port bit to use                        *
;                                                                     *
;**********************************************************************
EnableTx    macro   TXTRIS, TXPORT, TXBIT

    bcf     TXPORT,TXBIT    ; Clear Tx output, 'mark' (line idle)
    BANKSEL OPTION_REG
    bcf     TXTRIS,TXBIT    ; Set direction of Tx bit as output
    BANKSEL TMR0

    endm


;**********************************************************************
;                                                                     *
; Macro:     AcquireTx                                                *
;                                                                     *
;            Macro to acquire serial transmit physical interface.     *
;                                                                     *
; Arguments: TXTRIS - Transmit port direction register                *
;            TXPORT - Transmit port data register                     *
;            TXBIT  - Transmit port bit to use                        *
;                                                                     *
; Returns  : STATUS,Z - Set if Tx interface acquired, clear if not    *
;                                                                     *
;**********************************************************************
AcquireTx   macro   TXTRIS, TXPORT, TXBIT

    bcf     STATUS,Z        ; Set default return status of Tx not acquired

    btfss   TXPORT,TXBIT    ; Skip if Tx 'space' ...
    return                  ; ... otherwise return

    bcf     TXPORT,TXBIT    ; Clear Tx output, 'mark' (line idle)
    BANKSEL OPTION_REG
    bcf     TXTRIS,TXBIT    ; Set direction of Tx bit as output
    BANKSEL TMR0

    bsf     STATUS,Z        ; Return status = 'Tx interface acquired'
    return                  ; Return

    endm


;**********************************************************************
;                                                                     *
; Macro:     InitRx                                                   *
;                                                                     *
;            Macro to initialise serial interface receive variables.  *
;                                                                     *
; Arguments: rxStat     - Serial interface receive status variable    *
;            rxTimer    - Receive timer variable                      *
;            rxBitCount - Receive bits counter                        *
;            rxReg      - Receive data shift register                 *
;            RXFLG      - Receive buffer full status bit              *
;            RXERR      - Receive error status bit                    *
;            RXBRK      - Received break status bit                   *
;            RXSTOP     - Seeking stop bit status bit                 *
;                                                                     *
;**********************************************************************
InitRx      macro   rxStat, rxTimer, rxBitCount, rxReg, RXFLG, RXERR, RXBRK , RXSTOP

    clrf    rxBitCount
#ifndef SYNC_SERIAL
    clrf    rxTimer
    incf    rxTimer,F
#endif
    clrf    rxReg
    decf    rxReg,F
    bcf     rxStat,RXBRK    ; Clear the received break flag
    bcf     rxStat,RXFLG    ; Clear Rx buffer full flag

    ; In case Rx is enabled during transmission of a byte by other party
    ; make receiver wait for stop bit and discard anything received prior
    ; to seeing this. This avoids mistaking line settling time as data.
    bsf     rxStat,RXSTOP   ; Set the seeking stop bit flag
    bsf     rxStat,RXERR    ; Set the Rx error flag

    endm


;**********************************************************************
;                                                                     *
; Macro:     InitTx                                                   *
;                                                                     *
;            Macro to initialise serial interface transmit variables. *
;                                                                     *
; Arguments: txStat     - Serial interface transmit status variable   *
;            txTimer    - Transmit timer variable                     *
;            txBitCount - Transmit bits counter                       *
;            txReg      - Transmit data shift register                *
;            TXFLG      - Transmit buffer clear status bit            *
;            TXBRK      - Send break status bit                       *
;                                                                     *
;**********************************************************************
InitTx      macro   txStat, txTimer, txBitCount, txReg, TXFLG, TXBRK

    clrf    txBitCount
#ifndef SYNC_SERIAL
    clrf    txTimer
    incf    txTimer,F
#endif
    clrf    txReg
    bcf     txStat,TXBRK    ; Clear the send break flag
    bsf     txStat,TXFLG    ; Set Tx byte clear flag

    endm


;**********************************************************************
;                                                                     *
; Macro:     TxBreak                                                  *
;                                                                     *
;            Macro to cause transmission of a break.                  *
;                                                                     *
; Arguments: txStat     - Serial interface transmit status variable   *
;            TXBRK      - Send break status bit                       *
;                                                                     *
;**********************************************************************
TxBreak      macro   txStat, TXBRK

    bsf     txStat,TXBRK

    endm


;**********************************************************************
;                                                                     *
; Macro:     ServiceRx                                                *
;                                                                     *
;            Macro to service serial interface reception.             *
;                                                                     *
; Arguments: rxStat     - Receive status variable                     *
;            rxTimer    - Receive timer variable                      *
;            rxBitCount - Receive bits counter                        *
;            rxReg      - Receive data shift register                 *
;            rxBffr     - Received data buffer (loaded from rxReg)    *
;            RXPORT     - Receive port data register                  *
;            RXBIT      - Receive port bit to use                     *
;            CYCRXINI   - Number of cycles per bit for first bit      *
;            CYCRXBIT   - Number of cycles per bit                    *
;            RXERR      - Receive error status bit                    *
;            RXBRK      - Received break status bit                   *
;            RXNSTOP    - Seeking stop bit status bit                 *
;            RXFLG      - Receive buffer full status bit              *
;                                                                     *
; Return   : W - RX_IDLE, not receiving data                          *
;                RX_BUSY, receiving data                              *
;                RX_BRK,  received break                              *
;                                                                     *
;**********************************************************************
ServiceRx   macro   rxStat, rxTimer, rxBitCount, rxReg, rxBffr, RXPORT, RXBIT, CYCRXINI, CYCRXBIT, RXERR, RXBRK , RXSTOP, RXFLG

    local   BeginRx, ContinueRx, SeekStopBit, Overrun, RxBrkBit, RxDataBit

    ; Check to see if currently seeking stop bit
    btfsc   rxStat,RXSTOP   ; Test the seeking stop flag, skip if clear ...
    goto    SeekStopBit     ; ... else jump (seeking stop bit)

    ; Check to see if currently receiving a byte
    movf    rxBitCount,F    ; Test Rx bit count
    btfss   STATUS,Z        ; Skip if zero (no Rx in progress) ...
    goto    ContinueRx      ; ... else jump (Rx in progress)

    ; Not receiving, look for start of Rx byte
    btfss   RXPORT,RXBIT    ; Skip if start bit detected ...
    retlw   RX_IDLE         ; ... else return status = not receiving

BeginRx
    ; Seen start of Rx byte
    bcf     rxStat,RXERR    ; Clear the Rx error status flag
    movlw   RX_BITS         ; Load number of..
    movwf   rxBitCount      ; ... Rx bits to receive

#ifndef SYNC_SERIAL
    ; If not performing 'synchronous' reception
    movlw   (CYCRXINI + 1)  ; Load initial number of
    movwf   rxTimer         ; ... cycles for first Rx bit
#endif
    retlw   RX_BUSY         ; Return status = receiving data

ContinueRx
    ; Reception of data is in progress
#ifndef CLKD_SERIAL
    ; If not performing 'synchronous' reception test if time to sample
    ; next bit of received data
    decfsz  rxTimer,W       ; Skip if bit cycle count is zero ...
    retlw   RX_BUSY         ; ... else return status = receiving data
#endif
    ; Test if all data bits have been received
    decfsz  rxBitCount,F    ; Decrement Rx bit count, skip if zero ...
    goto    RxDataBit       ; ... else jump to read data bit

    bsf     rxStat,RXSTOP   ; Set seeking stop bit flag

SeekStopBit
    ; Sample the input expecting to see a stop bit
    btfsc   RXPORT,RXBIT    ; Test input for stop bit, skip if clear ...
    goto    Overrun         ; ... else jump (not received stop bit)

    bcf     rxStat,RXSTOP   ; Clear seeking stop bit flag

    ; Reception of a byte is complete, test to see it can be saved,
    ; i.e. previously received byte is no longer being used
    btfsc   rxStat,RXFLG    ; Skip if Rx buffer full flag is clear ...
    retlw   RX_IDLE         ; ... else return status = not receiving

    ; Rx buffer is not in use, the received byte can be saved but first
    ; check there were no errors during reception of the byte
    btfsc   rxStat,RXERR    ; Skip if the error flag is clear ...
    retlw   RX_IDLE         ; ... else return status = not receiving

    ; Reception of a byte has been completed without any error so store
    ; the received byte
    movf    rxReg,W         ; Copy the byte from the shift register ...
    movwf   rxBffr          ; ... into the Rx buffer
    bsf     rxStat,RXFLG    ; Set the Rx buffer full flag
    retlw   RX_IDLE         ; Return status = not receiving

Overrun
    bsf     rxStat,RXERR    ; No stop bit found, set the Rx error flag

    movf    rxReg,F         ; Test the shift register
    btfss   STATUS,Z        ; Skip if shift register zero ...
    goto    RxBrkBit        ; ... else keep looking for break in Rx data

    bsf     rxStat,RXBRK    ; Set the received break flag
    retlw   RX_BRK          ; Return status = receiving break

RxBrkBit
#ifndef CLKD_SERIAL
    ; If not performing 'synchronous' reception test if time to sample
    ; next bit of received data
    decfsz  rxTimer,W       ; Skip if bit cycle count is zero ...
    retlw   RX_BUSY         ; ... else return status = receiving data
#endif

RxDataBit
    ; Sample next bit of received data.  This is done by shifting the
    ; received data register one bit to the right, 'most significant bit'
    ; to 'least significant bit', using the carry flag to ensure the 'most
    ; significant bit' is clear after the shift.  The 'most significant
    ; bit' of the  received data register is then set to the complement of
    ; the input bit, i.e. Input high equals 0, input low equals 1 (RS232)
    bcf     STATUS,C        ; Clear the carry flag
    rrf     rxReg,F         ; Rotate carry flag into the shift register
    btfss   RXPORT,RXBIT    ; Test the Rx input bit, skip if set ...
    bsf     rxReg,MSB       ; ... else set MSB of Rx register

#ifndef SYNC_SERIAL
    ; If not performing 'synchronous' reception number of cycles until
    ; time to sample next bit of received data
    movlw   (CYCRXBIT + 1)  ; Reload number of ...
    movwf   rxTimer         ; ... cycles per Rx bit
#endif

    retlw   RX_BUSY         ; Return status = receiving data

    endm


;**********************************************************************
;                                                                     *
; Macro:     ServiceTx                                                *
;                                                                     *
;            Macro to perform serial interface transmission.          *
;                                                                     *
; Arguments: txStat     - Transmit status variable                    *
;            txTimer    - Transmit timer variable                     *
;            txBitCount - Transmit bits counter                       *
;            txReg      - Transmit data shift register                *
;            txBffr     - Transmit data buffer (loaded into txReg)    *
;            TXPORT     - Transmit port data register                 *
;            TXBIT      - Transmit port bit to use                    *
;     The next two are only used if Tx collision detection enabled    *
;            RXPORT     - Receive port data register                  *
;            RXBIT      - Receive port bit to use                     *
;            CYCTXBIT   - Number of cycles per bit                    *
;            TXFLG      - Transmit buffer clear status bit            *
;            TXBRK      - Send break status bit                       *
;                                                                     *
; Return   : W - TX_IDLE, not transmitting data                       *
;                TX_BUSY, transmitting data                           *
;                TX_CLSN, collision detected                          *
;                                                                     *
;**********************************************************************
ServiceTx   macro   txStat, txTimer, txBitCount, txReg, txBffr, TXPORT, TXBIT, RXPORT, RXBIT, CYCTXBIT, TXFLG, TXBRK

    local   CheckForTx, BeginTx, ContinueTx, TxNextBit, BreakTx, BeginBreak, ContinueBreak

    ; Check to see if currently sending a break
    btfsc   txStat,TXBRK    ; Test sending break flag, skip if clear ...
    goto    BreakTx         ; ... else jump (sending break)

    ; Check to see if currently transmitting a byte
    movf    txBitCount,F    ; Test Tx bit count
    btfss   STATUS,Z        ; Skip if zero (no Tx in progress) ...
    goto    ContinueTx      ; ... else continue transmission

CheckForTx
    ; Not transmitting, check to see if a byte is waiting to be sent
    btfsc   txStat,TXFLG   ; Skip if Tx byte clear flag not set ...
    retlw   TX_IDLE         ; ... else return status = not transmitting

BeginTx
    ; Start transmitting byte
    bsf     TXPORT,TXBIT    ; Set Tx output for start bit, 'space'

    movf    txBffr,W        ; Copy Tx byte into ...
    movwf   txReg           ; ... shift register
    bsf     txStat,TXFLG    ; Set Tx byte clear flag

    movlw   TX_BITS         ; Load number of ...
    movwf   txBitCount      ; ... Tx bits to send

#ifndef SYNC_SERIAL
    ; If not performing 'synchronous' transmission
    movlw   (CYCTXBIT + 1)  ; Load number of ...
    movwf   txTimer         ; ... cycles per Tx bit
#endif
    retlw   TX_BUSY         ; Return status = transmitting

ContinueTx
    ; Transmission of data is in progress
#ifndef CLKD_SERIAL
    ; If not performing 'synchronous' transmission test if time to send
    ; next bit of transmit data
    decfsz  txTimer,W       ; Skip if bit cycle count is zero ...
    retlw   TX_BUSY         ; ... else return status = transmitting
#endif
    ; Test if all bits have been transmitted
    decfsz  txBitCount,F    ; Decrement Tx bit count, skip if zero ...
    goto    TxNextBit       ; ... else transmit next bit

    ; Transmission complete, nothing else needs to be done
    goto    CheckForTx      ; Check if more data for transmission in Tx buffer

TxNextBit
#ifndef SYNC_SERIAL
    ; If not performing 'synchronous' transmission
    movlw   (CYCTXBIT + 1)  ; Reload number of ...
    movwf   txTimer         ; ... cycles per Tx bit
#endif
#ifndef SYNC_TXCD
    ; If not performing transmit 'collision detection'
    movlw   TX_BUSY         ; Set returned status = 'Tx in progress'
#else
    ; Performing transmit 'collision detection', compare current Rx and
    ; previous Tx bits to check for collisions
    clrw                    ; Clear W register, sets STATUS,Z
    btfsc   RXPORT,RXBIT    ; Test Rx input, skip if clear ...
    iorlw   1               ; ... load 1 into W register, clears STATUS,Z
    btfsc   TXPORT,TXBIT    ; Skip if Tx output (last bit sent) clear ...
    xorlw   1               ; ... else invert W register, and STATUS,Z
    movlw   TX_BUSY         ; Set returned status = transmitting
    btfss   STATUS,Z        ; Skip if Rx input and Tx output match ...
    movlw   TX_CLSN         ; ... else returned status = collision
#endif
    ; Send next bit of transmit data.  This is done by shifting the
    ; transmit data register one bit to the right, 'most significant bit'
    ; to 'least significant bit', using the carry flag to ensure the 'most
    ; significant bit' is set after the shift (ensures that after sending
    ; the eight bits of data stop bits are 1 (output low = RS232 'mark').
    ; The transmit output bit is set to the complement of the 'least
    ; significant bit' of the shift register (before the shift took
    ; place), i.e. 0 equals output high (RS232 'space'), 1 equals output
    ; low (RS232 'mark')
    bsf     STATUS,C        ; Rotate one into MSB of Tx shift ...
    rrf     txReg,F         ; ... register and LSB out to carry
    btfss   STATUS,C        ; If bit is a zero ...
    bsf     TXPORT,TXBIT    ; ... set Tx output, 'space'
    btfsc   STATUS,C        ; If bit is a one ...
    bcf     TXPORT,TXBIT    ; ... clear Tx output, 'mark'

    return                  ; W contains Tx status to return

BreakTx
    ; Check to see if currently sending break
    movf    txBitCount,F    ; Test Tx bit count
    btfss   STATUS,Z        ; Skip if zero (no break in progress) ...
    goto    ContinueBreak   ; ... else continue transmission of break

BeginBreak
    ; Start sending break
    bsf     TXPORT,TXBIT    ; Set Tx output, 'space'

#ifndef SYNC_SERIAL
    ; If not performing 'synchronous' transmission number of break
    ; cycles is number of break bits times cycles per bit
    movlw   (BRK_BITS * CYCTXBIT)
#else
    ; If performing 'synchronous' transmission number of break
    ; cycles is just number of break bits
    movlw   BRK_BITS
#endif
    movwf   txBitCount      ; Set number of cycles to send break 
    retlw   TX_BUSY         ; Return status = transmitting

ContinueBreak
    ; Test if break transmission complete
    decfsz  txBitCount,F    ; Decrement Tx bit count, skip if zero ...
    retlw   TX_BUSY         ; ... else return status = transmitting

    ; Transmission complete, nothing else needs to be done
    bcf     txStat,TXBRK    ; Clear the send break flag
    clrf    txReg           ; Clear Tx data shift register
    comf    txReg,F         ; Ones complement Tx data shift register (send 1s)
    movlw   BRKSTP_BITS     ; Load number of ...
    movwf   txBitCount      ; ... break stop bits to send

    goto    TxNextBit       ; Commence sending Tx break stop bits

    endm


;**********************************************************************
;                                                                     *
; Macro:     SerialRx                                                 *
;                                                                     *
;            Attempt to get an 'Rx' byte from the serial interface.   *
;                                                                     *
; Arguments: rxStat - Receive status variable                         *
;            rxBffr - Received data buffer (loaded from rxReg)        *
;            RXFLG  - Receive buffer full status bit                  *
;                                                                     *
; Returns  : STATUS,Z - Set if got byte, clear if not                 *
;            W        - Rx byte, if any                               *
;                                                                     *
;**********************************************************************
SerialRx    macro   rxStat, rxBffr, RXFLG

    bcf     STATUS,Z        ; Set default return status of no Rx data

    btfss   rxStat,RXFLG    ; Skip if Rx buffer full flag is set ...
    return                  ; ... else return (receive buffer empty)

    movf    rxBffr,W        ; Load W from the Rx buffer
    bcf     rxStat,RXFLG    ; Clear the Rx buffer full flag
    bsf     STATUS,Z        ; Return status = 'Rx data available in W'
    return                  ; Return

    endm


;**********************************************************************
;                                                                     *
; Macro:     SrlRxInd                                                 *
;                                                                     *
;            Attempt to get an 'Rx' byte from the serial interface.   *
;                                                                     *
; Arguments: FSR    - Address for recieved byte                       *
;            rxStat - Receive status variable                         *
;            rxBffr - Received data buffer (loaded from rxReg)        *
;            RXFLG  - Receive buffer full status bit                  *
;                                                                     *
; Returns  : STATUS,Z - Set if got byte, clear if not                 *
;            @FSR     - Rx byte, if any                               *
;                                                                     *
;**********************************************************************
SrlRxInd    macro   rxStat, rxBffr, RXFLG

    bcf     STATUS,Z        ; Set default return status of no Rx data

    btfss   rxStat,RXFLG    ; Skip if Rx buffer full flag is set ...
    return                  ; ... else return (receive buffer empty)

    movf    rxBffr,W        ; Load W from the Rx buffer
    movwf   INDF            ; Save byte from the Rx buffer
    bcf     rxStat,RXFLG    ; Clear the Rx buffer full flag
    bsf     STATUS,Z        ; Return status = 'Rx data saved'
    return                  ; Return

    endm


;**********************************************************************
;                                                                     *
; Macro:     SerialTx                                                 *
;                                                                     *
;            Attempt to send a 'Tx' byte to the serial interface.     *
;                                                                     *
; Arguments: FSR    - Byte to send                                    *
;            txStat - Transmit status variable                        *
;            txBffr - Transmit data buffer                            *
;            TXFLG  - Transmit buffer clear status bit                *
;                                                                     *
; Returns  : STATUS,Z - Set if sent byte, clear if not                *
;                                                                     *
;**********************************************************************
SerialTx    macro   txStat, txBffr, TXFLG

    bcf     STATUS,Z        ; Set default return status of no Tx performed
    btfss   txStat,TXFLG    ; Skip if Tx buffer clear flag is set ...
    return                  ; ... else return (transmit buffer full)

    movf    FSR,W           ; Copy Tx byte into ...
    movwf   txBffr          ; ... Tx buffer
    bcf     txStat,TXFLG    ; Clear the Tx byte clear flag
    bsf     STATUS,Z        ; Return status = 'Tx data sent'
    return                  ; Return

    endm


;**********************************************************************
;                                                                     *
; Macro:     SrlTxInd                                                 *
;                                                                     *
;            Attempt to send a 'Tx' byte to the serial interface.     *
;                                                                     *
; Arguments: FSR    - Address of byte to send                         *
;            txStat - Transmit status variable                        *
;            txBffr - Transmit data buffer                            *
;            TXFLG  - Transmit buffer clear status bit                *
;                                                                     *
; Returns  : STATUS,Z - Set if sent byte, clear if not                *
;                                                                     *
;**********************************************************************
SrlTxInd    macro   txStat, txBffr, TXFLG

    bcf     STATUS,Z        ; Set default return status of no Tx performed
    btfss   txStat,TXFLG    ; Skip if Tx buffer clear flag is set ...
    return                  ; ... else return (transmit buffer full)

    movf    INDF,W          ; Copy Tx byte into ...
    movwf   txBffr          ; ... Tx buffer
    bcf     txStat,TXFLG    ; Clear the Tx byte clear flag
    bsf     STATUS,Z        ; Return status = 'Tx data sent'
    return                  ; Return

    endm


;**********************************************************************
;                                                                     *
; Macro:     IsTxIdle                                                 *
;                                                                     *
;            Test if no transmission in progress.                     *
;                                                                     *
; Arguments: txBitCount - Transmit bits counter                       *
;                                                                     *
; Returns  : STATUS,Z - Set if idle, clear if busy                    *
;                                                                     *
;**********************************************************************
IsTxIdle    macro   txBitCount

    movf    txBitCount,F

    endm


    list

