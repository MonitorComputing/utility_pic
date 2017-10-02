        nolist

;**********************************************************************
;                                                                     *
;    Filename:      monitor.inc                                       *
;    Date:          16 Mar 2001                                       *
;    File Version:  9                                                 *
;                                                                     *
;    Author:        Chris White (whitecf@bcs.org.uk)                 *
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
;    Notes: Simple low level monitor program offering terminal        *
;           interface via a Bod link API conformant link ('up' link)  *
;                                                                     *
;           Requires the use to have defined the following:           *
;                                                                     *
;             BootVector - address to 'goto' to reboot system         *
;             UserInt  - routine to perform 'User' interrupt          *
;                        functions (extra to basic ISR functionality) *
;             LinkMRx  - routine to receive data ('up' link)          *
;             LinkMTx  - routine to transmit data ('up' link)         *
;             UserInit - routine to perform 'User' initialisation     *
;                        functions (extra to processor reset)         *
;             UserMain - routine to perform 'User' main functions     *
;                                                                     *
;           If EEDATA is not defined then EEPROM commands are not     *
;           available to the user.                                    *
;           If ADRES is not defined then A/D commands are not         *
;           available to the user.                                    *
;           If A/D commands are to be used then ADTRACKTIME must be   *
;           defined in order to time the A/D sample settleing delay.  *
;           If NOMONBANNER is defined then start up banner and prompt *
;           are not displayed (this would be used in a daisy chain    *
;           for nodes other than the head of the chain).              *
;           If MONUSERON is defined then the monitor will initialise  *
;           with 'user processing' enabled (default is disabled).     *
;           If LinkHTx is defined then the 'hop' feature is available.*
;           This allows a daisy chain to be built in which each node  *
;           can be made to 'hop' the interface to the next node in    *
;           the chain.  When 'hop' is in effect the prompt presented  *
;           to the user indicates the number of 'hops' in place.  If  *
;           'hop' mode is to be used the following must be defined:   *
;             LinkHRx  - routine to receive data ('down' link)        *
;             LinkHTx  - routine to transmit data ('down' link)       *
;                                                                     *
;**********************************************************************


;**********************************************************************
; Variable definitions                                                *
;**********************************************************************

        CBLOCK

; Monitor registers
monState      ; State and status
monData       ; Temporary register
monReg        ; Register file (EEPROM address) to read/write
monCmd        ; User command
monValue      ; Read/write value
        ENDC
#ifdef LinkHTx
        CBLOCK
monHop        ; 'Hop' level of this node in command 'daisy chain'
        ENDC
#endif


;**********************************************************************
; Constant definitions                                                *
;**********************************************************************

; Monitor constants
userBit       EQU     7           ; Monitor state bit 7 = run user code flag
userMask      EQU     0x80        ; Mask to reset monitor state without affecting user bit
stateMask     EQU     0x7F        ; Mask to remove user bit from monitor state value
cmdMask       EQU     0x0F        ; Mask to mask command when used as offset into jump table

; Monitor user interface constants
caseBit       EQU     5           ; Bit to change case of ASCII alpha characters
caseMask      EQU     0xDF        ; Mask to up shift case of ASCII alpha characters
reboot        EQU     'B'         ; Reboot system
register      EQU     'R'         ; Read/write register
eeprom        EQU     'E'         ; Read/write EEPROM
stopUser      EQU     'S'         ; Stop user code execution
contUser      EQU     'C'         ; Continue user code execution
initUser      EQU     'U'         ; Initialise user code
incReg        EQU     'I'         ; Increment register
decReg        EQU     'D'         ; Decrement register
adRead        EQU     'N'         ; Read analogue input
andReg        EQU     'A'         ; And with register
orReg         EQU     'O'         ; Or with register
xorReg        EQU     'X'         ; Xor with register
cancelHop     EQU     'Q'         ; Cancel all hops
setHop        EQU     'H'         ; Setup hop, or set hop level if accompanied with data
setCmdBit     EQU     3           ; Command bit to set to make read become write
cmdReboot     EQU     0x00        ; Reboot system
cmdRegister   EQU     0x01        ; Read register (0x09 = write)
cmdEeprom     EQU     0x02        ; Read EEPROM (0x0A = write)
cmdStop       EQU     0x03        ; Stop user code execution
cmdContinue   EQU     0x04        ; Continue user code execution
cmdInit       EQU     0x05        ; Initialise user code
cmdIncReg     EQU     0x06        ; Increment register
cmdDecReg     EQU     0x07        ; Decrement register
cmdAdRead     EQU     0x08        ; Read analogue input
wrtRegister   EQU     0x09        ; Write register
wrtEeprom     EQU     0x0A        ; Write EEPROM
cmdAndReg     EQU     0x0B        ; And with register
cmdOrReg      EQU     0x0C        ; Or with register
cmdXorReg     EQU     0x0D        ; Xor with register
cmdCancelHop  EQU     0x0E        ; Cancel all hops
cmdSetHop     EQU     0x0F        ; Setup hop, or set hop level if accompanied with data
delimt        EQU     ','         ; Character delimiting input value from address
execute       EQU     0x0D        ; Character marking end of command
escape        EQU     0x1B        ; Character indicating abandon command
crCode        EQU     0x0D        ; ASCII carriage return code
lfCode        EQU     0x0A        ; ASCII line feed code
bellCode      EQU     0x07        ; ASCII bell code
promptCode    EQU     '>'


;**********************************************************************
; Interrupt service routine (ISR) code macro                          *
;**********************************************************************

MonitorISR  macro

        btfsc   monState,userBit  ; Test 'perform user processing' bit, skip if not set ...
        call    UserInt           ; ... otherwise run user interrupt code

        endm


;**********************************************************************
; Monitor command interpreter code                                    *
;**********************************************************************

NextMonState    incf    monState,F

Monitor     movlw   high MonitorTable ; Load jump table address high byte ...
        movwf   PCLATH            ; ... into PCLATH to make jump in same code block
        movf    monState,W        ; Use current state value ...
        andlw   stateMask         ; ... with 'perform user processing' bit masked out ...
        addwf   PCL,F             ; ... as offset into state jump table

        ; Common initial state sequence
MonitorTable    goto    AwaitingData      ; State  0 - Waiting for user input
        goto    SendingUp         ; State  1 - Echo user input
        goto    DecodingCmd       ; State  2 - Determine user command

        ; 'Read or Set' command state sequence
        goto    AwaitingData      ; State  3 - Waiting for first digit of register number
        goto    SendingUp         ; State  4 - Echo user input
        goto    DecodingRegDig1   ; State  5 - Decoding first digit of register number

        goto    AwaitingData      ; State  6 - Waiting for second digit of register number
        goto    SendingUp         ; State  7 - Echo user input
        goto    DecodingRegDig2   ; State  8 - Decoding second digit of register number

        goto    AwaitingData      ; State  9 - Waiting for value delimiter character
        goto    SendingUp         ; State 10 - Echo user input
        goto    Chkng4delimt      ; State 11 - Checking for value delimiter character

        goto    AwaitingData      ; State 12 - Waiting for first digit of value
        goto    SendingUp         ; State 13 - Echo user input
        goto    DecodingValDig1   ; State 14 - Decoding first digit of register value

        goto    AwaitingData      ; State 15 - Waiting for second digit of value
        goto    SendingUp         ; State 16 - Echo user input
        goto    DecodingValDig2   ; State 17 - Decoding second digit of register value

        ; 'End of command' state sequence
        goto    AwaitingData      ; State 18 - Waiting for end of user command sequence
        goto    EscapeCmd         ; State 19 - Invalid user command sequence

        ; Command response sequence
        goto    SendingCR         ; State 20-  Sending carriage return code
        goto    SendingLF         ; State 21 - Sending line feed code
        goto    EncodingDigit     ; State 22 - Sending high byte of response
        goto    SendingUp         ; State 23
        goto    EncodingDigit     ; State 24 - Sending low byte of response
        goto    SendingUp         ; State 25
        goto    SendingCR         ; State 26 - Sending carriage return code
        goto    SendingLF         ; State 27 - Sending line feed code
#ifndef LinkHTx
        ; Display prompt sequence
        goto    SendingPrompt     ; State 28 - Sending hop prompt code
        goto    ResetMonState     ; State 29 - Pseudo state terminating prompt
        goto    ResetMonState     ; State 30 - Pseudo state terminating prompt
        goto    ResetMonState     ; State 31 - Pseudo state terminating prompt
        goto    ResetMonState     ; State 32 - Pseudo state terminating prompt
        goto    ResetMonState     ; State 33 - Pseudo state terminating prompt
#else
        ; Display prompt sequence
        goto    EncodingHop       ; State 28 - Sending high byte of response
        goto    SendingUp         ; State 29
        goto    EncodingDigit     ; State 30 - Sending low byte of response
        goto    SendingUp         ; State 31
        goto    SendingPrompt     ; State 32 - Sending hop prompt code
        goto    ResetMonState     ; State 33 - Pseudo state terminating prompt

        ; Hop sequence
        goto    SendingHop        ; State 34
        goto    AwaitingEcho      ; State 35
        goto    EncodingNextHop   ; State 36 - Sending high byte of next hop
        goto    SendingDown       ; State 37
        goto    AwaitingEcho      ; State 38
        goto    EncodingDigit     ; State 39 - Sending low byte of next hop
        goto    SendingDown       ; State 40
        goto    AwaitingEcho      ; State 41
        goto    ForwardingCR      ; State 42 - Forwarding carriage return code

        goto    AwaitingForward   ; State 43
        goto    ForwardingDown    ; State 44
        goto    ForwardingUp      ; State 45
        goto    SendingUnhop      ; State 46
        goto    CmdCmpltd         ; State 47 - Pseudo state terminating hopping
#endif
        ; Reset state machine
ResetMonState   clrw

#if (high MonitorTable) != (high $)
    error "Monitor state jump table split across page boundary"
#endif

        btfsc   monState,userBit  ; If run user process bit is set ...
        iorlw   userMask          ; ... ensure it remains so
        movwf   monState
        return

        ; State constants
readSetState    EQU     3
valueState      EQU     12
noValueState    EQU     18
responseState   EQU     20
rawSendState    EQU     25
endCmdState     EQU     26
endPromptState  EQU     32
#ifdef LinkHTx
beginHopState   EQU     34
hopState        EQU     43
hopDownState    EQU     44
hopUpState      EQU     45
endHopState     EQU     46
#endif


ExecuteCmd  movlw   high ExecuteTable ; Load jump table address high byte ...
        movwf   PCLATH            ; ... into PCLATH to make jump in same code block
        movf    monCmd,W          ; Use command value ...
        andlw   cmdMask
        addwf   PCL,F             ; ... as offset into jump table

ExecuteTable    goto    BootVector        ; Reboot system
        goto    ReadReg
#ifdef EEDATA
        goto    ReadEeprom
#else
        goto    EscapeCmd         ; Command entry in table not being used
#endif
        goto    StopUser
        goto    ContUser
        goto    InitUser
        goto    IncRegister
        goto    DecRegister
#ifdef ADRES
        goto    ReadAnalogue
#else
        goto    EscapeCmd         ; Command entry in table not being used
#endif
        goto    WriteReg
#ifdef EEDATA
        goto    WriteEeprom
#else
        goto    EscapeCmd         ; Command entry in table not being used
#endif
        goto    AndReg
        goto    OrReg
        goto    XorReg
#ifdef LinkHTx
        goto    CancelHop
        goto    SetHop
#else
        goto    EscapeCmd         ; Command entry in table not being used
        goto    EscapeCmd         ; Command entry in table not being used
#endif

#if (high ExecuteTable) != (high $)
    error "ExecuteCmd state jump table split across page boundary"
#endif


CmdCmpltd   movlw   endCmdState
ForceState  btfsc   monState,userBit  ; If run user process bit is set ...
        iorlw   userMask          ; ... ensure it remains so
        movwf   monState

        goto    Monitor           ; Go directly to next state


EscapeCmd   ; Terminate the current command input sequence,
        ; send 'bell' to user and display prompt

        clrf    monReg
        decf    monReg,F
        clrf    monCmd
        clrf    monValue
        movlw   bellCode
        movwf   monData

        movlw   rawSendState
        goto    ForceState


AwaitingData    ; Wait for user input to arrive,
        ; testing for end of command, escape from command or cancel hop

        call    LinkMRx           ; Look for Rx data
        btfss   STATUS,Z          ; Skip if Rx data in W ...
        return                    ; ... otherwise return and keep waiting

        movwf   monData           ; Save Rx data in temporary register

        xorlw   execute           ; Test for end of user command input
        btfsc   STATUS,Z          ; Skip if not end of user command input ...
        goto    ExecuteCmd        ; ... otherwise execute user command

        movf    monData,W         ; Restore Rx data
        xorlw   escape            ; Test for escape user command
        btfsc   STATUS,Z          ; Skip if not escape of user command ...
        goto    EscapeCmd         ; ... otherwise escape user command
#ifdef LinkHTx
        movf    monData,W         ; Restore Rx data
        andlw   caseMask          ; If alpha ensure upper case
        xorlw   cancelHop         ; Test for 'cancel hop' command
        btfsc   STATUS,Z          ; Skip if not 'cancel hop' command ...
        goto    CancelHop         ; ... otherwise cancel hop mode
#endif
        goto    NextMonState


SendingUp   ; Loop until data sent to user

        movf    monData,W         ; Copy temporary register ...
SendUp      movwf   FSR               ; ... to FSR, for sending
        call    LinkMTx           ; Try to send data
        btfss   STATUS,Z          ; Skip if data sent ...
        return                    ; ... otherwise return and keep trying

        goto    NextMonState      ; Go directly to next state


SendingCR   ; Send 'carriage return' to user

        movlw   crCode            ; Load carriage return code
        goto    SendUp


SendingLF   ; Send 'line feed' to user

        movlw   lfCode            ; Load line feed code
        goto    SendUp


SendingPrompt   ; Send 'prompt' to user

        movlw   promptCode        ; Load prompt code
        goto    SendUp


#ifdef LinkHTx
AwaitingEcho    ; Loop here whilst in 'hop mode' waiting for 'echo' from 'down' side which
        ; is to be relayed 'up' to user, if data is received from 'up' side
        ; (from user) 'cancel hop'

        call    LinkMRx           ; Check for Rx data from 'up' side
        btfss   STATUS,Z          ; Skip if Rx data in W ...
        goto    NoUpBreak

        goto    CancelHop

NoUpBreak   call    LinkHRx           ; Check for Rx data from 'down' side
        btfss   STATUS,Z          ; Skip if Rx data in W ...
        return                    ; ... otherwise return and keep waiting

        goto    NextMonState      ; Next state = ForwardingUp


AwaitingForward ; Loop here whilst in 'hop' waiting for data from 'up' side (from user) which
        ; is to be relayed 'down', if data is received from 'down' side 'turn around'
        ; and relay data 'up' to user

        call    LinkHRx           ; Check for Rx data from 'down' side
        btfss   STATUS,Z          ; Skip if Rx data in W ...
        goto    NoDownData

        clrf    monReg            ; Clear for 'down' data received during relay
        movwf   monData           ; Save Rx data in temporary register
        movlw   hopUpState        ; Next state = ForwardingUp
        goto    ForceState

NoDownData  call    LinkMRx           ; Check for Rx data from 'up' side
        btfss   STATUS,Z          ; Skip if Rx data in W ...
        return                    ; ... otherwise return and keep waiting

        movwf   monData           ; Save Rx data in temporary register
        andlw   caseMask          ; If alpha ensure upper case
        xorlw   cancelHop         ; Test for 'cancel hop' command
        btfsc   STATUS,Z          ; Skip if not 'cancel hop' command ...
        goto    CancelHop

        movlw   hopDownState      ; Next state = ForwardingDown
        goto    ForceState


ForwardingUp    ; Loop here whilst in 'hop' trying to relay data to 'up' side (to user),
        ; if data is received from 'down' side whilst relaying save it and relay
        ; it once current data has been sent

        movf    monData,W         ; Copy temporary register ...
        movwf   FSR               ; ... to FSR, for sending
        call    LinkMTx           ; Try to send data to 'up' side
        btfss   STATUS,Z          ; Skip if data sent ...
        goto    UpBusy            ; ... otherwise return and keep trying

        movf    monReg,W          ; Check any any saved 'down' link data
        btfsc   STATUS,Z          ; Skip if it has ...
        goto    UpFinished        ; ... otherwise forwarding is finished

        movwf   monData           ; Retrieve saved 'down' link data
        clrf    monReg            ; Clear 'down' link data store

UpBusy      call    LinkHRx           ; Check for Rx data from 'down' side
        btfsc   STATUS,Z          ; Skip if no Rx data in W ...
        movwf   monReg            ; Store 'down' side Rx data

        return                    ; Stay in this state to continue relaying data 'up'

UpFinished  movlw   hopState          ; Next state = AwaitingForward
        goto    ForceState


SendingDown ; Loop here whilst in 'hop' sending data (not from user) to 'down' side,
        ; if data is received from 'up' side (from user) 'cancel hop'

        movf    monData,W         ; Copy temporary register ...
SendDown    movwf   FSR               ; ... to FSR, for sending
        call    LinkHTx           ; Try to send data to 'down' side
        btfsc   STATUS,Z          ; Skip if data not sent ...
        goto    NextMonState      ; Go directly to next state

        call    LinkMRx           ; Look for up Rx data
        btfss   STATUS,Z          ; Skip if Rx data in W ...
        return                    ; ... otherwise stay in this state to keep waiting

        clrf    monHop            ; Abandon 'hop' mode if 'interrupted' by up link
        goto    EscapeCmd


ForwardingCR    movlw   crCode            ; Send carriage return code to 'down' side
        goto    SendDown


SendingHop  movlw   setHop            ; Send 'hop' command code to 'down' side
        goto    SendDown


SendingUnhop    movlw   cancelHop         ; Send 'cancel hop' command code to 'down' side
        goto    SendDown


ForwardingDown  ; Loop here whilst in 'hop' trying to relay data to 'down' side

        movf    monData,W         ; Copy temporary register ...
        movwf   FSR               ; ... to FSR, for sending
        call    LinkHTx           ; Try to send data to 'down' side
        btfss   STATUS,Z          ; Skip if data sent ...
        return                    ; ... otherwise stay in this state to keep trying

        movlw   hopState          ; Next state = AwaitingForward
        goto    ForceState
#endif


DecodingCmd ; Determine which 'command' has been received from the user

        call    DecodeCmd
        movwf   monCmd            ; Store decoded command

        btfss   STATUS,Z          ; Skip if command was recognised ...
        goto    EscapeCmd         ; ... otherwise ignore command

        call    Cmd2State         ; Use command to determine next state
        goto    ForceState


DecodingRegDig1 call    DecodeDigit
        movf    monData,W         ; Store decoded digit ...
        movwf   monReg            ; ... as register (or EEPROM) address low nibble
        goto    NextMonState      ; Go directly to next state


DecodingRegDig2 movlw   delimt            ; Test if received character ...
        xorwf   monData,W         ; ... is the 'value delimiter' ...
        btfss   STATUS,Z          ; ... if so skip
        goto    Dig2Notdelimt

        movlw   valueState        ; Register (or EEPROM) address complete, seen 'delimiter' ...
        goto    ForceState        ; ... so get 'value' (always follows 'delimiter')

Dig2Notdelimt   call    DecodeDigit
        swapf   monReg,W          ; Combine register (or EEPROM) address low nibble ...
        iorwf   monData,W         ; ... with decoded digit ...
        movwf   monReg            ; ... to form complete register (or EEPROM) address
        goto    NextMonState      ; Go directly to next state


Chkng4delimt    movlw   delimt            ; Seen all digits for address, test if received character ...
        xorwf   monData,W         ; ... is the 'value delimiter' ...
        btfss   STATUS,Z          ; ... if so skip, ...
        goto    EscapeCmd         ; ... otherwise error in user command input

        goto    NextMonState      ; Go directly to next state


DecodingValDig1 call    DecodeDigit
        movf    monData,W         ; Store decoded digit ...
        movwf   monValue          ; ... as input value low nibble

        bsf     monCmd,setCmdBit  ; Alter command value to be 'set' instead of 'read'
        goto    NextMonState      ; Go directly to next state


DecodingValDig2 call    DecodeDigit
        swapf   monValue,W        ; Combine input value low nibble ...
        iorwf   monData,W         ; ... with decoded digit
        movwf   monValue          ; ... to form complete input value
        goto    NextMonState      ; Go directly to next state


DecodeDigit clrw                      ; Clear accumulator
        btfsc   monData,6         ; Skip if 'digit' is not in range 'A' to 'F' ...
        movlw   0x09              ; ... otherwise load accumulator with hex conversion value
        addwf   monData,W         ; Convert digit from alphanumeric character to hex 'nibble' value
        andlw   0x0F              ; Discard upper 'nibble' of byte
        movwf   monData           ; Store converted value
        return


#ifdef LinkHTx
EncodingNextHop movf    monHop,W          ; Set this chip's 'hop' level ...
        movwf   monValue          ; ... as data to send
        incf    monValue,F
        goto    EncodingDigit

EncodingHop movf    monHop,W          ; Set this node's 'hop' level ...
        movwf   monValue          ; ... as data to send
        btfss   STATUS,Z          ; Skip if this node is head of chain ...
        goto    EncodingDigit     ; ... otherwise display 'hop' level

        movlw   endPromptState    ; Head of chain display prompt without 'hop' level
        goto    ForceState
#endif

EncodingDigit   swapf   monValue,F        ; Operate on high 'nibble' then low 'nibble'
        movf    monValue,W        ; Load accumulator with ...
        andlw   0x0F              ; ... appropriate 'nibble' of byte
        iorlw   0x30              ; Convert value 0-9 to alphanumeric character

        sublw   0x39              ; Test if 'nibble' value ...
        btfss   STATUS,C          ; ... was greater than 9 (i.e. A-F) ...
        addlw   0xF9              ; ... if so adjust ...
        sublw   0x39              ; ... then complete conversion

        movwf   monData           ; Store alphanumeric character encoding of 'nibble' value
        goto    NextMonState      ; Go directly to next state


StopUser    bcf     monState,userBit  ; Clear 'perform user processing' bit
        goto    CmdCmpltd


ContUser    bsf     monState,userBit  ; Set 'perform user processing' bit
        goto    CmdCmpltd


InitUser    call    UserInit          ; Perform user 'initialisation'
        goto    CmdCmpltd


AddressRegister movf    monReg,W          ; Load register address ...
        movwf   FSR               ; ... into indirection select register
        movf    monValue,W        ; Load new value into W (overwritten in read operations)
        return


ReadReg     call    AddressRegister
ReadBackReg movf    INDF,W            ; Load register contents, indirectly, into W
        movwf   monValue          ; Store register contents from W
        movlw   responseState     ; Set monitor state so as to display register contents
        goto    ForceState


#ifdef ADRES
ReadAnalogue    movf    monReg,W          ; Load analogue channel to read into W
        call    SelectAD
        bsf     ADCON0,GO

        movlw   ADTRACKTIME
        movwf   monValue
WaitAnalogue    decfsz  monValue,F
        goto    WaitAnalogue

        call    GetAD
        movwf   monValue          ; Store analogue conversion value from W
        movlw   responseState     ; Set monitor state so as to display analogue conversion value
        goto    ForceState
#endif


#ifdef EEDATA
ReadEeprom
        movf    monReg,W          ; Load EEPROM location to read into W
        call    GetEEPROM
        movwf   monValue          ; Store EEPROM location value from W
        movlw   responseState     ; Set monitor state to display EEPROM value
        goto    ForceState
#endif


WriteReg
        call    AddressRegister
        movwf   INDF              ; Load register contents, indirectly, from W
        goto    CmdCmpltd


#ifdef EEDATA
WriteEeprom
        call    WaitEEPROM        ; Ensure no EEPROM write in progress

        movf    monReg,W          ; Set address of ...
        movwf   EEADR             ; ... EEPROM location
        movf    monValue,W        ; Load new value into W
        call    SetEEPROM
        goto    CmdCmpltd
#endif


IncRegister call    AddressRegister
        incf    INDF,F            ; Indirectly increment specified register
        goto    ReadBackReg


DecRegister call    AddressRegister
        decf    INDF,F            ; Indirectly decrement specified register
        goto    ReadBackReg


AndReg      call    AddressRegister
        andwf   INDF,F            ; Indirectly AND with specified register
        goto    ReadBackReg


OrReg       call    AddressRegister
        iorwf   INDF,F            ; Indirectly OR with specified register
        goto    ReadBackReg


XorReg      call    AddressRegister
        xorwf   INDF,F            ; Indirectly XOR with specified register
        goto    ReadBackReg


#ifdef LinkHTx
CancelHop   clrf    monHop
        movlw   endHopState
        goto    ForceState


SetHop      movf    monReg,W          ; Check for received 'hop' level value
        btfss   STATUS,Z          ; Skip if non received ...
        goto    SetHopLevel       ; ... otherwise set this node's 'hop' level

        movlw   beginHopState     ; No 'hop' level received so enter 'hop' mode to next node
        goto    ForceState

SetHopLevel movwf   monHop            ; Set node's 'hop' level to received value
        goto    CmdCmpltd
#endif


DecodeCmd   bcf     monData,caseBit   ; Ensure alpha command character is uppercase

        movlw   reboot
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdReboot

        movlw   register
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdRegister
#ifdef ADRES
        movlw   adRead
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdAdRead
#endif
#ifdef EEDATA
        movlw   eeprom
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdEeprom
#endif
        movlw   stopUser
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdStop

        movlw   contUser
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdContinue

        movlw   initUser
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdInit

        movlw   incReg
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdIncReg

        movlw   decReg
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdDecReg

        movlw   andReg
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdAndReg

        movlw   orReg
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdOrReg

        movlw   xorReg
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdXorReg
#ifdef LinkHTx
        movlw   cancelHop
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdCancelHop

        clrf    monReg            ; Set 'register' value to zero, if value is received ...
        movlw   setHop            ; ... then store value, otherwise enter hop mode
        xorwf   monData,W
        btfsc   STATUS,Z
        retlw   cmdSetHop
#endif
        retlw   0


Cmd2State   movlw   cmdStop
        xorwf   monCmd,W
        btfsc   STATUS,Z
        retlw   noValueState      ; Command may not have a 'value' supplied

        movlw   cmdContinue
        xorwf   monCmd,W
        btfsc   STATUS,Z
        retlw   noValueState      ; Command may not have a 'value' supplied

        movlw   cmdInit
        xorwf   monCmd,W
        btfsc   STATUS,Z
        retlw   noValueState      ; Command may not have a 'value' supplied

        movlw   cmdReboot
        xorwf   monCmd,W
        btfsc   STATUS,Z
        retlw   noValueState      ; Command may not have a 'value' supplied

        retlw   readSetState      ; Command may have a 'value' supplied


#ifdef ADRES
SelectAD    bsf     ADCON0,ADON       ; Turn on A/D converter
        bcf     ADCON0,CHS0       ; Select A/D ...
        bcf     ADCON0,CHS1       ; ... channel 0

        xorlw   0
        btfsc   STATUS,Z          ; Test if A/D channel 0 not to be selected ...
        return                    ; ... otherwise return

        addlw   0xFF              ; Decrement W
        btfsc   STATUS,Z          ; Test if A/D channel 1 not to be selected ...
        goto    SelectAD1         ; ... otherwise jump

        addlw   0xFF              ; Decrement W
        btfss   STATUS,Z          ; Test if A/D channel 2 to be selected ...
        bsf     ADCON0,CHS0       ; ... otherwise select A/D channel 3
        bsf     ADCON0,CHS1
        return

SelectAD1   bsf     ADCON0,CHS0
        return


GetAD       btfsc   ADCON0,GO
        goto    GetAD

        movf    ADRES,W
;       bcf     ADCON0,ADON
        return
#endif


#ifdef EEDATA
#include <\dev\projects\utility\pic\eeprom.inc>
#endif


#ifndef GOTUSERBANNER
#ifndef NOMONBANNER
;**********************************************************************
; Banner display code                                                 *
;**********************************************************************

TxBanner    movlw   crCode
        call    TxLoop
        movlw   lfCode
        call    TxLoop
        movlw   'B'
        call    TxLoop
        movlw   'o'
        call    TxLoop
        movlw   'd'
        call    TxLoop
        movlw   ' ' 
        call    TxLoop
        movlw   'M'
        call    TxLoop
        movlw   'o'
        call    TxLoop
        movlw   'n'
        call    TxLoop
        movlw   'i'
        call    TxLoop
        movlw   't'
        call    TxLoop
        movlw   'o'
        call    TxLoop
        movlw   'r'
        call    TxLoop
        movlw   bellCode
        call    TxLoop
        movlw   crCode
        call    TxLoop
        movlw   lfCode
        call    TxLoop
        movlw   lfCode

TxLoop      movwf   FSR               ; Copy W to FSR, for sending
LoopTx      call    LinkMTx           ; Try to send data
        btfss   STATUS,Z          ; Skip if data sent ...
        goto    LoopTx            ; ... otherwise keep trying to send

        return
#endif
#endif

;**********************************************************************
; Monitor main program code                                           *
;**********************************************************************

MonitorMain ; Initialise monitor
        clrf    monState
#ifdef MONUSERON
        bsf     monState,userBit  ; Set 'perform user processing' bit
#endif
        clrf    monData
        clrf    monReg
        decf    monReg,F
        clrf    monCmd
        clrf    monValue
#ifdef LinkHTx
        clrf    monHop
#endif

#ifdef GOTUSERBANNER
        call    UserBanner
        call    CmdCmpltd
#else
#ifndef NOMONBANNER
        call    TxBanner
        call    CmdCmpltd
#endif
#endif

MonitorLoop     ; Top of main processing loop

        call    Monitor

        btfsc   monState,userBit
        call    UserMain          ; Run user main loop code

        goto    MonitorLoop       ; End of main processing loop


        list

