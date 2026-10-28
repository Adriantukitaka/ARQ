/*
 * PROGRAMA 2: LECTOR MULTI-METODO DE TECLADO
 * Compilar: gcc -Wall -Wextra -pedantic prog2_leer_teclado.c -o prog2
 * Ejecutar: ./prog2
 *
 * Mejoras respecto a la guia:
 *  - Codigo limpio y formateado (el original llegaba sin espacios).
 *  - Una sola rutina de extraccion de la cola (usada por las opciones 4 y 5).
 *  - Lectura por puerto: respeta el handshake (leer 0x60 limpia el bit de 0x64).
 *  - Muestra la tecla de forma legible aunque sea un caracter no imprimible.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#define BDA_FILE    "/tmp/bios_keyboard_bda.bin"
#define BUF_INICIO  0x001E
#define BUF_ULTIMO  0x003C

#pragma pack(push, 1)
typedef struct {
    uint16_t head;
    uint16_t tail;
    uint16_t buffer[16];
    uint8_t  port_60_data;
    uint8_t  port_64_status;
} BiosKeyboardMemory;
#pragma pack(pop)

static void iniciar_bda(BiosKeyboardMemory *bda) {
    memset(bda, 0, sizeof(*bda));
    bda->head = BUF_INICIO;
    bda->tail = BUF_INICIO;
}

static void cargar_bda(BiosKeyboardMemory *bda) {
    FILE *f = fopen(BDA_FILE, "rb");
    if (!f) { iniciar_bda(bda); return; }
    if (fread(bda, sizeof(*bda), 1, f) != 1) iniciar_bda(bda);
    fclose(f);
}

static void guardar_bda(const BiosKeyboardMemory *bda) {
    FILE *f = fopen(BDA_FILE, "wb");
    if (!f) { perror("No se pudo guardar la BDA"); return; }
    fwrite(bda, sizeof(*bda), 1, f);
    fclose(f);
}

static char imprimible(uint8_t c) { return (c >= 32 && c <= 126) ? (char)c : '.'; }

/* Extrae una palabra de la cola. Devuelve 0 si ok, 1 si esta vacia. */
static int extraer_de_cola(BiosKeyboardMemory *bda, uint16_t *palabra) {
    if (bda->head == bda->tail) return 1;
    int idx = (bda->head - BUF_INICIO) / 2;
    *palabra = bda->buffer[idx];
    bda->head = (bda->head >= BUF_ULTIMO) ? BUF_INICIO : bda->head + 2;
    return 0;
}

/* ---------- OPCION 1 ---------- */
static void lectura_con_scanf(void) {
    char palabra[100];
    printf("\n--- [OPCION 1: LECTURA CON scanf()] ---\n");
    printf(">> Ingrese o espere entrada: ");
    if (scanf("%99s", palabra) == 1) {
        printf("\n======================================================\n");
        printf("[RESULTADO scanf]: \"%s\"\n", palabra);
        printf("======================================================\n");
    }
}

/* ---------- OPCION 2 ---------- */
static void lectura_con_getc(void) {
    printf("\n--- [OPCION 2: LECTURA CON getc(stdin)] ---\n");
    printf(">> Ingrese o espere un caracter: ");
    int c = getc(stdin);
    if (c != EOF) {
        printf("\n======================================================\n");
        printf("[RESULTADO getc]: '%c' | ASCII: %d (0x%02X)\n",
               imprimible((uint8_t)c), c, (uint8_t)c);
        printf("======================================================\n");
    }
}

/* ---------- OPCION 3 ---------- */
static void lectura_desde_puerto(void) {
    printf("\n--- [OPCION 3: LECTURA DESDE PUERTO DE HARDWARE (0x60)] ---\n");
    printf("Instruccion fisica x86: in al, 60h\n");

    BiosKeyboardMemory bda;
    cargar_bda(&bda);
    printf(">> Estado del Puerto 0x64 (Status): 0x%02X\n", bda.port_64_status);

    if ((bda.port_64_status & 0x01) == 0) {
        printf(">> [PUERTO 0x60] No hay datos pendientes en el puerto.\n");
        return;
    }
    uint8_t scan_code = bda.port_60_data;
    bda.port_64_status &= (uint8_t)~0x01;   /* handshake: leer limpia el bit */
    guardar_bda(&bda);

    printf("======================================================\n");
    printf("[RESULTADO PUERTO 0x60]: Scan-code = 0x%02X\n", scan_code);
    printf(">> Estado tras la lectura (0x64): 0x%02X\n", bda.port_64_status);
    printf("======================================================\n");
}

/* ---------- OPCION 4 ---------- */
static void lectura_directa_memoria(void) {
    printf("\n--- [OPCION 4: LECTURA DIRECTA DESDE MEMORIA RAM (BDA)] ---\n");
    BiosKeyboardMemory bda;
    cargar_bda(&bda);
    printf(">> HEAD: 0x%04X | TAIL: 0x%04X\n", bda.head, bda.tail);

    uint16_t palabra;
    if (extraer_de_cola(&bda, &palabra)) {
        printf(">> [RAM] Buffer vacio en memoria (HEAD == TAIL).\n");
        return;
    }
    guardar_bda(&bda);

    printf("======================================================\n");
    printf("[RESULTADO DIRECTO A MEMORIA RAM]:\n");
    printf("Palabra de 16 bits en celda: 0x%04X\n", palabra);
    printf("-> Byte Bajo (ASCII)     : '%c' (0x%02X)\n",
           imprimible((uint8_t)(palabra & 0xFF)), palabra & 0xFF);
    printf("-> Byte Alto (Scan-Code) : 0x%02X\n", palabra >> 8);
    printf("-> Nuevo puntero HEAD    : 0x%04X\n", bda.head);
    printf("======================================================\n");
}

/* ---------- OPCION 5 ---------- */
static void lectura_con_int16(void) {
    printf("\n--- [OPCION 5: CON INT 16h (SERVICIO 00h EN ASM)] ---\n");
    printf("Instruccion fisica x86: mov ah, 00h ; int 16h\n");

    BiosKeyboardMemory bda;
    cargar_bda(&bda);

    uint16_t palabra;
    if (extraer_de_cola(&bda, &palabra)) {
        printf(">> [BIOS INT 16h] Buffer actualmente vacio.\n");
        return;
    }
    guardar_bda(&bda);

    printf("======================================================\n");
    printf(" [RESULTADO REGISTROS DE CPU TRAS INT 16h]:\n");
    printf("   Registro AL (ASCII)    : '%c' (0x%02X)\n",
           imprimible((uint8_t)(palabra & 0xFF)), palabra & 0xFF);
    printf("   Registro AH (Scan-code): 0x%02X\n", palabra >> 8);
    printf("   Registro AX completo   : 0x%04X\n", palabra);
    printf("======================================================\n");
}

int main(void) {
    int opcion;

    printf("====================================================================\n");
    printf("   PROGRAMA 2: LECTOR DESDE TECLADO / COLA / MEMORIA / PUERTOS\n");
    printf("====================================================================\n");
    printf("Seleccione el metodo de lectura:\n");
    printf("  [1] CON scanf() (palabra formateada por stdin)\n");
    printf("  [2] CON getc() (caracter por caracter por stdin)\n");
    printf("  [3] LEYENDO DESDE EL PUERTO (0x60 / IN AL, 60h)\n");
    printf("  [4] LEYENDO DIRECTO DESDE LA MEMORIA (BDA 0x0040:0x001A)\n");
    printf("  [5] CON LA INT 16h (Servicio 00h de la BIOS en ASM)\n");
    printf("Ingrese opcion (1-5): ");
    if (scanf("%d", &opcion) != 1) return 1;
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }

    switch (opcion) {
        case 1: lectura_con_scanf(); break;
        case 2: lectura_con_getc(); break;
        case 3: lectura_desde_puerto(); break;
        case 4: lectura_directa_memoria(); break;
        case 5: lectura_con_int16(); break;
        default: printf("Opcion invalida.\n"); return 1;
    }
    return 0;
}
