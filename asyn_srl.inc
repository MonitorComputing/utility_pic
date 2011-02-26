        nolist
; $RCSfile$ $Revision$
; $Date$

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
;    SerInit   - Initialise serial interface.                         *
;    EnableRx  - Enable receive hardware.                             *
;    EnableTx  - Enable transmit hardware.                            *
;    InitRx    - Initialise receivesoftware.                          *
;    InitTx    - Initialise transmit software.                        *
;                                                                     *
;  These macros employ 'returns' and so their instances must be       *
;  invoked as subroutine calls:                                       *
;                                                                     *
;    ServiceRx - Service actual receptions of data.                   *
;    ServiceTx - Service actual transmission of data.                 *
;    BreakTx   - Transmit a 'break' signal.                           *
;    SerialRx  - Get most recently received byte, if available.       *
;    SerialTx  - Start transmission of byte, if transmitter not busy. *
;                                                                     *
; Each instance of a serial interface requires -                      *
;                                                                     *
; Variables:                                                          *
;     Receive:                                                        *
;            rxTimer     - Receive timer variable                     *
;            rxPort      - Receive port data register                 *
;            rxBitCount  - Receive bits counter                       *
;            rxStat      - Receive status variable                    *
;            rxReg       - Receive data shift register                *
;            rxByte      - Receive data byte buffer                   *
;     Transmit:                                                       *
;            txTimer     - Transmit timer variable                    *
;            txPort      - Transmit port data register                *
;            txBitCount  - Transmit bits counter                      *
;            txStat      - Transmit status variable                   *
;            txReg       - Transmit data shift register               *
;            txByte      - Transmit data byte buffer                  *
;                                                                     *
; Constants:                                                          *
;     Receive:                                                        *
;            RXBIT       - Receive port bit to use                    *
;            CYCRXINI    - Number of cycles per bit for first bit     *
;            CYCRXBIT    - Number of cycles per bit                   *
;            RXFLAG      - Receive byte buffer 'full' status bit      *
;            RXERR       - Receive error status bit                   *
;            RXBREAK     - Received 'break' status bit                *
;            RXSTOP      - Seeking stop bit                           *
;     Transmit:                                                       *
;            TXBIT       - Transmit port bit to use                   *
;            CYCTXBIT    - Number of cycles per bit                   *
;            TXFLAG      - Transmit byte buffer 'clear' status bit    *
;                                                                     *
; Flags:     SYNC_SERIAL - 'Synchronous mode', no bit timing needed   *
;            TXCD        - 'Tx collison detect', check last Tx bit    *
;                                                                     *
; Notes: If the macro 'SerialTx' is employed and will be interrupted  *
;        the interrupt code must preserve the contents of the FSR     *
;        register.                                                    *
;        There are only three status bits so a single status variable *
;        can be used for both rx and tx (rxStat == txStat).  Also two *
;        interfaces can share the same status variable (more if 'half *
;        duplex', see below).                                         *
;        If an interface is running 'half duplex' the following       *
;        constants and variables can be equated in order to reduce    *
;        memory use,                                                  *
;                                                                     *
;            RXFLAG     == TXFLAG                                     *
;            rxTimer    == txTimer                                    *
;            rxBitCount == txBitCount                                 *
;            rxReg      == txReg                                      *
;            rxByte     == txByte                                     *
;                                                                     *
;        If an interface is operating in synchronous mode,            *
;        i.e. SYNC_SERIAL defined when macros instanced, then the     *
;        following will not be used and dummy values can be passed    *
;        as macro arguments; CYCRXINI, CYCRXBIT, CYCTXBIT, rxTimer,   *
;        txTimer.                                                     *
;                                                                     *
;        The initialise and enable macros differ from the rest in     *
;        that they do not incorporate a return from subroutine call.  *
;        This is to allow for the addition of custom code before and  *
;        / or after their invocations.                                *
;                                                                     *
;**********************************************************************


;**********************************************************************
; Constants                                                           *
;**********************************************************************

RX_BITS     EQU  9 ; 1 start (not counted), 8 data, no parity, 1 stop.
TX_BITS     EQU 10 ; 1 start, 8 data, no parity, 1 stop (then Tx extra zero).
MSB         EQU  7 ; Bit number of 'most significant bit'.

RX_IDLE     EQU  0 ; Returned to indicate no data being received.
RX_BUSY     EQU  1 ; Returned to indicate data being received.
RX_BRK      EQU  2 ; Returned to indicate 'break' has been received.

TX_IDLE     EQU  0 ; Returned to indicate no data being transmitted.
TX_BUSY     EQU  1 ; Returned to indicate data being transmitted.
TX_CLSN     EQU  3 ; Returned to indicate detection of Tx collision.


;**********************************************************************
;                                                                     *
; Macro:     SerInit                                                  *
;                                                                     *
;            Macro to initialise serial interface variables.          *
;                                                                     *
; Arguments: serStat    - Serial interface status variable            *
;            txTimer    - Transmit timer variable                     *
;            txReg      - Transmit data shift register                *
;            txByte     - Transmit data byte buffer                   *
;            txBitCount - Transmitted bits counter                    *
;            rxTimer    - Receive timer variable                      *
;            rxReg      - Receive data shift register                 *
;            rxByte     - Receive data byte buffer                    *
;            rxBitCount - Received bits counter                       *
;                                                                     *
;**********************************************************************
SerInit     macro   serStat, txTimer, txReg, txByte, txBitCount, rxTimer, rxReg, rxByte, rxBitCount

        clrf    serStat
#ifndef SYNC_SERIAL
        clrf    txTimer
#endif
        clrf    txReg
        clrf    txByte
        clrf    txBitCount
#ifndef SYNC_SERIAL
        clrf    rxTimer
#endif
        clrf    rxReg
        clrf    rxByte
        clrf    rxBitCount

        endm


;**********************************************************************
;                                                                     *
; Macro:     EnableRx                                                 *
;                                                                     *
;            Macro to enable serial recieve physical interface.       *
;                                                                     *
; Arguments: RXTRIS - Recieve port direction register                 *
;            rxPort - Recieve port data register                      *
;            RXBIT  - Recieve port bit to use                         *
;                                                                     *
;**********************************************************************
EnableRx    macro   RXTRIS, rxPort, RXBIT

        BANKSEL OPTION_REG
        bsf     RXTRIS,RXBIT    ; Set direction of Rx bit as input.
        BANKSEL TMR0
        bsf     rxPort,RXBIT    ; Set Rx bit (in case using Port A).

        endm


;**********************************************************************
;                                                                     *
; Macro:     EnableTx                                                 *
;                                                                     *
;            Macro to enable serial transmit physical interface.      *
;                                                                     *
; Arguments: TXTRIS - Transmit port direction register                *
;            txPort - Transmit port data register                     *
;            TXBIT  - Transmit port bit to use                        *
;                                                                     *
;**********************************************************************
EnableTx    macro   TXTRIS, txPort, TXBIT

        bcf     txPort,TXBIT    ; Clear Tx bit.
        BANKSEL OPTION_REG
        bcf     TXTRIS,TXBIT    ; Set direction of Tx bit as output.
        BANKSEL TMR0

        endm


;**********************************************************************
;                                                                     *
; Macro:     InitRx                                                   *
;                                                                     *
;            Macro to initialise serial interface recieve variables.  *
;                                                                     *
; Arguments: rxTimer - Recieve timer variable                         *
;            rxStat  - Serial interface recieve status variable       *
;            RXFLAG  - Recieve byte buffer 'loaded' status bit        *
;            RXERR   - Recieve error status bit                       *
;            RXBREAK - Received 'break' status bit                    *
;                                                                     *
;**********************************************************************
InitRx      macro   rxTimer, rxStat, RXFLAG, RXERR, RXBREAK, RXSTOP

#ifndef SYNC_SERIAL
        clrf    rxTimer
#endif
        bcf     rxStat,RXFLAG   ; Clear Rx byte 'loaded' flag.
        bcf     rxStat,RXERR    ; Clear the Rx error flag.
        bcf     rxStat,RXBREAK  ; Clear the received 'break' flag.
        bcf     rxStat,RXSTOP   ; Seeking stop bit

        endm


;**********************************************************************
;                                                                     *
; Macro:     InitTx                                                   *
;                                                                     *
;            Macro to initialise serial interface transmit variables. *
;                                                                     *
; Arguments: txTimer - Transmit timer variable                        *
;            txStat  - Serial interface transmit status variable      *
;            TXFLAG  - Transmit byte buffer 'clear' status bit        *
;                                                                     *
;**********************************************************************
InitTx      macro   txTimer, txStat, TXFLAG

#ifndef SYNC_SERIAL
        clrf    txTimer
#endif
        bsf     txStat,TXFLAG   ; Set Tx byte 'clear' flag.

        endm


;**********************************************************************
;                                                                     *
; Macro:     ServiceRx                                                *
;                                                                     *
;            Macro to service serial interface reception.             *
;                                                                     *
; Arguments: rxTimer    - Receive timer variable                      *
;            rxPort     - Receive port data register                  *
;            RXBIT      - Receive port bit to use                     *
;            rxBitCount - Receive bits counter                        *
;            CYCRXINI   - Number of cycles per bit for first bit      *
;            rxStat     - Receive status variable                     *
;            RXERR      - Receive error status bit                    *
;            RXBREAK    - Received 'break' status bit                 *
;            rxReg      - Receive data shift register                 *
;            rxByte     - Receive data byte buffer                    *
;            RXFLAG     - Receive byte buffer 'loaded' status bit     *
;            CYCRXBIT   - Number of cycles per bit                    *
;                                                                     *
; Return   : W - RX_IDLE, not currently receiving any data.           *
;                RX_BUSY, receiving data.                             *
;                                                                     *
;**********************************************************************
ServiceRx   macro   rxTimer, rxPort, RXBIT, rxBitCount, CYCRXINI, rxStat, RXERR, RXBREAK, RXSTOP, rxReg, rxByte, RXFLAG, CYCRXBIT

        local   BeginRx, ContinueRx, SeekStopBit, EndRx, StoreRx, RxDataBit

        ; Check to see if currently seeking stop bit.
        btfsc   rxStat,RXSTOP   ; Test the seeking stop flag, skip if clear ...
        goto    SeekStopBit     ; ... else jump (seeking stop bit).

        ; Check to see if currently receiving a byte.
        movf    rxBitCount,F    ; Test Rx bit count.
        btfss   STATUS,Z        ; Skip if zero (no Rx in progress) ...
        goto    ContinueRx      ; ... else jump (Rx in progress).

        ; Not receiving, look for start of Rx byte.
        btfss   rxPort,RXBIT    ; Skip if start bit detected ...
        retlw   RX_IDLE         ; ... else return.

        ; Seen start of Rx byte.
        bcf     rxStat,RXBREAK  ; Clear the received 'break' status flag.
        movlw   RX_BITS         ; Load number of...
        movwf   rxBitCount      ; ... Rx bits to receive.

#ifndef SYNC_SERIAL
        ; If not performing 'synchronous' reception.
        movlw   CYCRXINI        ; Load initial number of
        movwf   rxTimer         ; ... cycles for first Rx bit.
#endif
        retlw   RX_BUSY

ContinueRx
        ; Reception of data is in progress.
#ifndef SYNC_SERIAL
        ; If not performing 'synchronous' reception test if time to sample
        ; next bit of received data.
        decfsz  rxTimer,F       ; Decrement bit cycle count, skip if zero ...
        retlw   RX_BUSY         ; ... else return.
#endif
        ; Test if all data bits have been received.
        decfsz  rxBitCount,F    ; Decrement Rx bit count, skip if zero ...
        goto    RxDataBit       ; ... else jump to sample data bit.

        bsf     rxStat,RXSTOP   ; Set seeking stop bit flag.

SeekStopBit
        ; Sample the input expecting to see a stop bit.
        btfss   rxPort,RXBIT    ; Test input for 'stop' bit, skip if set ...
        goto    EndRx           ; ... else jump (received the stop bit).

        ; No stop bit found, it might be that a 'break' is being transmitted
        ; or something has caused lose of synchronisation with transmitter.
        btfsc   rxStat,RXERR    ; Test the Rx error flag, skip if clear ...
        bsf     rxStat,RXBREAK  ; ... else must be receiving a 'break'.

        bsf     rxStat,RXERR    ; Set the Rx error flag.

        btfsc   rxStat,RXBREAK  ; Skip if the 'break' flag is not set ...
        retlw   RX_BRK          ; ... else return 'receiving break' ...
        retlw   RX_IDLE         ; ... else just return 'receive idle'.

EndRx
        ; Stop bit seen.
        bcf     rxStat,RXSTOP   ; Clear seeking stop bit flag.

        ; Reception of a byte is complete, test to see it can be saved,
        ; i.e. previously received byte is no longer being used.
        btfsc   rxStat,RXFLAG   ; Skip if Rx buffer 'full' flag is clear ...
        retlw   RX_BUSY         ; ... else return.

        ; Rx buffer is not in use, the received byte can be saved but first
        ; check there were no errors during reception of the byte.
        btfss   rxStat,RXERR    ; Skip if the error flag is set ...
        goto    StoreRx         ; ... else jump.

        ; Reception of a byte has been completed but there was an error.  Clear
        ; the error condition but throw away the received byte.
        bcf     rxStat,RXERR    ; Clear error flag.
        retlw   RX_BUSY         ; Return (received byte ignored).

StoreRx
        ; Reception of a byte has been completed without any error so store
        ; the received byte.
        movf    rxReg,W         ; Copy the byte from the shift register ...
        movwf   rxByte          ; ... into the Rx buffer.
        bsf     rxStat,RXFLAG   ; Set the Rx buffer 'full' flag.
        retlw   RX_BUSY         ; Return.

RxDataBit
        ; Sample next bit of received data.  This is done by shifting the
        ; received data register one bit to the right, 'most significant bit'
        ; to 'least significant bit', using the carry flag to ensure the 'most
        ; significant bit' is clear after the shift.  The 'most significant
        ; bit' of the  received data register is then set to the complement of
        ; the input bit, i.e. Input high equals 0, input low equals 1 (RS232).
        bcf     STATUS,C        ; Clear the carry flag.
        rrf     rxReg,F         ; Rotate carry flag into the shift register.
        btfss   rxPort,RXBIT    ; Test the Rx input bit, skip if set ...
        bsf     rxReg,MSB       ; ... else set MSB of Rx register.

#ifndef SYNC_SERIAL
        ; If not performing 'synchronous' reception number of cycles until
        ; time to sample next bit of received data.
        movlw   CYCRXBIT        ; Reload number of ...
        movwf   rxTimer         ; ... cycles per Rx bit.
#endif
        retlw   RX_BUSY         ; Return.

        endm


;**********************************************************************
;                                                                     *
; Macro:     ServiceTx                                                *
;                                                                     *
;            Macro to perform serial interface transmission.          *
;                                                                     *
; Arguments: txTimer    - Transmit timer variable                     *
;            txStat     - Transmit status variable                    *
;            txByte     - Transmit data byte buffer                   *
;            txReg      - Transmit data shift register                *
;            TXFLAG     - Transmit byte buffer 'clear' status bit     *
;            txBitCount - Transmit bits counter                       *
;            CYCTXBIT   - Number of cycles per bit                    *
;            txPort     - Transmit port data register                 *
;            TXBIT      - Transmit port bit to use                    *
;     The next two are only used if Tx collision detection enabled    *
;            rxPort     - Receive port data register                  *
;            RXBIT      - Receive port bit to use                     *
;                                                                     *
; Return   : W - TX_IDLE, not currently transmitting any data         *
;                TX_BUSY, transmitting data                           *
;                                                                     *
;**********************************************************************
ServiceTx   macro   txTimer, txStat, txByte, txReg, TXFLAG, txBitCount, CYCTXBIT, txPort, TXBIT, rxPort, RXBIT

        local   ContinueTx, TxNextBit, TxOff

        ; Check to see if currently transmitting a byte
        movf    txBitCount,F    ; Test Tx bit count
        btfss   STATUS,Z        ; Skip if zero (no Tx in progress) ...
        goto    ContinueTx      ; ... else jump (Tx in progress).

        ; Not transmitting, check to see if a byte is waiting to be sent.
        btfsc   txStat,TXFLAG   ; Test Tx byte 'clear' flag skip if not set ...
        retlw   TX_IDLE         ; ... else return (nothing to transmit)

        ; Start transmitting byte
        bsf     txPort,TXBIT    ; Set Tx output for start bit.

        movf    txByte,W        ; Copy Tx byte into ...
        movwf   txReg           ; ... shift register.
        bsf     txStat,TXFLAG   ; Set Tx byte 'clear' flag.

        movlw   TX_BITS         ; Load number of...
        movwf   txBitCount      ; ... Tx bits to send.

#ifndef SYNC_SERIAL
        ; If not performing 'synchronous' transmission.
        movlw   CYCTXBIT        ; Load number of ...
        movwf   txTimer         ; ... cycles per Tx bit
#endif
        retlw   TX_BUSY         ; Return.

ContinueTx
        ; Transmission of data is in progress.
#ifndef SYNC_SERIAL
        ; If not performing 'synchronous' transmission test if time to send
        ; next bit of transmit data.
        decfsz  txTimer,F       ; Decrement bit cycle count, skip if zero ...
        retlw   TX_BUSY         ; ... else return.
#endif
        ; Test if all bits have been transmitted.
        decfsz  txBitCount,F    ; Decrement Tx bit count, skip if zero ...
        goto    TxNextBit       ; ... else jump.

        ; Transmission complete, nothing else needs to be done.
        retlw   TX_BUSY

TxNextBit
#ifndef SYNC_SERIAL
        ; If not performing 'synchronous' transmission.
        movlw   CYCTXBIT        ; Reload number of ...
        movwf   txTimer         ; ... cycles per Tx bit.
#endif
#ifndef SYNC_TXCD
        ; If not performing transmit 'collision detection'.
        movlw   TX_BUSY         ; Set returned status = 'Tx in progress'
#else
        ; Performing transmit 'collision detection', compare current Rx and
        ; previous Tx bits to check for collisions.
        clrw                    ; Clear W register, sets STATUS,Z.
        btfsc   rxPort,RXBIT    ; Test Rx input, skip if clear ...
        iorlw   1               ; ... load 1 into W register, clears STATUS,Z.
        btfsc   txPort,TXBIT    ; Skip if Tx output (last bit sent) clear ...
        xorlw   1               ; ... else invert W register, and STATUS,Z.
        movlw   TX_BUSY         ; Set returned status = 'Tx in progress'.
        btfss   STATUS,Z        ; Skip if Rx input and Tx output match ...
        movlw   TX_CLSN         ; ... else returned status = 'Tx collision'.
#endif
        ; Send next bit of received data.  This is done by shifting the
        ; transmit data register one bit to the right, 'most significant bit'
        ; to 'least significant bit', using the carry flag to ensure the 'most
        ; significant bit' is set after the shift (ensures that after sending
        ; the eight bits of data extra, stop, bits are 1).  The transmit
        ; output bit is set to the complement of the 'least significant bit'
        ; of the shift register (before the shift took place), i.e. 0 equals
        ;output high, 1 equals output low (RS232).
        bsf     STATUS,C        ; Rotate one into MSB of Tx shift ...
        rrf     txReg,F         ; ... register and LSB out to carry.
        btfss   STATUS,C        ; If bit is a zero ...
        bsf     txPort,TXBIT    ; ... set Tx output.
        btfsc   STATUS,C        ; If bit is a one ...
        bcf     txPort,TXBIT    ; ... clear Tx output.

        return                  ; W contains Tx status to return.

        endm


;**********************************************************************
;                                                                     *
; Macro:     BreakTx                                                  *
;                                                                     *
;            Macro to send 'break' on serial interface.               *
;                                                                     *
; Arguments: txTimer    - Transmit timer variable                     *
;            txBitCount - Transmit bits counter                       *
;            CYCTXBIT   - Number of cycles per bit                    *
;            txPort     - Transmit port data register                 *
;            TXBIT      - Transmit port bit to use                    *
;                                                                     *
; Return   : W - TX_IDLE, 'break' sent                                *
;                TX_BUSY, sending break                               *
;                                                                     *
;**********************************************************************
BreakTx     macro   txTimer, txBitCount, CYCTXBIT, txPort, TXBIT

        local   ContinueBreak, ReloadBitTimer

        ; Check to see if currently sending 'break'
        movf    txBitCount,F    ; Test Tx bit count
        btfss   STATUS,Z        ; Skip if zero (no 'break' in progress) ...
        goto    ContinueTx      ; ... else jump ('break' in progress).

        ; Start sending 'break'
        bsf     txPort,TXBIT    ; Set Tx output bit.

        movlw   TX_BITS         ; Load number of...
        movwf   txBitCount      ; ... Tx bits to send ...
        incf    txBitCount,F    ; ... plus 1.

        movlw   CYCTXBIT        ; Load number of ...
        movwf   txTimer         ; ... cycless per Tx bit.

        retlw   TX_BUSY         ; Return.

ContinueBreak
        ; Sending of 'break' is in progress, test if time to send next bit.
        decfsz  txTimer,F       ; Decrement bit cycle count, skip if zero ...
        retlw   TX_BUSY         ; ... else return.

        ; Test if all bits have been sent.
        decfsz  txBitCount,F    ; Decrement Tx bit count, skip if zero ...
        goto    ReloadBitTimer  ; ... else jump.

        ; Transmission complete, nothing else needs to be done.
        bcf     txPort,TXBIT    ; Clear Tx output bit.
        retlw   TX_IDLE         ; Return.

ReloadBitTimer
        movlw   CYCTXBIT        ; Reload number of ...
        movwf   txTimer         ; ... cycless per Tx bit.

        retlw   TX_BUSY         ; Return.

        endm


;**********************************************************************
;                                                                     *
; Macro:     SerialRx                                                 *
;                                                                     *
;            Attempt to get an 'Rx' byte from the serial interface.   *
;                                                                     *
; Arguments: rxStat - Receive status variable                         *
;            RXFLAG - Receive byte buffer 'loaded' status bit         *
;            rxByte - Receive data byte buffer                        *
;                                                                     *
; Returns  : STATUS,Z - Set if got byte, clear if not                 *
;            W        - Rx byte, if any                               *
;                                                                     *
;**********************************************************************
SerialRx        macro   rxStat, RXFLAG, rxByte

        bcf     STATUS,Z        ; Set default return status of no Rx data.

        btfss   rxStat,RXFLAG   ; Skip if Rx buffer 'loaded' flag is set ...
        return                  ; ... else return (receive buffer empty).

        movf    rxByte,W        ; Load W from the Rx buffer.
        bcf     rxStat,RXFLAG   ; Clear the Rx buffer 'loaded' flag.
        bsf     STATUS,Z        ; Return status = 'Rx data available in W'.
        return                  ; Return.

        endm


;**********************************************************************
;                                                                     *
; Macro:     SerialTx                                                 *
;                                                                     *
;            Attempt to send a 'Tx' byte to the serial interface.     *
;                                                                     *
; Arguments: FSR    - Byte to send                                    *
;            txStat - Transmit status variable                        *
;            TXFLAG - Transmit byte buffer 'clear' status bit         *
;            txByte - Transmit data byte buffer                       *
;                                                                     *
; Returns  : STATUS,Z - Set if sent byte, clear if not                *
;                                                                     *
;**********************************************************************
SerialTx        macro   txStat, TXFLAG, txByte

        bcf     STATUS,Z        ; Set default return status of no Tx performed.
        btfss   txStat,TXFLAG   ; Skip if Tx buffer 'clear' flag is set ...
                return          ; ... else return (transmit buffer full).

        movf    FSR,W           ; Copy Tx byte into ...
        movwf   txByte          ; ... Tx buffer
        bcf     txStat,TXFLAG   ; Clear the Tx byte 'clear' flag.
        bsf     STATUS,Z        ; Return status = 'Tx data sent'.
        return                  ; Return.

        endm


        list

