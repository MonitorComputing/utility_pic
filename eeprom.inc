    nolist
#ifndef    EEPROM_INC
#define    EEPROM_INC

;**********************************************************************
;                                                                     *
; Description: Utility functions to read and write EEPROM data.       *
;              Always return with register bank 0 selected regardless *
;              of current selection when invoked.                     *
;                                                                     *
; Author: Chris White (whitecf69@gmail.com)                           *
;                                                                     *
; Copyright (C) 2008 by Monitor Computing Services Limited, licensed  *
; under CC BY-NC-SA 4.0. To view a copy of this license, visit        *
; https://creativecommons.org/licenses/by-nc-sa/4.0/                  *
;                                                                     *
; This program is distributed in the hope that it will be useful, but *
; WITHOUT ANY WARRANTY; without even the implied warranty of          *
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
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

#endif
    list
