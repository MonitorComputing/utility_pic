		nolist
;**********************************************************************
;                                                                     *
;    Filename:	    asyn_srl.inc                                      *
;    Date:          04 Apr 2001                                       *
;    File Version:  4                                                 *
;                                                                     *
;    Author:        Chris White (whitecf@bcs.org.uk)                  *
;    Company:       Monitor Computing Services Ltd.                   *
;                                                                     *
;**********************************************************************
;                                                                     *
;    Copyright (C) 2001  Monitor Computing Services Ltd.              *
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
; Description: Macros to implement asynchronous (or synchronous if    *
;              SYNC_SERIAL is defined), byte orientated (1 start,     *
;              8 data, no parity, 1 stop) serial interface            *
;                                                                     *
; Each instance of a serial interface requires -                      *
;                                                                     *
; Constants: RXBIT       - Receive port bit to use                    *
;            CYCRXINI    - Number of cycles per bit for first bit     *
;            CYCRXBIT    - Number of cycles per bit                   *
;            RXFLAG      - Receive byte buffer 'loaded' status bit    *
;            RXERR       - Receive error status bit                   *
;            RXBREAK     - Received 'break' status bit                *
;            TXBIT       - Transmit port bit to use                   *
;            CYCTXBIT    - Number of cycles per bit                   *
;            TXFLAG      - Transmit byte buffer 'clear' status bit    *
;                                                                     *
; Variables: rxTimer     - Receive timer variable                     *
;            rxPort      - Receive port data register                 *
;            rxBitCount  - Receive bits counter                       *
;            rxStat      - Receive status variable                    *
;            rxReg       - Receive data shift register                *
;            rxByte      - Receive data byte buffer                   *
;            txTimer     - Transmit timer variable                    *
;            txPort      - Transmit port data register                *
;            txBitCount  - Transmit bits counter                      *
;            txStat      - Transmit status variable                   *
;            txReg       - Transmit data shift register               *
;            txByte      - Transmit data byte buffer                  *
;                                                                     *
; Flags    : SYNC_SERIAL - 'Synchronous mode', no bit timing needed   *
;            SYNC_TXCD   - 'Tx collison detect', check last Tx bit    *
;                                                                     *
; Notes: If the macro 'SerialTx' is employed and can be interrupted   *
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
;        that they do not incorporate a return.  This is to allow for *
;        the addition of custom code before and / or after their      *
;        invocations.                                                 *
;                                                                     *
;**********************************************************************

RX_BITS		EQU      9 ; 1 start (not counted), 8 data, no parity, 1 stop
RX_IDLE		EQU      0
RX_BUSY		EQU      1
RX_BRK		EQU      2
TX_BITS		EQU     10 ; 1 start, 8 data, no parity, 1 stop (tx an extra zero)
TX_IDLE		EQU      0
TX_BUSY		EQU      1
TX_CLSN		EQU      3

MSB		EQU      7


;**********************************************************************
;                                                                     *
; Macro:     SerInit                                                  *
;                                                                     *
;            Macro to initialise serial interface variables           *
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
SerInit		macro   serStat, txTimer, txReg, txByte, txBitCount, rxTimer, rxReg, rxByte, rxBitCount

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
;            Macro to enable serial recieve physical interface        *
;                                                                     *
; Arguments: RXTRIS - Recieve port direction register                 *
;            rxPort - Recieve port data register                      *
;            RXBIT  - Recieve port bit to use                         *
;                                                                     *
;**********************************************************************
EnableRx	macro   RXTRIS, rxPort, RXBIT

		BANKSEL OPTION_REG
		bsf     RXTRIS,RXBIT      ; Set direction of Rx bit
		BANKSEL TMR0
		bsf     rxPort,RXBIT      ; Set Rx bit (in case using Port A)

		endm


;**********************************************************************
;                                                                     *
; Macro:     EnableTx                                                 *
;                                                                     *
;            Macro to enable serial transmit physical interface       *
;                                                                     *
; Arguments: TXTRIS - Transmit port direction register                *
;            txPort - Transmit port data register                     *
;            TXBIT  - Transmit port bit to use                        *
;                                                                     *
;**********************************************************************
EnableTx	macro   TXTRIS, txPort, TXBIT

		bcf     txPort,TXBIT      ; Clear Tx bit
		BANKSEL OPTION_REG
		bcf     TXTRIS,TXBIT      ; Set direction of Tx bit
		BANKSEL TMR0

		endm


;**********************************************************************
;                                                                     *
; Macro:     InitRx                                                   *
;                                                                     *
;            Macro to initialise serial interface recieve variables   *
;                                                                     *
; Arguments: rxTimer - Recieve timer variable                         *
;            rxStat  - Serial interface recieve status variable       *
;            RXFLAG  - Recieve byte buffer 'loaded' status bit        *
;            RXERR   - Recieve error status bit                       *
;            RXBREAK - Received 'break' status bit                    *
;                                                                     *
;**********************************************************************
InitRx		macro   rxTimer, rxStat, RXFLAG, RXERR, RXBREAK

#ifndef SYNC_SERIAL
		clrf    rxTimer
#endif
		bcf     rxStat,RXFLAG     ; Clear Rx byte 'loaded' flag
		bcf     rxStat,RXERR      ; Clear the Rx error flag
		bcf     rxStat,RXBREAK    ; Clear the received 'break' flag

		endm


;**********************************************************************
;                                                                     *
; Macro:     InitTx                                                   *
;                                                                     *
;            Macro to initialise serial interface transmit variables  *
;                                                                     *
; Arguments: txTimer - Transmit timer variable                        *
;            txStat  - Serial interface transmit status variable      *
;            TXFLAG  - Transmit byte buffer 'clear' status bit        *
;                                                                     *
;**********************************************************************
InitTx		macro   txTimer, txStat, TXFLAG

#ifndef SYNC_SERIAL
		clrf    txTimer
#endif
		bsf     txStat,TXFLAG     ; Set Tx byte 'clear' flag

		endm


;**********************************************************************
;                                                                     *
; Macro:     ServiceRx                                                *
;                                                                     *
;            Macro to service serial interface reception              *
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
; Return   : W - 0 Rx idle, not currently receiving any data          *
;                1 Rx busy, receiving data                            *
;                                                                     *
;**********************************************************************
ServiceRx	macro   rxTimer, rxPort, RXBIT, rxBitCount, CYCRXINI, rxStat, RXERR, RXBREAK, rxReg, rxByte, RXFLAG, CYCRXBIT

		local   BeginRx, ContinueRx, EndRx, StoreRx, RxDataBit

		; Check to see if currently receiving a byte
		movf    rxBitCount,F      ; Test Rx bit count
		btfss   STATUS,Z          ; Skip if zero (no Rx in progress) ...
		goto    ContinueRx        ; ... otherwise jump

		; Looking for start of Rx byte
		btfss   rxPort,RXBIT      ; Skip if start bit detected ...
		retlw   RX_IDLE           ; ... otherwise return

		; Seen start of Rx byte
		bcf     rxStat,RXBREAK    ; Clear the received 'break' status flag
		movlw   RX_BITS           ; Load number of...
		movwf   rxBitCount        ; ... Rx bits to receive

#ifndef SYNC_SERIAL
		movlw   CYCRXINI          ; Load initial number of
		movwf   rxTimer           ; ... cycles for first Rx bit
#endif
                retlw   RX_BUSY

ContinueRx
#ifndef SYNC_SERIAL
		decfsz  rxTimer,F         ; Decrement Rx bit cycle count, skip if zero ...
                retlw   RX_BUSY           ; ... otherwise return
#endif
		; Receive next bit
		decfsz  rxBitCount,F      ; Decrement Rx bit count, skip if zero ...
		goto    RxDataBit         ; ... otherwise jump

		; Looking for stop bit of Rx byte
		btfss   rxPort,RXBIT      ; Test for stop bit, skip if set ...
		goto    EndRx             ; ... otherwise jump (received the stop bit)

		; Stop bit of Rx byte missing
		btfsc   rxStat,RXERR      ; Test the Rx error flag, skip if clear ...
		bsf     rxStat,RXBREAK    ; ... otherwise must be receiving a 'break' sequence

		bsf     rxStat,RXERR      ; Set the Rx error flag
		incf    rxBitCount,F      ; Set Rx bit count and interrupt count ...
#ifndef SYNC_SERIAL
		incf    rxTimer,F         ; ... in order to keep looking for stop bit
#endif
		btfsc   rxStat,RXBREAK    ; Test the received 'break' flag skip if not set ...
		retlw   RX_BRK            ; ... otherwise return 'receiving break' status ...
		retlw   RX_BUSY           ; ... else just return 'receive busy' status

EndRx		; Seen stop bit of Rx byte
		btfsc   rxStat,RXFLAG     ; Test the Rx byte 'loaded' flag, skip if clear ...
		retlw   RX_BUSY           ; ... otherwise return (last byte still in use)

		; Rx buffer is clear, Rx byte can be stored
		btfss   rxStat,RXERR      ; Test the Rx byte error flag, skip if set ...
		goto    StoreRx           ; ... otherwise jump to store received byte in buffer

		; Stop bit of Rx byte was seen later than expected, ignore Rx byte
		bcf     rxStat,RXERR      ; Clear Rx error flag
		retlw   RX_BUSY           ; Return (received byte no good)

StoreRx		; Stop bit of Rx byte seen as expected, store Rx byte
		movf	rxReg,W           ; Copy the Rx byte from the shift register ...
		movwf   rxByte            ; ... into the buffer
		bsf     rxStat,RXFLAG     ; Set the Rx byte 'loaded' flag
		retlw   RX_BUSY

RxDataBit	; Receive next bit of Rx byte
		bcf     STATUS,C          ; Clear the carry flag
		rrf	rxReg,F           ; Rotate the carry flag into the Rx register
		btfss   rxPort,RXBIT      ; Test the Rx input bit, skip if set ...
		bsf     rxReg,MSB         ; ... else set MSB of Rx register

#ifndef SYNC_SERIAL
		movlw   CYCRXBIT          ; Reload number of ...
		movwf   rxTimer           ; ... cycles per Rx bit
#endif
                retlw   RX_BUSY

		endm


;**********************************************************************
;                                                                     *
; Macro:     ServiceTx                                                *
;                                                                     *
;            Macro to perform serial interface transmission           *
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
; Return   : W - 0 Tx idle, not currently transmitting any data       *
;                1 Tx busy, transmitting data                         *
;                                                                     *
;**********************************************************************
ServiceTx	macro   txTimer, txStat, txByte, txReg, TXFLAG, txBitCount, CYCTXBIT, txPort, TXBIT, rxPort, RXBIT

		local   ContinueTx, TxNextBit, TxOff

		; Check to see if currently transmitting a byte
		movf    txBitCount,F
		btfss   STATUS,Z          ; Skip if no Tx 'in progress' ...
		goto    ContinueTx        ; ... otherwise jump

		; Not transmitting, check to see if byte waiting to be sent
		btfsc   txStat,TXFLAG     ; Test Tx byte 'clear' flag skip if not set ...
                retlw   TX_IDLE           ; ... otherwise return (nothing to transmit)

		; Start transmitting byte
                bsf     txPort,TXBIT      ; Set Tx output for start bit

		movf    txByte,W          ; Copy Tx byte into ...
		movwf   txReg             ; ... Tx shift register
		bsf     txStat,TXFLAG     ; Set Tx byte 'clear' flag

		movlw   TX_BITS           ; Load number of...
		movwf   txBitCount        ; ... Tx bits to send
#ifndef SYNC_SERIAL
		movlw   CYCTXBIT          ; Load number of ...
		movwf   txTimer           ; ... cycles per Tx bit
#endif
                retlw   TX_BUSY

ContinueTx
#ifndef SYNC_SERIAL
		decfsz  txTimer,F         ; Decrement Tx bit interrupt count, skip if zero ...
                retlw   TX_BUSY           ; ... otherwise return
#endif
		decfsz  txBitCount,F      ; Decrement Tx bit count, skip if zero ...
		goto    TxNextBit         ; ... otherwise jump

		; Transmission complete
		retlw   TX_BUSY

TxNextBit
#ifndef SYNC_SERIAL
		movlw   CYCTXBIT          ; Reload number of ...
		movwf   txTimer           ; ... cycles per Tx bit
#endif
#ifndef SYNC_TXCD
		movlw   TX_BUSY           ; Set returned status to be 'tx in progress'
#else
		; Compare current Rx and previous Tx bits to check for collisions
		clrw                      ; Set STATUS,Z
                btfsc   rxPort,RXBIT      ; Test rx input, skip if clear ...
                iorlw   1                 ; ... else clear STATUS,Z
		btfsc   txPort,TXBIT      ; Test tx output (last bit sent), skip if clear ...
		xorlw   1                 ; ... otherwise invert STATUS,Z
		movlw   TX_BUSY           ; Set returned status to be 'tx in progress'
		btfss   STATUS,Z          ; Skip if rx and tx bits match ...
                movlw   TX_CLSN           ; ... otherwise returned status == 'tx collision'
#endif
		bsf     STATUS,C          ; Rotate one into MSB of Tx shift ...
		rrf     txReg,F           ; ... register and LSB out to carry
		btfss   STATUS,C          ; If bit is a zero ...
                bsf     txPort,TXBIT      ; ... set Tx output
		btfsc   STATUS,C          ; If bit is a one ...
                bcf     txPort,TXBIT      ; ... clear Tx output

		return                    ; W contains tx status

		endm


;**********************************************************************
;                                                                     *
; Macro:     BreakTx                                                  *
;                                                                     *
;            Macro to send 'break' on serial interface                *
;                                                                     *
; Arguments: txTimer    - Transmit timer variable                     *
;            txBitCount - Transmit bits counter                       *
;            CYCTXBIT   - Number of cycles per bit                    *
;            txPort     - Transmit port data register                 *
;            TXBIT      - Transmit port bit to use                    *
;                                                                     *
; Return   : W - 0 'break' sent                                       *
;                1 sending break                                      *
;                                                                     *
;**********************************************************************
BreakTx		macro   txTimer, txBitCount, CYCTXBIT, txPort, TXBIT

		local   ContinueBreak, ReloadBitTimer

		; Check to see if currently sending 'break'
		movf    txBitCount,F
		btfss   STATUS,Z          ; Skip if no send 'in progress' ...
		goto    ContinueBreak     ; ... otherwise jump

		; Start sending 'break'
                bsf     txPort,TXBIT      ; Set Tx output

		movlw   TX_BITS           ; Load number of...
		movwf   txBitCount        ; ... Tx bits to send
		incf    txBitCount,F

		movlw   CYCTXBIT          ; Load number of ...
		movwf   txTimer           ; ... cycless per Tx bit

		retlw   TX_BUSY

ContinueBreak	decfsz  txTimer,F         ; Decrement Tx bit interrupt count, skip if zero ...
                retlw   TX_BUSY           ; ... otherwise return

		decfsz  txBitCount,F      ; Decrement Tx bit count, skip if zero ...
		goto    ReloadBitTimer

		; Transmission complete
                bcf     txPort,TXBIT      ; Clear Tx output
		retlw   TX_IDLE

ReloadBitTimer	movlw   CYCTXBIT          ; Load number of ...
		movwf   txTimer           ; ... cycless per Tx bit

		retlw   TX_BUSY

		endm


;**********************************************************************
;                                                                     *
; Macro:     SerialRx                                                 *
;                                                                     *
;            Attempt to get an 'Rx' byte from the serial interface    *
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

		bcf     STATUS,Z          ; Set default return status of no Rx data
		btfss   rxStat,RXFLAG     ; Test the Rx byte 'loaded' flag, skip if set ...
		return                    ; ... otherwise return (receive buffer empty)

		movf	rxByte,W          ; Load W from the Rx buffer
		bcf     rxStat,RXFLAG     ; Clear the Rx byte 'loaded' flag
		bsf     STATUS,Z          ; Set return status to indicate Rx data available in W
		return

		endm


;**********************************************************************
;                                                                     *
; Macro:     SerialTx                                                 *
;                                                                     *
;            Attempt to send a 'Tx' byte to the serial interface      *
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

		bcf     STATUS,Z          ; Set default return status of no Tx performed
		btfss   txStat,TXFLAG     ; Test Tx byte 'clear' flag skip if set ...
                return                    ; ... otherwise return (transmit buffer full)

		movf    FSR,W             ; Copy Tx byte into ...
		movwf   txByte            ; ... Tx buffer
		bcf     txStat,TXFLAG     ; Clear the Tx byte 'clear' flag
		bsf     STATUS,Z          ; Set return status to indicate Tx data sent
		return

		endm


		list

