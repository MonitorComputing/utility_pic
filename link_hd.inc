#ifndef    LINK_HD_INC
#define    LINK_HD_INC
    nolist

; $Id$

;**********************************************************************
;                                                                     *
;    Description:   Half duplex data link macros.                     *
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


#ifndef maxlwdefined
#define maxlwdefined
;**********************************************************************
;                                                                     *
; Macro:     MaxLw                                                    *
;                                                                     *
;            Utility to load W with numerically greater of arguments  *
;                                                                     *
;**********************************************************************
MaxLw   macro   arg1, arg2

#if (arg1 > arg2)
    movlw   arg1
#else
    movlw   arg2
#endif

    endm
#endif


; lnkState - Link state register
;   bits 0,1 - Current state
;     0 - Switching to Rx
;     1 - Receiving data
;     2 - Waiting for far end to turn around
;     3 - Transmiting data
;   bits 2,3 - Unused
;   bit  4 - Tx enabled
;   bit  5 - Rx enabled
;   bit  6 - Synchronise, Tx or Rx a break
;   bit  7 - Required direction, set = Tx, clear = Rx

LNKSTTEMASK    EQU  B'00000011' ; Mask to isolate state value

LNKTXFLG       EQU  4           ; Link Tx active status bit
LNKRXFLG       EQU  5           ; Link Rx active status bit
LNKSYNFLG      EQU  6           ; Synchronise Tx or Rx status bit
LNKDIRFLG      EQU  7           ; Required direction bit, set = Tx, clear = Rx


;**********************************************************************
;                                                                     *
; Macro:     SrvcLink                                                 *
;                                                                     *
;            State machine macro to implement half duplex link        *
;                                                                     *
; Arguments: lnkState  - Link's state machine state variable          *
;            dlyTmr    - Link's delay timer                           *
;            LINKDLYRX - Delay for when switching to Rx               *
;            LINKDLYTX - Delay, used twice, for when switching to Tx  *
;            LINKTMOUT - Delay value to timeout if no data received   *
;            EnableTx  - Subroutine to enable physical Tx             *
;            InitTx    - Subroutine to initialise physical Tx         *
;            SrvcTx    - Subroutine to perform physical Tx            *
;            TxSynch   - Subroutine to start physical Tx of a synch   *
;            EnableRx  - Subroutine to enable physical Rx             *
;            InitRx    - Subroutine to initialise physical Rx         *
;            SrvcRx    - Subroutine to perform physical Rx            *
;                                                                     *
;**********************************************************************
SrvcLink    macro   lnkState, dlyTmr, LINKDLYRX, LINKDLYTX, EnableTx, InitTx, SrvcTx, TxSynch, EnableRx, InitRx, SrvcRx

            local   StateTable, State0, State1, State2, State3, SkipRxBreak, CheckRxDir, State4, State5, State6, State7, State8, State9, CheckTxDir, FlipToRx, SrvcLinkTmr, SrvcLinkEnd

#ifndef CLKD_LINK
    decf    dlyTmr,W        ; Decrement delay timing counter
    btfss   STATUS,Z        ; Skip if delay expired ...
    goto    SrvcLinkTmr     ; ... else just update the delay timer
#endif

    movlw   high StateTable   ; Load jump table address high byte ...
    movwf   PCLATH            ; ... into PCLATH to make jump in same code block
    movf    lnkState,W        ; Use current state value ...
    andlw   LNKSTTEMASK       ; ... (after removing flag bits) ...
    addwf   PCL,F             ; ... as offset into state jump table

StateTable
    ; Rx states
    goto    State0          ; State 0  - Switching to Rx
    goto    State1          ; State 1  - Receiving data
    goto    State2          ; State 2  - Waiting for far end to turn around
    goto    State3          ; State 3  - Transmiting data


#if (high StateTable) != (high $)
    error "Link service state machine split across page boundary"
#endif

State0      ; State 0 - Switching to Rx

    call    EnableRx        ; Enable Rx physical interface
    call    InitRx          ; Initialise physical Rx

    incf    lnkState,F      ; Next state = Receiving data

    MaxLw   LINKDLYRX, 1
    goto    SrvcLinkTmr     ; Load physical interface settle delay

State1      ; State 1 - Receiving data

    bsf     lnkState,LNKRXFLG   ; Enable link Rx

    call    SrvcRx              ; Service link serial reception

    btfss   lnkState,LNKDIRFLG  ; Skip if required direction is Tx ...
    goto    SrvcLinkEnd         ; ... else continue to receive

    ; Need to change direction to Tx

    bcf     lnkState,LNKRXFLG   ; Disable link Rx (link is half duplex)

    incf    lnkState,F      ; Next state = Waiting for far end to turn around

    MaxLw   LINKDLYTX, 1
    goto    SrvcLinkTmr     ; Load far end turn around delay

State2      ; State 2 - Waiting for far end to turn around

    call    EnableTx        ; Enable Tx physical interface
    call    InitTx          ; Initialise physical Tx

    bsf     lnkState,LNKTXFLG   ; Enable link Tx

    incf    lnkState,F      ; Next state = Transmiting data

    MaxLw   LINKDLYTX, 1
    goto    SrvcLinkTmr     ; Load physical interface settle delay

State3     ; State 3 - Transmiting data

    call    SrvcTx          ; Service link serial transmission
    xorlw   TX_IDLE         ; Test if transmit is idle ...
    btfss   STATUS,Z        ; ... if so skip  ...
    goto    SrvcLinkEnd     ; ... else continue transmiting

    btfsc   lnkState,LNKSYNFLG  ; Skip if synchronise not required ...
    call    TxSynch             ; ... else trigger synchronise
    bcf     lnkState,LNKSYNFLG  ; Clear synchronise required flag

    btfsc   lnkState,LNKDIRFLG  ; Skip if required direction is Rx
    goto    SrvcLinkEnd         ; ... else continue transmiting

    ; Change direction to Rx

    bcf     lnkState,LNKTXFLG   ; Disable link Tx (link is half duplex)

    movlw   ~LNKSTTEMASK    ; Mask out state value
    andwf   lnkState,F      ; Next state = Switching to Rx
    goto    State0

SrvcLinkTmr
#ifndef CLKD_LINK
    movwf   dlyTmr          ; Update timing counter
#endif

SrvcLinkEnd

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

    bcf     STATUS,Z            ; Default return is no data received

    btfss   lnkState,LNKRXFLG   ; Skip if in receiving data state ...
    return                      ; ... else abandon check for received data

    btfss   lnkState,LNKSYNFLG  ; Skip if waiting to synchronise ...
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

    bcf     STATUS,Z            ; Default return is no data sent

    btfsc   lnkState,LNKTXFLG   ; Skip if not in sending data state ...
    goto    PhysTx              ; ... else attempt to send data
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

    list
#endif
