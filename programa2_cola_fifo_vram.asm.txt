; ============================================================
; PROGRAMA 2: TERMINAL INTEGRADA (PUERTO + COLA FIFO + VRAM)
; ------------------------------------------------------------
; Reto integrador completo:
;   PRODUCTOR : lee Puerto 0x02 y encola en RAM 0x70..0x77
;   CONSUMIDOR: extrae de la cola y proyecta en la VRAM
; Incluye: cola circular, deteccion de overflow, Backspace
; y scroll vertical al llenar la pantalla 4x4.
; ============================================================

; --- Constantes ---
CURSOR      EQU 0x60        ; direccion actual del cursor (RAM de datos)
KB_COLA     EQU 0x70        ; base de la cola (8 bytes: 0x70..0x77)
KB_HEAD     EQU 0x78        ; indice de lectura
KB_TAIL     EQU 0x79        ; indice de escritura
KB_COUNT    EQU 0x7A        ; elementos en la cola
CAPACIDAD   EQU 8
MASCARA     EQU 7           ; (i + 1) mod 8  ==  (i + 1) AND 7
VRAM_INI    EQU 0xF0
VRAM_FIN    EQU 0xFF
ULTIMA_FILA EQU 0xFC        ; inicio de la fila 3
PORT_ESTADO EQU 0x01
PORT_DATOS  EQU 0x02
ASCII_BS    EQU 0x08
ASCII_ESP   EQU 0x20

; --- Inicializacion ---
INICIO:
    MOV   R0, 0
    STORE R0, [KB_HEAD]
    STORE R0, [KB_TAIL]
    STORE R0, [KB_COUNT]

    MOV   R1, VRAM_INI
    STORE R1, [CURSOR]

    MOV   R2, VRAM_INI
    MOV   R3, ASCII_ESP
    MOV   R4, 16
LIMPIAR:
    STORE R3, [R2]
    ADD   R2, 1
    SUB   R4, 1
    JNZ   LIMPIAR

; --- Ciclo principal ---
CICLO:
    CALL  PRODUCTOR             ; captura teclado -> cola
    CALL  CONSUMIDOR            ; cola -> pantalla
    JMP   CICLO

; ============================================================
; PRODUCTOR: Puerto 0x02 -> Cola FIFO
; ============================================================
PRODUCTOR:
    IN    R1, [PORT_ESTADO]
    CMP   R1, 0
    JZ    PROD_FIN              ; no hay tecla

    IN    R2, [PORT_DATOS]      ; leer SIEMPRE (limpia el puerto 0x01)

    LOAD  R3, [KB_COUNT]
    CMP   R3, CAPACIDAD
    JGE   PROD_FIN              ; cola llena -> descartar (overflow)

    LOAD  R4, [KB_TAIL]
    MOV   R5, R4
    ADD   R5, KB_COLA           ; direccion = 0x70 + TAIL
    STORE R2, [R5]              ; encolar

    ADD   R4, 1
    AND   R4, MASCARA           ; TAIL = (TAIL + 1) mod 8
    STORE R4, [KB_TAIL]

    ADD   R3, 1
    STORE R3, [KB_COUNT]        ; COUNT++
PROD_FIN:
    RET

; ============================================================
; CONSUMIDOR: Cola FIFO -> VRAM
; ============================================================
CONSUMIDOR:
    LOAD  R3, [KB_COUNT]
    CMP   R3, 0
    JZ    CONS_FIN              ; cola vacia

    LOAD  R4, [KB_HEAD]
    MOV   R5, R4
    ADD   R5, KB_COLA           ; direccion = 0x70 + HEAD
    LOAD  R2, [R5]              ; desencolar -> R2 = ASCII

    ADD   R4, 1
    AND   R4, MASCARA           ; HEAD = (HEAD + 1) mod 8
    STORE R4, [KB_HEAD]

    SUB   R3, 1
    STORE R3, [KB_COUNT]        ; COUNT--

    CMP   R2, ASCII_BS
    JZ    BACKSPACE

; --- Mostrar caracter ---
    LOAD  R6, [CURSOR]
    STORE R2, [R6]              ; escribir en VRAM
    CMP   R6, VRAM_FIN
    JZ    HACER_SCROLL          ; se lleno la pantalla
    ADD   R6, 1
    STORE R6, [CURSOR]
    JMP   CONS_FIN

; --- Backspace ---
BACKSPACE:
    LOAD  R6, [CURSOR]
    CMP   R6, VRAM_INI
    JZ    CONS_FIN              ; cursor en 0xF0: no retrocede
    SUB   R6, 1
    STORE R6, [CURSOR]
    MOV   R1, ASCII_ESP
    STORE R1, [R6]              ; borrar visualmente
    JMP   CONS_FIN

; --- Scroll vertical: filas 1..3 suben una fila ---
HACER_SCROLL:
    MOV   R7, VRAM_INI
    MOV   R4, 12                ; 12 celdas a copiar
SC_COPIA:
    MOV   R5, R7
    ADD   R5, 4                 ; celda de la fila de abajo
    LOAD  R1, [R5]
    STORE R1, [R7]              ; sobreescribir celda superior
    ADD   R7, 1
    SUB   R4, 1
    JNZ   SC_COPIA

    MOV   R1, ASCII_ESP         ; limpiar ultima fila (R7 = 0xFC)
    MOV   R4, 4
SC_LIMPIA:
    STORE R1, [R7]
    ADD   R7, 1
    SUB   R4, 1
    JNZ   SC_LIMPIA

    MOV   R6, ULTIMA_FILA
    STORE R6, [CURSOR]          ; cursor al inicio de la ultima fila
CONS_FIN:
    RET
