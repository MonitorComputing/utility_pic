    nolist

; $Id$

;**********************************************************************
;                                                                     *
;    Description:   Half duplex Bod data link macros.                 *
;                                                                     *
;    Author:        Chris White (whitecf@bcs.org.uk)                  *
;    Company:       Monitor Computing Services Ltd.                   *
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


; lnkState - Link state register
;   bit 0,3 - Current state
;     0  - Switching to Rx
;     1  - Waiting for interface to 'settle'
;     2  - Rx Idle
;     3  - Receiving data
;     4  - Switching to Tx
;     5  - Waiting for far end to 'turn around'
;     6  - Waiting for interface to 'settle'
;     7  - Transmiting break
;          'Active' Tx states must be in the range 8 to 15
;     8  - Tx idle
;     9  - Transmiting data
;   bit 4 - Unused
;   bit 5 - Synchronise, Tx a break or Rx until a break is received
;   bit 6 - Rx idle timedout without receiving any data
;   bit 7 - Required direction, set = Tx, clear = Rx

; Rx states
SWITCH2RXSTATE  EQU     0   ; Switching to Rx
RXSETTLESTATE   EQU     1   ; Waiting for interface to 'settle'
RXIDLESTATE     EQU     2   ; Rx Idle
RXSTATE         EQU     3   ; Receiving data

; Tx states
SWITCH2TXSTATE  EQU     4   ; Switching to Tx
TXTURNSTATE     EQU     5   ; Waiting for far end to 'turn around'
TXSETTLESTATE   EQU     6   ; Waiting for interface to 'settle'
TXBRKSTATE      EQU     7   ; Transmiting break
TXIDLESTATE     EQU     8   ; Tx idle
TXSTATE         EQU     9   ; Transmiting data

TXACTVMASK      EQU     B'00001000' ; Mask to test for in active Tx state

LNKSTTEMASK     EQU     B'00001111' ; Mask to isolate state value

LNKSYNFLAG      EQU     5   ; Synchronise Tx or Rx status bit
LNKTMOFLAG      EQU     6   ; Link Rx idle timedout status bit
LNKDIRFLAG      EQU     7   ; Required direction bit, set = Tx, clear = Rx

;**********************************************************************
;                                                                     *
; Macro:     SrvcLink                                                 *
;                                                                     *
;            State machine macro to implement half duplex Bod link    *
;                                                                     *
; Arguments: ServiceRx - Subroutine to service physical Rx            *
;            ServiceTx - Subroutine to service physical Tx            *
;            BreakTx   - Subroutine to service physical break Tx      *
;            lnkState  - Link's state machine state variable          *
;            LINKDELAY - Delay value to be used in link timing delays *
;            LINKTMOUT - Delay value to timeout if no data received   *
;            dlyTmr    - Link's delay timing variable                 *
;            EnableTx  - Subroutine to enable physical Tx             *
;            InitTx    - Subroutine to initialise physical Tx         *
;            EnableRx  - Subroutine to enable physical Rx             *
;            InitRx    - Subroutine to initialise physical Rx         *
;                                                                     *
;**********************************************************************
SrvcLink    macro   ServiceRx, ServiceTx, BreakTx, lnkState, LINKDELAY, LINKTMOUT, dlyTmr, EnableTx, InitTx, EnableRx, InitRx

            local   SrvcLnkTable, State0, State1, State2, State3, SkipRxBreak, CheckRxDir, State4, State5, State6, State7, State8, State9, CheckTxDir, FlipToRx


    movlw   high SrvcLnkTable ; Load jump table address high byte ...
    movwf   PCLATH            ; ... into PCLATH to make jump in same code block
    movf    lnkState,W        ; Use current state value ...
    andlw   LNKSTTEMASK       ; ... (after removing flag bits) ...
    addwf   PCL,F             ; ... as offset into state jump table

SrvcLnkTable
    ; Rx states
    goto    State0          ; State 0 - Switching to Rx
    goto    State1          ; State 1 - Waiting for interface to 'settle'
    goto    State2          ; State 2 - Rx Idle
    goto    State3          ; State 3 - Receiving data
    ; Tx states
    goto    State4          ; State 4  - Switching to Tx
    goto    State5          ; State 5  - Waiting for far end to 'turn around'
    goto    State6          ; State 6  - Waiting for interface to 'settle'
    goto    State7          ; State 7  - Transmiting break
    goto    State8          ; State 8  - Tx idle
    goto    State9          ; State 9  - Transmiting data

#if (high SrvcLnkTable) != (high $)
    error "Link service state jump table split across page boundary"
#endif

State0      ; State 0 - Switching to Rx

    call    EnableRx        ; Enable Rx physical interface

    movlw   low (LINKDELAY + 1)
    movwf   dlyTmr          ; Load interface 'settle' delay

    incf    lnkState,F      ; Next state = Waiting for interface to 'settle'

State1      ; Waiting for interface to 'settle'

    decfsz  dlyTmr,F        ; Decrement delay timer, skip if delay expired ...
    return                  ; ... else remain in current state

    call    InitRx          ; Initialise physical Rx

    movlw   low (LINKTMOUT + 1)
    movwf   dlyTmr          ; Load Rx idle link timeout delay

    incf    lnkState,F      ; Next state = Rx idle

State2      ; State 2 - Rx Idle

    decf    dlyTmr,W        ; Decrement Rx idle link timeout count
    btfss   STATUS,Z        ; Skip if decremented to zero ...
    movwf   dlyTmr          ; ... else update timer counter
    btfsc   STATUS,Z        ; Skip if not zero ...
    bsf     lnkState,LNKTMOFLAG

    call    ServiceRx       ; Check for receive data
    xorlw   RX_BUSY         ; Test if receiving data ...
    btfss   STATUS,Z        ; ... skip if receiving data ...
    goto    CheckRxDir      ; ... else check direction if not receiving data

    bcf     lnkState,LNKTMOFLAG

    incf    lnkState,F      ; Next state = Receiving data
    return

State3      ; State 3 - Receiving data

    call    ServiceRx       ; Receive data
    xorlw   RX_BUSY         ; Test if receiving data ...
    btfsc   STATUS,Z        ; ... skip if not receiving data ...
    return                  ; ... else return if receiving data

    xorlw   (RX_BRK ^ RX_BUSY)  ; Test if received break ...
    btfss   STATUS,Z            ; ... skip if received break ...
    goto    SkipRxBreak         ; ... else simply become Rx idle

    btfss   lnkState,LNKDIRFLAG ; Skip if direction changing to Tx ...
    bcf     lnkState,LNKSYNFLAG ; ... else clear synchronise

SkipRxBreak
    movlw   low (LINKTMOUT + 1)
    movwf   dlyTmr          ; Load Rx idle link timeout delay

    decf    lnkState,F      ; Next state = Rx Idle

CheckRxDir
    btfss   lnkState,LNKDIRFLAG
    return

    ; Need to change direction to Tx

    movf    lnkState,W      ; Get link status
    andlw   ~LNKSTTEMASK    ; Mask out state value
    iorlw   SWITCH2TXSTATE  ; Next state = Switching to Tx
    movwf   lnkState        ; Update link status
    return

State4      ; State 4 - Switching to Tx

    movlw   low (LINKDELAY + 1)
    movwf   dlyTmr          ; Load far end link 'turn around' delay

    incf    lnkState,F      ; Next state = Waiting for far end to 'turn around'

State5      ; State 5 - Waiting for far end to 'turn around'

    decfsz  dlyTmr,F        ; Decrement delay timer, skip if delay expired ...
    return                  ; ... else remain in current state

    call    EnableTx        ; Enable Tx physical interface

    movlw   low (LINKDELAY + 1)
    movwf   dlyTmr          ; Load interface 'settle' delay

    incf    lnkState,F      ; Next state = Waiting for interface to 'settle'

State6      ; State 6 - Waiting for interface to 'settle'

    decfsz  dlyTmr,F        ; Decrement delay timer, skip if delay expired ...
    return                  ; ... else remain in current state

    call    InitTx          ; Initialise physical Tx

    incf    lnkState,F          ; Next state = Transmitting break
    btfsc   lnkState,LNKSYNFLAG ; Skip if synchronise not required ...
    goto    State7              ; ... else go to State 7 - Transmitting break

    incf    lnkState,F      ; Next state = Tx idle
    goto    State8          ; Go to State 8 - Tx Idle

State7      ; State 7 - Transmitting break

    call    BreakTx         ; Transmit break
    andlw   TX_BUSY         ; Test if transmitting break ...
    btfss   STATUS,Z        ; ... skip if not transmitting break ...
    return                  ; ... else return if transmitting break

    btfsc   lnkState,LNKDIRFLAG ; Skip if direction changing to Rx ...
    bcf     lnkState,LNKSYNFLAG ; ... else clear synchronise

    incf    lnkState,F      ; Next state = Tx idle
    goto    CheckTxDir

State8      ; State 8 - Tx Idle

    call    ServiceTx       ; Check for transmitdata
    andlw   TX_BUSY         ; Test if transmitting data
    btfsc   STATUS,Z        ; Skip if transmitting data ...
    goto    CheckTxDir      ; ... else check direction if not transmitting data

    incf    lnkState,F      ; Next state = Transmitting data
    return

State9      ; State 9 - Transmitting data

    call    ServiceTx       ; Transmit data
    andlw   TX_BUSY         ; Test if transmitting data ...
    btfss   STATUS,Z        ; ... skip if not transmitting data ...
    return                  ; ... else return if transmitting data

    decf    lnkState,F      ; Next state = Tx idle

CheckTxDir
    btfss   lnkState,LNKDIRFLAG ; Skip if direction staying as Tx
    goto    FlipToRx

    btfsc   lnkState,LNKSYNFLAG ; Skip if synchronise not required ...
    decf    lnkState,F          ; ... else next state = Transmitting break
    return

FlipToRx
    ; Need to change direction to Rx

    movf    lnkState,W      ; Get link status
    andlw   ~LNKSTTEMASK    ; Mask out state value
    iorlw   SWITCH2RXSTATE  ; Next state = Switching to Rx
    movwf   lnkState        ; Update link status
    return

    endm


;**********************************************************************
;                                                                     *
; Macro:     LinkRx                                                   *
;                                                                     *
;            Attempt to get an 'Rx' byte from the link                *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;            PhysRx    - Subroutine to attempt to get 'Rx' byte from  *
;                        physical interface                           *
;                                                                     *
; Returns  : STATUS,Z  - Set if got byte, clear if not                *
;            W         - Rx byte, if any                              *
;                                                                     *
;**********************************************************************
LinkRx      macro   lnkState, PhysRx

    bcf     lnkState,LNKDIRFLAG ; Ensure link direction is Rx
    bcf     STATUS,Z            ; Clear STATUS,Z to indicate no data received
    btfsc   lnkState,LNKSYNFLAG ; Skip if synchronise not required ...
    return                      ; ... else skip data pending synchronisation

    movf    lnkState,W
    xorlw   RXIDLESTATE
    btfsc   STATUS,Z        ; Skip if not in Rx idle state ...
    goto    PhysRx          ; ... else look for received data

    xorlw   (RXSTATE ^ RXIDLESTATE)
    btfsc   STATUS,Z        ; Skip if not in receiving data state ...
    goto    PhysRx          ; ... else look for received data

    return

    endm


;**********************************************************************
;                                                                     *
; Macro:     LinkTx                                                   *
;                                                                     *
;            Attempt to send a 'Tx' byte to the link                  *
;                                                                     *
; Arguments: FSR       - Byte to send                                 *
;            lnkState  - Link's state machine state variable          *
;            PhysRx    - Subroutine to attempt to send 'Tx' byte to   *
;                        physical interface                           *
;                                                                     *
; Returns  : STATUS,Z  - Set if sent byte, clear if not               *
;                                                                     *
;**********************************************************************
LinkTx      macro   lnkState, PhysTx

    bsf     lnkState,LNKDIRFLAG ; Ensure link direction is Tx
    bcf     STATUS,Z            ; Clear STATUS,Z to indicate no data sent
    btfsc   lnkState,LNKSYNFLAG ; Skip if synchronise not required ...
    return                      ; ... else block send pending synchronisation

    movf    lnkState,W
    andlw   TXACTVMASK
    btfss   STATUS,Z        ; Skip if not in an active Tx state ...
    goto    PhysTx          ; ... else try to send data

    bcf     STATUS,Z            ; Clear STATUS,Z to indicate data not sent
    return

    endm


;**********************************************************************
;                                                                     *
; Macro:     SyncRx                                                   *
;                                                                     *
;            Synchronise by waiting until a break is received         *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;                                                                     *
;**********************************************************************
SyncRx      macro   lnkState

    bcf     lnkState,LNKDIRFLAG ; Ensure link direction is Rx
    bsf     lnkState,LNKSYNFLAG ; Set synchronise required

    endm


;**********************************************************************
;                                                                     *
; Macro:     SyncTx                                                   *
;                                                                     *
;            Synchronise by sending a break is received               *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;                                                                     *
;**********************************************************************
SyncTx      macro   lnkState

    bsf     lnkState,LNKDIRFLAG ; Ensure link direction is Tx
    bsf     lnkState,LNKSYNFLAG ; Set synchronise required

    endm


;**********************************************************************
;                                                                     *
; Macro:     IsRxIdle                                                 *
;                                                                     *
;            Test if link state machine is in 'Rx Idle' state         *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;                                                                     *
; Returns  : STATUS,Z  - Set if in 'Rx Idle' state, clear if not      *
;                                                                     *
;**********************************************************************
IsRxIdle    macro   lnkState

    movlw   LNKSTTEMASK
    andwf   lnkState,W
    xorlw   RXIDLESTATE

    endm



;**********************************************************************
;                                                                     *
; Macro:     IsTxIdle                                                 *
;                                                                     *
;            Test if link state machine is in 'Tx Idle' state         *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;                                                                     *
; Returns  : STATUS,Z  - Set if in 'Tx Idle' state, clear if not      *
;                                                                     *
;**********************************************************************
IsTxIdle    macro   lnkState

    movlw   LNKSTTEMASK
    andwf   lnkState,W
    xorlw   TXIDLESTATE

    endm

    list

