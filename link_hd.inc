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


; Rx states
SWITCH2RXSTATE  EQU     0
RXSETTLESTATE   EQU     1
RXIDLESTATE     EQU     2
RXSTATE         EQU     3
; Tx states (must start on even number for TXSTATEMASK to work)
SWITCH2TXSTATE  EQU     4
TXTURNSTATE     EQU     5
TXSETTLESTATE   EQU     6
TXIDLESTATE     EQU     7
TXSTATE         EQU     8

TXSTATEMASK     EQU     B'11111100'


;**********************************************************************
;                                                                     *
; Macro:     SrvcLink                                                 *
;                                                                     *
;            State machine macro to implement half duplex Bod link    *
;                                                                     *
; Arguments: ServiceRx - Subroutine to service physical Rx            *
;            ServiceTx - Subroutine to service physical Tx            *
;            lnkSte    - Link's state machine state variable          *
;            LINKDELAY - Delay value to be used in link timing delays *
;            dlyTmr    - Link's delay timing variable                 *
;            EnableTx  - Subroutine to enable physical Tx             *
;            InitTx    - Subroutine to initialise physical Tx         *
;            EnableRx  - Subroutine to enable physical Rx             *
;            InitRx    - Subroutine to initialise physical Rx         *
;                                                                     *
;**********************************************************************
SrvcLink    macro   ServiceRx, ServiceTx, lnkSte, LINKDELAY, dlyTmr, EnableTx, InitTx, EnableRx, InitRx

            local   SrvcLnkTable, State0, State1, State2, State3, State4, State5, State6, State7, State8


    movlw   high SrvcLnkTable ; Load jump table address high byte ...
    movwf   PCLATH            ; ... into PCLATH to make jump in same code block
    movf    lnkSte,W          ; Use current state value as ...
    addwf   PCL,F             ; ... offset into state jump table

SrvcLnkTable
    ; Rx states
    goto    State0          ; State 0 - Switching to RX
    goto    State1          ; State 1 - Waiting for interface to 'settle'
    goto    State2          ; State 2 - Rx Idle
    goto    State3          ; State 3 - Receiving data
    ; Tx states (must start on even number for TXSTATEMASK to work)
    goto    State4          ; State 4 - Switching to TX
    goto    State5          ; State 5 - Waiting for far end to 'turn around'
    goto    State6          ; State 6 - Waiting for interface to 'settle'
    goto    State7          ; State 7 - Tx idle
    goto    State8          ; State 8 - Transmiting data

#if (high SrvcLnkTable) != (high $)
    error "Link service state jump table split across page boundary"
#endif

State0      ; State 0 - Switching to RX

    call    EnableRx        ; Enable Rx physical interface

    movlw   low (LINKDELAY + 1)
    movwf   dlyTmr          ; Load interface 'settle' delay

    incf    lnkSte,F        ; Next state = Waiting for interface to 'settle'

State1      ; Waiting for interface to 'settle'

    decfsz  dlyTmr,F        ; Decrement delay timer, skip if delay expired ...
    return                  ; ... otherwise remain in current state

    call    InitRx          ; Initialise physical Rx

    incf    lnkSte,F        ; Next state = Rx idle

State2      ; State 2 - Rx Idle

    call    ServiceRx       ; Check for receive data
    xorlw   RX_IDLE         ; Test if not receiving data ...
    btfsc   STATUS,Z        ; ... skip if receiving data ...
    return                  ; ... return if not receiving data

    incf    lnkSte,F        ; ... otherwise, next state = Receiving data
    return

State3      ; State 3 - Receiving data

    call    ServiceRx       ; Receive data
    xorlw   RX_IDLE         ; Test if not receiving data ...
    btfss   STATUS,Z        ; ... skip if not receiving data ...
    return                  ; ... else return if receiving data

    decf    lnkSte,F        ; ... otherwise, next state = Rx Idle
    return

State4      ; State 4 - Switching to TX

    movlw   low (LINKDELAY + 1)
    movwf   dlyTmr          ; Load far end link 'turn around' delay

    incf    lnkSte,F        ; Next state = Waiting for far end to 'turn around'

State5      ; State 5 - Waiting for far end to 'turn around'

    decfsz  dlyTmr,F        ; Decrement delay timer, skip if delay expired ...
    return                  ; ... otherwise remain in current state

    call    EnableTx        ; Enable Tx physical interface

    movlw   low (LINKDELAY + 1)
    movwf   dlyTmr          ; Load interface 'settle' delay

    incf    lnkSte,F        ; Next state = Waiting for interface to 'settle'

State6      ; State 6 - Waiting for interface to 'settle'

    decfsz  dlyTmr,F        ; Decrement delay timer, skip if delay expired ...
    return                  ; ... otherwise remain in current state

    call    InitTx          ; Initialise physical Tx

    incf    lnkSte,F        ; Next state = Tx idle

State7      ; State 7 - Tx Idle

    call    ServiceTx       ; Check for transmitdata
    andlw   TX_BUSY         ; Test if transmitting data ...
    btfsc   STATUS,Z        ; ... skip if transmitting data ...
    return                  ; ... else return if not transmitting data

    incf    lnkSte,F        ; Next state = Transmitting data
    return

State8      ; State 8 - Transmitting data

    call    ServiceTx       ; Transmit data
    andlw   TX_BUSY         ; Test if transmitting data ...
    btfss   STATUS,Z        ; ... skip if not transmitting data ...
    return                  ; ... else return if transmitting data

    decf    lnkSte,F        ; Next state = Tx idle
    return

    endm


;**********************************************************************
;                                                                     *
; Macro:     LinkRx                                                   *
;                                                                     *
;            Attempt to get an 'Rx' byte from the link                *
;                                                                     *
; Arguments: lnkSte    - Link's state machine state variable          *
;            PhysRx    - Subroutine to attempt to get 'Rx' byte from  *
;                        physical interface                           *
;                                                                     *
; Returns  : STATUS,Z  - Set if got byte, clear if not                *
;            W         - Rx byte, if any                              *
;                                                                     *
;**********************************************************************
LinkRx      macro   lnkSte, PhysRx

    movlw   RXIDLESTATE
    xorwf   lnkSte,W
    btfsc   STATUS,Z        ; Skip if not in Rx idle state ...
    goto    PhysRx          ; ... otherwise look for received data

    movlw   RXSTATE
    xorwf   lnkSte,W
    btfsc   STATUS,Z        ; Skip if not in receiving data state ...
    goto    PhysRx          ; ... otherwise look for received data

    movlw   TXIDLESTATE
    xorwf   lnkSte,W
    btfss   STATUS,Z        ; Skip if in Tx idle state ...
    return                  ; ... otherwise give up and return

    ; Link is in Tx idle state, force turn around to Rx
    movlw   SWITCH2RXSTATE
    movwf   lnkSte
    bcf     STATUS,Z        ; Clear STATUS,Z to indicate no data received
    return

    endm


;**********************************************************************
;                                                                     *
; Macro:     LinkTx                                                   *
;                                                                     *
;            Attempt to send a 'Tx' byte to the link                  *
;                                                                     *
; Arguments: FSR       - Byte to send                                 *
;            lnkSte    - Link's state machine state variable          *
;            PhysRx    - Subroutine to attempt to send 'Tx' byte to   *
;                        physical interface                           *
;                                                                     *
; Returns  : STATUS,Z  - Set if sent byte, clear if not               *
;                                                                     *
;**********************************************************************
LinkTx      macro   lnkSte, PhysTx

            local NotIdle, GiveUpTx

    movlw   TXIDLESTATE
    xorwf   lnkSte,W
    btfss   STATUS,Z        ; Skip if in Tx idle state ...
    goto    NotIdle         ; ... otherwise jump

    ; Link in Tx idle state, try to send data
    call    PhysTx
    btfss   STATUS,Z        ; Skip if managed to start Tx ...
    return                  ; ... otherwise give up and return

    movlw   TXSTATE         ; Tx started, set link ...
    movwf   lnkSte          ; ... state to transmitting
    return                  ; STATUS,Z still set to indicate data sent

NotIdle
    movlw   TXSTATE
    xorwf   lnkSte,W
    btfsc   STATUS,Z        ; Skip if not in transmitting state ...
    goto    PhysTx          ; ... otherwise try to send data

    movlw   RXSTATE
    xorwf   lnkSte,W
    btfsc   STATUS,Z        ; Skip if not in receiving state ...
    goto    GiveUpTx        ; ... otherwise give up and return

    movlw   TXSTATEMASK
    andwf   lnkSte,W
    btfss   STATUS,Z        ; Skip if not in any Tx state ...
    goto    GiveUpTx        ; ... otherwise give up and return


    ; Link is in non receiving Rx state, force turn around to transmit
    movlw   SWITCH2TXSTATE
    movwf   lnkSte

GiveUpTx
    bcf     STATUS,Z        ; Clear STATUS,Z to indicate data not sent
    return

    endm


;**********************************************************************
;                                                                     *
; Macro:     IsRxIdle                                                 *
;                                                                     *
;            Test if link state machine is in 'Rx Idle' state         *
;                                                                     *
; Arguments: lnkSte    - Link's state machine state variable          *
;                                                                     *
; Returns  : STATUS,Z  - Set if in 'Rx Idle' state, clear if not      *
;                                                                     *
;**********************************************************************
IsRxIdle    macro   lnkSte

    movlw   RXIDLESTATE
    xorf    lnkSte,W

    endm



;**********************************************************************
;                                                                     *
; Macro:     IsTxIdle                                                 *
;                                                                     *
;            Test if link state machine is in 'Tx Idle' state         *
;                                                                     *
; Arguments: lnkSte    - Link's state machine state variable          *
;                                                                     *
; Returns  : STATUS,Z  - Set if in 'Tx Idle' state, clear if not      *
;                                                                     *
;**********************************************************************
IsTxIdle    macro   lnkSte

    movlw   TXIDLESTATE
    xorf    lnkSte,W

    endm

    list

