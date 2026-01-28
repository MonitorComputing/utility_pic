		nolist
#ifndef    MATH_INC
#define    MATH_INC

;**********************************************************************
;                                                                     *
; Description: Arithmetic helper macros.                              *
;                                                                     *
; Author: Chris White (whitecf69@gmail.com)                           *
;                                                                     *
; Copyright (C) 2011 by Monitor Computing Services Limited, licensed  *
; under CC BY-NC-SA 4.0. To view a copy of this license, visit        *
; https://creativecommons.org/licenses/by-nc-sa/4.0/                  *
;                                                                     *
; This program is distributed in the hope that it will be useful, but *
; WITHOUT ANY WARRANTY; without even the implied warranty of          *
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
;                                                                     *
;**********************************************************************


;**********************************************************************
;                                                                     *
; Macro:     Mul8x8                                                   *
;                                                                     *
;            8 bit by 8 bit multiply giving 16 bit result             *
;                                                                     *
; Arguments: W register - 8 bit multiplicand - undamaged              *
;            Mulplr     - 8 bit multiplier   - destroyed              *
;            ResLow     - Result low byte                             *
;            ResHigh    - Result high byte                            *
;                                                                     *
;**********************************************************************
Mul8x8		macro   Mulplr, ResLow, ResHigh

		local   Loop, EndMul

		; Clear result
		clrf    ResHigh
		clrf    ResLow

		; Set carry bit, rotated into multiplier and ensures 8 loop iterations
		bsf     STATUS,C

Loop		; Rotate multiplier 1 bit right, carry -> bit 7, bit 0 -> carry
		rrf     Mulplr,F

		; Test if multiplier now == 0, if so multiply is finished
		movf    Mulplr,F
		btfsc   STATUS,Z
		goto    EndMul

		; Test carry, if set add multiplicand to result high byte
		btfsc   STATUS,C
		addwf   ResHigh,F

		; Rotate result right (/ by 2), as ResLow was cleared this also clears carry
		rrf     ResHigh,F
		rrf     ResLow,F

		goto    Loop

EndMul

		endm


;**********************************************************************
;                                                                     *
; Macro:     Mul16x8                                                  *
;                                                                     *
;            16 bit by 8 bit multiply giving 24 bit result            *
;            NOTE - contents of W are destroyed                       *
;                                                                     *
; Arguments: Mulplr   - 8 bit multiplier              - destroyed     *
;            MulpLow  - 16 bit multiplicand low byte  - undamaged     *
;            MulpHigh - 16 bit multiplicand high byte - undamaged     *
;            ResLow   - Result low byte                               *
;            ResMid   - Result mid byte                               *
;            ResHigh  - Result high byte                              *
;                                                                     *
;**********************************************************************
Mul16x8		macro   Mulplr, MulpLow, MulpHigh, ResLow, ResMid, ResHigh

		local   Loop, LoopMul, EndMul

		; Clear result
		clrf    ResHigh
		clrf    ResMid
		clrf    ResLow

		; Set carry bit, this is rotated into multiplier and ensures 8 loop iterations
		bsf     STATUS,C

Loop		; Rotate multiplier 1 bit right, carry -> bit 7, bit 0 -> carry
		rrf     Mulplr,F

		; Test if multiplier now == 0, if so multiply is finished
		movf    Mulplr,F
		btfsc   STATUS,Z
		goto    EndMul

		; Test carry, if set add multiplicand to result high byte
		btfss   STATUS,C
		goto    LoopMul

		Add16   MulpLow, MulpHigh, ResMid, ResHigh

		; Rotate result right (/ by 2), as ResLow was cleared this also clears carry
LoopMul		rrf     ResHigh,F
		rrf     ResMid,F
		rrf     ResLow,F

		goto    Loop

EndMul

		endm


;**********************************************************************
;                                                                     *
; Macro:     Mul16x8R                                                 *
;                                                                     *
;            16 bit by 8 bit multiply restricted to 16 bit result     *
;            NOTE - contents of W are destroyed                       *
;                                                                     *
; Arguments: Mulplr   - 8 bit multiplier              - destroyed     *
;            MulpLow  - 16 bit multiplicand low byte  - undamaged     *
;            MulpHigh - 16 bit multiplicand high byte - undamaged     *
;            ResLow   - Result low byte                               *
;            ResMid   - Result mid byte                               *
;            ResHigh  - Result high byte              - cleared       *
;                                                                     *
;**********************************************************************
Mul16x8R	macro   Mulplr, MulpLow, MulpHigh, ResLow, ResMid, ResHigh

		local   NoOverflow

		Mul16x8 Mulplr, MulpLow, MulpHigh, ResLow, ResMid, ResHigh

		movf    ResHigh,F
		btfsc   STATUS,Z
		goto    NoOverflow

		movlw   0xFF
		movwf   ResLow
		movwf   ResMid
		clrf    ResHigh

NoOverflow

		endm


;**********************************************************************
;                                                                     *
; Macro:     Add16                                                    *
;                                                                     *
;            16 bit add giving 16 bit result                          *
;            NOTE - contents of W are destroyed                       *
;                   STATUS,C returns carry status                     *
;                                                                     *
; Arguments: AddLow     - 1st Number low byte  - undamaged            *
;            AddHigh    - 1st Number high byte - undamaged            *
;            ResLow     - 2nd Number low byte  - set to result        *
;            ResHigh    - 2nd Number high byte - set to result        *
;                                                                     *
;**********************************************************************
Add16		macro   AddLow, AddHigh, ResLow, ResHigh

		local   HighAdd, EndAdd

		; Add low bytes
		movf    AddLow,W
		addwf   ResLow,F

		movf    AddHigh,W
		btfss   STATUS,C
		; No carry generated by low byte addition, simply add high bytes
		goto    HighAdd

		; Carry generated by low byte addition, add to high bytes
		addlw   1
		btfsc   STATUS,C
		; Low byte carry has caused AddHigh to overflow to zero,
                ; leave ResHigh unaltered and carry flag set
		goto    EndAdd

HighAdd		; Add high bytes
                addwf   ResHigh,F

EndAdd

		endm


;**********************************************************************
;                                                                     *
; Macro:     Add24                                                    *
;                                                                     *
;            24 bit add giving 24 bit result                          *
;            NOTE - contents of W are destroyed                       *
;                   STATUS,C returns carry status                     *
;                                                                     *
; Arguments: AddLow     - 1st Number low byte  - undamaged            *
;            AddMid     - 1st Number mid byte  - undamaged            *
;            AddHigh    - 1st Number high byte - undamaged            *
;            ResLow     - 2nd Number low byte  - set to result        *
;            ResMid     - 2nd Number mid byte  - set to result        *
;            ResHigh    - 2nd Number high byte - set to result        *
;                                                                     *
;**********************************************************************
Add24		macro   AddLow, AddMid, AddHigh, ResLow, ResMid, ResHigh

		local   HighAdd, EndAdd

		; Add low word
		Add16   AddLow, AddMid, ResLow, ResMid

		movf    AddHigh,W
		btfss   STATUS,C
		; No carry generated by low byte addition, simply add high bytes
		goto    HighAdd

		; Carry generated by low byte addition, add to high bytes
		addlw   1
		btfsc   STATUS,C
		; Low byte carry has caused AddHigh to overflow to zero,
                ; leave ResHigh unaltered and carry flag set
		goto    EndAdd

HighAdd		; Add high bytes
                addwf   ResHigh,F

EndAdd

		endm


;**********************************************************************
;                                                                     *
; Macro:     Sub16                                                    *
;                                                                     *
;            16 bit subtract giving 16 bit result                     *
;            NOTE - contents of W are destroyed                       *
;                   STATUS,C returns 0 -ve result, 1 +ve result       *
;                                                                     *
; Arguments: SubLow     - 1st Number low byte  - undamaged            *
;            SubHigh    - 1st Number high byte - undamaged            *
;            ResLow     - 2nd Number low byte  - set to result        *
;            ResHigh    - 2nd Number high byte - set to result        *
;                                                                     *
;**********************************************************************
Sub16		macro   SubLow, SubHigh, ResLow, ResHigh

		local   HighSub, EndSub

		; Subtract low bytes
		movf    SubLow,W
		subwf   ResLow,F

		movf    SubHigh,W
		btfsc   STATUS,C
		; Carry set - result positive, simply subtract high bytes
		goto    HighSub

		; Low byte needs to borrow from high bytes
		addlw   1
		btfss   STATUS,C
		goto    HighSub

		; Low byte borrow has caused SubHigh to overflow to zero,
                ; leave ResHigh unaltered and carry flag set (= -ve result)
		bcf     STATUS,C
		goto    EndSub

HighSub		; Subtract high bytes
                subwf   ResHigh,F

EndSub

		endm


;**********************************************************************
;                                                                     *
; Macro:     Sub24                                                    *
;                                                                     *
;            24 bit subtract giving 24 bit result                     *
;            NOTE - contents of W are destroyed                       *
;                   STATUS,C returns 0 -ve result, 1 +ve result       *
;                                                                     *
; Arguments: SubLow     - 1st Number low byte  - undamaged            *
;            SubMid     - 1st Number mid byte  - undamaged            *
;            SubHigh    - 1st Number high byte - undamaged            *
;            ResLow     - 2nd Number low byte  - set to result        *
;            ResMid     - 2nd Number mid byte  - set to result        *
;            ResHigh    - 2nd Number high byte - set to result        *
;                                                                     *
;**********************************************************************
Sub24		macro   SubLow, SubMid, SubHigh, ResLow, ResMid, ResHigh

		local   HighSub, EndSub

		; Subtract low word
		Sub16   SubLow, SubMid, ResLow, ResMid

		movf    SubHigh,W
		btfsc   STATUS,C
		; Carry set - result positive, simply subtract high bytes
		goto    HighSub

		; Low word needs to borrow from high bytes
		addlw   1
		btfss   STATUS,C
		goto    HighSub

		; Low word borrow has caused SubHigh to overflow to zero,
                ; leave ResHigh unaltered and carry flag set (= -ve result)
		bcf     STATUS,C
		goto    EndSub

HighSub		; Subtract high bytes
                subwf   ResHigh,F

EndSub

		endm


;**********************************************************************
;                                                                     *
; Macro:     Int8x16                                                  *
;                                                                     *
;            Interpolate 8 bit value using over 16 discrete ranges    *
;            giving a 12 bit result                                   *
;            NOTE - contents of W are destroyed                       *
;                                                                     *
; Arguments: Value      - value to interpolate     - undamaged        *
;            TempAcc8   - temporary accumulator    - destroyed        *
;            IntTable   - interpolation table base - undamaged        *
;            TempAccL   - temporary low byte       - destroyed        *
;            TempAccH   - temporary high byte      - destroyed        *
;            ResLow     - 2nd temporary low byte   - set to result    *
;            ResHigh    - 2nd temporary high byte  - set to result    *
;                                                                     *
;**********************************************************************
Int8x16		macro   Value, TempAcc8, IntTable, TempAccL, TempAccH, ResLow, ResHigh

		swapf   Value,W            ; Get high nibble of value ...
		andlw   0x0F               ; ... into low nibble, isolate, ...
		addlw   LOW IntTable       ; ... add to table start ...
		movwf   FSR                ; ... load as indirect address

		movf    INDF,W             ; Get lower ...
		movwf   TempAcc8           ; ... range bound

		movf    Value,W            ; Get value ...
		andlw   0x0F               ; ... low nibble ...
		sublw   0x10               ; ... and subtract it from 16

		; Multiply: W       = 16 - value low nibble
		;       by: TempAcc8 = range lower bound
		Mul8x8  TempAcc8, ResLow, ResHigh

		incf    FSR,F              ; Increment indirect address
		movf    INDF,W             ; Get upper ...
		movwf   TempAcc8           ; ... range bound

		movf    Value,W            ; Get value ...
		andlw   0x0F               ; ... low nibble

		; Multiply: W       = value low nibble
		;       by: TempAcc8 = range upper bound
		Mul8x8  TempAcc8, TempAccL, TempAccH

		; Add range upper and lower range contributions
		Add16   TempAccL, TempAccH, ResLow, ResHigh

		endm


;**********************************************************************
;                                                                     *
; Macro:     Div24x8                                                  *
;                                                                     *
;            24 bit by 8 bit multiply giving 24 bit result            *
;            NOTE - contents of W are destroyed                       *
;                                                                     *
; Arguments: W        - 8 bit divisor                 - undamaged     *
;            Counter  - Division loop counter         - destroyed     *
;            TempAcc  - Temporary register            - destroyed     *
;            DivLow   - Dividend (and result) low byte                *
;            DivMid   - Dividend (and result) mid byte                *
;            DivHigh  - Dividend (and result) high byte               *
;                                                                     *
;**********************************************************************
Div24x8		macro   Counter, TempAcc, DivLow, DivMid, DivHigh

		local   Loop, Itterate

		clrf    TempAcc

		clrf    Counter            ; Set counter for 25 itterations
		bsf     Counter,4
		bsf     Counter,3
		bsf     Counter,0


Loop		; Rotate dividend 1 bit left, carry -> bit 0, bit 24 -> carry
		rlf     DivLow,F
		rlf     DivMid,F
		rlf     DivHigh,F

		rlf     TempAcc,F

		subwf   TempAcc,F
		btfsc   STATUS,C
		goto    Itterate

		addwf   TempAcc,F
		bcf     STATUS,C

Itterate	decfsz  Counter,F
		goto    Loop

		endm

#endif
		list
