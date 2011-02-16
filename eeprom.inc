; $RCSfile$
; $Date$

;**********************************************************************
;                                                                     *
;    Filename:      eeprom.inc                                        *
;    Date:          19 Jun 2008                                       *
;    File Version:  1                                                 *
;                                                                     *
;    Author:        Chris White (whitecf@bcs.org.uk)                 *
;    Company:       Monitor Computing Services Ltd.                   *
;                                                                     *
;**********************************************************************
;                                                                     *
;    Copyright (C) 2008  Monitor Computing Services Ltd.              *
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
;    Notes: Utility functions to read and write EEPROM data.          *
;           Always return with register bank 0 selected regardless of *
;           current selection when invoked.                           *
;                                                                     *
;**********************************************************************


WaitEEPROM
        BANKSEL EECON1            ; Ensure correct register page is selected
        btfsc   EECON1,WR         ; Skip EEPROM write not 'in progress' ...
        goto    WaitEEPROM        ; ... else wait for write to complete

        BANKSEL 0                 ; Select register page 0
        return


SetEEPROM
        ; Value to be written supplied in W
        ; N.B. EEPROM location address, EEADR, must already be set
        movwf   EEDATA            ; Set EEPROM location value

        btfsc   INTCON,GIE        ; Skip if interrupts not enabled ...
        goto    EEPROMInt         ; ... else disable interrupts during write

IntEEPROM
        BANKSEL EECON1            ; Ensure correct register page is selected
        bsf     EECON1,WREN       ; Enable EEPROM writes
        movlw   0x55
        movwf   EECON2
        movlw   0xAA
        movwf   EECON2
        bsf     EECON1,WR         ; Set EEPROM write status, ...
                                  ; ... initiates hardware write cycle
        bcf     EECON1,EEIF       ; Clear EE write complete interrupt flag
        bcf     EECON1,WREN       ; Disable EEPROM writes
        BANKSEL 0                 ; Select register page 0

        return

EEPROMInt
        bcf     INTCON,GIE        ; Disable interrupts
        bcf     INTCON,GIE        ; Ensure interrupts are disabled
        call    IntEEPROM         ; Write to EEPROM
        bsf     INTCON,GIE        ; Enable interrupts
        return


GetEEPROM
        ; EEPROM address supplied in W, value returned in W
        call    WaitEEPROM        ; Ensure no EEPROM write in progress

        BANKSEL EEADR             ; Ensure correct register page is selected
        movwf   EEADR             ; Set address of EEPROM location to read

        BANKSEL EECON1            ; Ensure correct register page is selected
        bsf     EECON1,RD

        BANKSEL EEDATA            ; Ensure correct register page is selected
        movf    EEDATA,W

        BANKSEL 0                 ; Select register page 0
        return
