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
;          Rx states must be in the range 0 to 3
;     0  - Switching to Rx
;     1  - Waiting for interface lines to settle
;     2  - Receiving data
;     3  - Unused
;          Tx states must be in the range 4 to 15
;     4  - Unused
;     5  - Unused
;     6  - Switching to Tx
;     7  - Waiting for far end to turn around
;          'Active' Tx states must be in the range 8 to 15
;     8  - Waiting for interface lines to settle
;     9  - Transmiting break
;     10 - Transmiting data
;   bit 4 - Tx enabled
;   bit 5 - Rx enabled
;   bit 6 - Synchronise, Tx or Rx a break
;   bit 7 - Required direction, set = Tx, clear = Rx

; Rx states
SWITCH2RXSTATE EQU  0           ; Switching to Rx
RXSETTLESTATE  EQU  1           ; Waiting for interface to 'settle'
RXSTATE        EQU  2           ; Receiving data

; Tx states
SWITCH2TXSTATE EQU  6           ; Switching to Tx
TXTURNSTATE    EQU  7           ; Waiting for far end to 'turn around'
TXSETTLESTATE  EQU  8           ; Waiting for interface to 'settle'
TXBRKSTATE     EQU  9           ; Transmiting break
TXSTATE        EQU  10          ; Transmiting data

TXSTTEMASK     EQU  B'00001100' ; Mask to test for any Tx state
TXACTVMASK     EQU  B'00001000' ; Mask for in an active Tx state

LNKSTTEMASK    EQU  B'00001111' ; Mask to isolate state value

LNKTXFLG       EQU  4           ; Link Tx active status bit
LNKTXMSK       EQU  B'00010000' ; Link Tx active status mask
LNKRXFLG       EQU  5           ; Link Rx active status bit
LNKRXMSK       EQU  B'00100000' ; Link Rx active status mask
LNKSYNFLG      EQU  6           ; Synchronise Tx or Rx status bit
LNKSYNMSK      EQU  B'01000000' ; Link synchronise status mask
LNKDIRFLG      EQU  7           ; Required direction bit, set = Tx, clear = Rx
LNKDIRMSK      EQU  B'10000000' ; Link direction status mask

;**********************************************************************
;                                                                     *
; Macro:     SrvcLink                                                 *
;                                                                     *
;            State machine macro to implement half duplex Bod link    *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;            linkTimer - Rx timeout timer                             *
;            dlyTmr    - Link's delay timer                           *
;            LINKDLYRX - Delay for when switching to Rx               *
;            LINKDLYTX - Delay, used twice, for when switching to Tx  *
;            LINKTMOUT - Delay value to timeout if no data received   *
;            EnableTx  - Subroutine to enable physical Tx             *
;            InitTx    - Subroutine to initialise physical Tx         *
;            IsTxIdle  - Subroutine to check if physical Tx is idle   *
;            EnableRx  - Subroutine to enable physical Rx             *
;            InitRx    - Subroutine to initialise physical Rx         *
;                                                                     *
;**********************************************************************
SrvcLink    macro   lnkState, linkTimer, dlyTmr, LINKDLYRX, LINKDLYTX, LINKTMOUT, EnableTx, InitTx, IsTxIdle, EnableRx, InitRx

            local   StateTable, State0, State1, State2, State3, SkipRxBreak, CheckRxDir, State4, State5, State6, State7, State8, State9, State10, CheckTxDir, GoToState10, FlipToRx

    movlw   high StateTable   ; Load jump table address high byte ...
    movwf   PCLATH            ; ... into PCLATH to make jump in same code block
    movf    lnkState,W        ; Use current state value ...
    andlw   LNKSTTEMASK       ; ... (after removing flag bits) ...
    addwf   PCL,F             ; ... as offset into state jump table

StateTable
    ; Rx states
    goto    State0          ; State 0  - Switching to Rx
    goto    State1          ; State 1  - Waiting for interface lines to settle
    goto    State2          ; State 2  - Receiving data
    return                  ; State 3  - Unused
    ; Tx states
    return                  ; State 4  - Unused
    return                  ; State 5  - Unused
    goto    State6          ; State 6  - Switching to Tx
    goto    State7          ; State 7  - Waiting for far end to turn around
    ;  'Active' Tx states must be in the range 8 to 15
    goto    State8          ; State 8  - Waiting for interface lines to settle
    goto    State9          ; State 9  - Transmiting break
    goto    State10         ; State 10 - Transmiting data

#if (high StateTable) != (high $)
    error "Link service state machine split across page boundary"
#endif

State0      ; State 0 - Switching to Rx

    bcf     lnkState,LNKTXFLG   ; Disable physical Tx (link is half duplex)

    call    EnableRx        ; Enable Rx physical interface
    call    InitRx          ; Initialise physical Rx

    movlw   (LINKDLYRX + 1)
    movwf   dlyTmr          ; Load interface settle delay

    incf    lnkState,F      ; Next state = Waiting for interface to settle

State1      ; Waiting for interface lines to settle

    decfsz  dlyTmr,W        ; Skip if delay expired ...
    return                  ; ... else remain in current state

    movlw   (LINKTMOUT + 1)
    movwf   linkTimer       ; Reset no data received timeout

    bsf     lnkState,LNKRXFLG   ; Enable physical Rx

    incf    lnkState,F      ; Next state = Receiving data

State2      ; State 2 - Receiving data

    btfss   lnkState,LNKDIRFLG  ; Skip if required direction is Tx ...
    return                      ; ... else remain in current state

    ; Need to change direction to Tx

    incf    lnkState,F      ; Next state = Unused

State3      ; State 5 - Unused
    incf    lnkState,F      ; Next state = Unused

State4      ; State 5 - Unused
    incf    lnkState,F      ; Next state = Unused

State5      ; State 5 - Unused
    incf    lnkState,F      ; Next state = Unused

State6      ; State 6 - Switching to Tx

    bcf     lnkState,LNKRXFLG   ; Disable physical Rx (link is half duplex)

    movlw   (LINKTMOUT + 1)
    movwf   linkTimer       ; Reset link Rx idle timeout counter

    movlw   (LINKDLYTX + 1)
    movwf   dlyTmr          ; Load far end turn around delay

    incf    lnkState,F      ; Next state = Waiting for far end to turn around

State7      ; State 7 - Waiting for far end to turn around

    decfsz  dlyTmr,W        ; Skip if delay expired ...
    return                  ; ... else remain in current state

    call    EnableTx        ; Enable Tx physical interface
    call    InitTx          ; Initialise physical Tx

    movlw   (LINKDLYTX + 1)
    movwf   dlyTmr          ; Load interface settle delay

    incf    lnkState,F      ; Next state = Waiting for interface to settle

State8      ; State 8 - Waiting for interface to settle

    decfsz  dlyTmr,W        ; Skip if delay expired ...
    return                  ; ... else remain in current state

    bsf     lnkState,LNKTXFLG   ; Enable physical Tx

    incf    lnkState,F          ; Next state = Transmiting break
    btfss   lnkState,LNKSYNFLG  ; Skip if synchronise required ...
    goto    GoToState10         ; ... else jump into transmiting data state

State9      ; State 9 - Transmiting break

    btfsc   lnkState,LNKDIRFLG  ; Skip if direction is changing to Rx ...
    bcf     lnkState,LNKSYNFLG  ; ... else clear synchronise

GoToState10
    incf    lnkState,F      ; Next state = Transmiting data

State10     ; State 10 - Transmiting data

    btfss   lnkState,LNKDIRFLG  ; Skip if required direction is Tx
    goto    FlipToRx

    btfss   lnkState,LNKSYNFLG  ; Skip if synchronise required ...
    return                      ; ... else remain in current state

    decf    lnkState,F          ; Next state = Transmiting break
    goto    State9

FlipToRx
    ; Need to change direction to Rx

    call    IsTxIdle
    btfss   STATUS,Z        ; Skip if transmit is idle ...
    return                  ; ... else continue transmiting

    movf    lnkState,W      ; Get link status
    andlw   ~LNKSTTEMASK    ; Mask out state value
    iorlw   SWITCH2RXSTATE  ; Next state = Switching to Rx
    movwf   lnkState        ; Update link status
    goto    State0

    endm


;**********************************************************************
;                                                                     *
; Macro:     SrvcLinkIf                                               *
;                                                                     *
;            Interrupt servicing of link physical interface           *
;                                                                     *
; Arguments: linkState  - Link's state machine state variable         *
;            linkTimer - Rx timeout timer                             *
;            LINKTMOUT - Delay value to timeout if no data received   *
;            SrvcTx    - Subroutine to service physical Tx            *
;            SrvcRx    - Subroutine to service physical Rx            *
;                                                                     *
;**********************************************************************
SrvcLinkIf  macro   linkState, linkTimer, LINKTMOUT, SrvcTx, SrvcRx

            local   linkNotRx

    btfss   linkState,LNKRXFLG
    goto    linkNotRx

    decfsz  linkTimer,W     ; Decrement Rx timeout counter, skip if zero ...
    movwf   linkTimer       ; ... else update the counter

    call    SrvcRx          ; Service link serial reception
    xorlw   RX_BUSY         ; Test if receiving data
    movlw   (INTLINKTMOP + 1)
    btfsc   STATUS,Z        ; Skip if not receiving data ...
    movwf   linkTimer       ; ... else reload the Rx timeout counter

linkNotRx
    btfsc   linkState,LNKTXFLG
    call    SrvcTx          ; Service link serial transmission

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

            local   QuitRx

    movf    lnkState,W
    andlw   TXSTTEMASK
    btfss   STATUS,Z            ; Skip if not in any Tx state ...
    return                      ; ... else abandon check for received data

    btfsc   lnkState,LNKSYNFLG  ; Skip if waiting to synchronise ...
    return                      ; ... else abandon check for received data

    bcf     STATUS,Z            ; Cannot be receive if not in Rx state
    btfsc   lnkState,LNKRXFLG   ; Skip if not in receiving data state ...
    goto    PhysRx              ; ... else look for received data
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

            local   QuitTx

    movf    lnkState,W
    andlw   TXACTVMASK
    btfss   STATUS,Z        ; Skip if not in an active Tx state ...
    goto    PhysTx          ; ... else attempt to send data

QuitTx
    bcf     STATUS,Z        ; Clear STATUS,Z to indicate data not sent
    return

    endm


;**********************************************************************
;                                                                     *
; Macro:     SyncRx                                                   *
;                                                                     *
;            Schedule Rx synchronisation                              *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;                                                                     *
;**********************************************************************
SyncRx      macro   lnkState

    bcf     lnkState,LNKDIRFLG  ; Ensure link direction is Rx
    bsf     lnkState,LNKSYNFLG  ; Set synchronise required

    endm


;**********************************************************************
;                                                                     *
; Macro:     SyncTx                                                   *
;                                                                     *
;            Schedule Tx synchronisation                              *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;                                                                     *
;**********************************************************************
SyncTx      macro   lnkState

    bsf     lnkState,LNKDIRFLG  ; Ensure link direction is Tx
    bsf     lnkState,LNKSYNFLG  ; Set synchronise required

    endm


;**********************************************************************
;                                                                     *
; Macro:     IsLinkRxTo                                               *
;                                                                     *
;            Test if link reception has timed out                     *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;            linkTimer - Rx timeout timer                             *
;                                                                     *
; Returns  : STATUS,Z  - Set if Rx timedout, clear if not             *
;                                                                     *
;**********************************************************************
IsLinkRxTo  macro   lnkState, linkTimer

    bcf     STATUS,Z            ; Cannot be timed out if not in Rx state
    btfsc   lnkState,LNKRXFLG   ; Skip if not in receiving data state ...
    decf    linkTimer,W         ; ... else test link timeout timer

    endm

    list

