; ============================================================
; PROGRAMA 1: TERMINAL BASICA POR SONDEO DIRECTO (SIN COLA)
; ------------------------------------------------------------
; Lee la tecla del Puerto 0x02 y la proyecta de inmediato en
; la VRAM. Soporta Backspace (0x08). No usa cola FIFO.
; Si la CPU tarda en volver al sondeo, las teclas se pierden.
; ============================================================

; --- Constantes ---
CURSOR      EQU 0x60        ; variable en RAM de datos: direccion actual del cursor
VRAM_INI    EQU 0xF0        ; primera celda de la VRAM
VRAM_FIN    EQU 0xFF        ; ultima celda de la VRAM
PORT_ESTADO EQU 0x01        ; Bit 0 = 1 si hay tecla
PORT_DATOS  EQU 0x02        ; Codigo ASCII (al leer, limpia el estado)
ASCII_BS    EQU 0x08
ASCII_ESP   EQU 0x20

; --- Inicializacion ---
INICIO:
    MOV   R1, VRAM_INI
    STORE R1, [CURSOR]          ; cursor = 0xF0

    MOV   R2, VRAM_INI          ; puntero de limpieza
    MOV   R3, ASCII_ESP         ; caracter espacio
    MOV   R4, 16                ; 16 celdas
LIMPIAR:
    STORE R3, [R2]
    ADD   R2, 1
    SUB   R4, 1
    JNZ   LIMPIAR

; --- Bucle principal: sondeo por puerto (E/S aislada) ---
ESPERAR_TECLA:
    IN    R1, [PORT_ESTADO]     ; IO_READ al puerto 0x01
    CMP   R1, 0
    JZ    ESPERAR_TECLA         ; sin tecla -> seguir esperando

    IN    R2, [PORT_DATOS]      ; IO_READ al puerto 0x02 -> R2 = ASCII
                                ; el hardware pone el estado en 0

    CMP   R2, ASCII_BS
    JZ    BORRAR                ; es Backspace

; --- Eco del caracter en la VRAM ---
MOSTRAR:
    LOAD  R6, [CURSOR]
    STORE R2, [R6]              ; escribe en la celda del cursor
    CMP   R6, VRAM_FIN
    JZ    ENVOLVER              ; fin de pantalla -> volver al inicio
    ADD   R6, 1
    STORE R6, [CURSOR]
    JMP   ESPERAR_TECLA

ENVOLVER:
    MOV   R6, VRAM_INI          ; sin scroll: el cursor vuelve a 0xF0
    STORE R6, [CURSOR]
    JMP   ESPERAR_TECLA

; --- Backspace ---
BORRAR:
    LOAD  R6, [CURSOR]
    CMP   R6, VRAM_INI
    JZ    ESPERAR_TECLA         ; ya esta en 0xF0, no retrocede
    SUB   R6, 1
    STORE R6, [CURSOR]
    MOV   R1, ASCII_ESP
    STORE R1, [R6]              ; borra visualmente con un espacio
    JMP   ESPERAR_TECLA
