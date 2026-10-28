/*
 * PROGRAMA 1: ESCRITOR EN COLA / MEMORIA DE TECLADO
 * Compilar: gcc -Wall -Wextra -pedantic prog1_escribir_cola.c -o prog1
 * Ejecutar: ./prog1
 *
 * Mejoras respecto a la guia:
 *  - Tabla real de scan-codes (set 1) en lugar de 0x1E + (c - 'A').
 *  - Las opciones 2 y 3 aceptan una palabra completa, no un solo caracter.
 *  - Se corrige el offset impreso en la opcion 3 (offset donde se escribio).
 *  - Funcion unica de insercion en la cola circular (sin codigo duplicado).
 *  - Mensaje claro si TIOCSTI esta deshabilitado en el kernel.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#define BDA_FILE    "/tmp/bios_keyboard_bda.bin"
#define BUF_INICIO  0x001E
#define BUF_ULTIMO  0x003C   /* offset de la ultima palabra del buffer */

#pragma pack(push, 1)
typedef struct {
    uint16_t head;            /* Offset 0x001A: puntero de lectura   */
    uint16_t tail;            /* Offset 0x001C: puntero de escritura */
    uint16_t buffer[16];      /* 0x001E..0x003D: [scan-code | ASCII] */
    uint8_t  port_60_data;    /* Puerto 0x60: dato                   */
    uint8_t  port_64_status;  /* Puerto 0x64: bit 0 = tecla lista    */
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

/* Scan-codes (set 1) para a..z */
static uint8_t scan_de(char c) {
    static const uint8_t letras[26] = {
        0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,
        0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C
    };
    if (isalpha((unsigned char)c)) return letras[tolower((unsigned char)c) - 'a'];
    if (c >= '1' && c <= '9')      return (uint8_t)(0x02 + (c - '1'));
    if (c == '0')                  return 0x0B;
    if (c == ' ')                  return 0x39;
    if (c == '\n')                 return 0x1C;
    return 0x00;
}

/* Inserta una palabra en la cola circular. Devuelve 0 si ok, 1 si overflow. */
static int insertar_en_cola(BiosKeyboardMemory *bda, char ascii, uint8_t scan,
                            uint16_t *offset_escrito) {
    uint16_t next_tail = (bda->tail >= BUF_ULTIMO) ? BUF_INICIO : bda->tail + 2;
    if (next_tail == bda->head) return 1;            /* cola llena */

    int idx = (bda->tail - BUF_INICIO) / 2;
    bda->buffer[idx] = ((uint16_t)scan << 8) | (uint8_t)ascii;
    *offset_escrito = bda->tail;
    bda->tail = next_tail;
    bda->port_60_data = scan;
    bda->port_64_status |= 0x01;
    return 0;
}

/* ---------- OPCION 1: inyeccion en la cola TTY del SO ---------- */
static void metodo_inyectado(const char *texto) {
    printf("\n--- [OPCION 1: INYECTANDO EN COLA DE ENTRADA TTY] ---\n");
#if defined(__unix__) || defined(__APPLE__)
    int fd = open("/dev/tty", O_RDWR);
    if (fd < 0) fd = STDIN_FILENO;
    int i = 0;
    for (; texto[i] != '\0'; i++) {
#ifdef TIOCSTI
        if (ioctl(fd, TIOCSTI, &texto[i]) < 0) {
            perror("Error en ioctl TIOCSTI");
            printf(">> En Linux >= 6.2 puede estar deshabilitado.\n"
                   "   Habilitar (solo practica): sudo sysctl dev.tty.legacy_tiocsti=1\n");
            if (fd != STDIN_FILENO) close(fd);
            return;
        }
#else
        printf("TIOCSTI no esta definido en este sistema.\n");
        break;
#endif
    }
    if (fd != STDIN_FILENO) close(fd);
    printf(">> Se inyectaron %d caracteres en la cola de entrada del terminal.\n", i);
#else
    (void)texto;
    printf("TIOCSTI solo esta disponible en entornos POSIX / Unix.\n");
#endif
}

/* ---------- OPCION 2: INT 16h servicio 05h ---------- */
static void metodo_asm_int16(const char *texto) {
    printf("\n--- [OPCION 2: EN ASM CON INT 16h (SERVICIO 05h)] ---\n");
    BiosKeyboardMemory bda;
    cargar_bda(&bda);

    size_t i;
for (i = 0; texto[i] != '\0'; i++) {
        char c = texto[i];
        if (c == '\n') continue;
        uint8_t sc = scan_de(c);
        uint16_t off;

        printf("\nCodigo x86 equivalente para '%c':\n", c);
        printf("  mov ah, 05h        ; insertar pulsacion en buffer\n");
        printf("  mov ch, 0x%02X       ; scan-code\n", sc);
        printf("  mov cl, 0x%02X       ; ASCII '%c'\n", (uint8_t)c, c);
        printf("  int 16h\n");

        if (insertar_en_cola(&bda, c, sc, &off)) {
            printf(">> [BIOS INT 16h] AL = 1: BUFFER OVERFLOW. Buffer lleno.\n");
            break;
        }
        printf(">> [BIOS INT 16h] Tecla '%c' insertada. AL = 0.\n", c);
    }
    guardar_bda(&bda);
}

/* ---------- OPCION 3: escritura directa en la BDA ---------- */
static void metodo_directo_memoria(const char *texto) {
    printf("\n--- [OPCION 3: DIRECTO A MEMORIA RAM (BDA 0x0040:0x001A)] ---\n");
    BiosKeyboardMemory bda;
    cargar_bda(&bda);

    size_t i;
for (i = 0; texto[i] != '\0'; i++) {
        char c = texto[i];
        if (c == '\n') continue;
        uint16_t off;
        if (insertar_en_cola(&bda, c, scan_de(c), &off)) {
            printf(">> [ERROR RAM] Buffer circular lleno en memoria fisica.\n");
            break;
        }
        printf(">> [RAM BDA] Palabra 0x%04X escrita en offset 0x%04X\n",
               bda.buffer[(off - BUF_INICIO) / 2], off);
    }
    guardar_bda(&bda);
    printf(">> [RAM BDA] Nuevo TAIL = 0x%04X | HEAD = 0x%04X\n", bda.tail, bda.head);
}

int main(void) {
    int opcion;
    char texto[100];

    printf("====================================================================\n");
    printf("   PROGRAMA 1: ESCRITOR EN COLA / MEMORIA DE TECLADO\n");
    printf("====================================================================\n");
    printf("Seleccione el metodo de escritura:\n");
    printf("  [1] INYECTADO (cola de entrada del SO via ioctl TIOCSTI)\n");
    printf("  [2] EN ASM CON INT 16h (Servicio 05h de la BIOS x86)\n");
    printf("  [3] DIRECTO A MEMORIA (Segmento 0x0040:0x001A de la BIOS en RAM)\n");
    printf("Ingrese opcion (1-3): ");
    if (scanf("%d", &opcion) != 1) return 1;
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }

    if (opcion < 1 || opcion > 3) { printf("Opcion no valida.\n"); return 1; }

    printf("\nIngrese texto o palabra: ");
    if (!fgets(texto, sizeof(texto), stdin)) return 1;

    switch (opcion) {
        case 1:
            if (texto[strlen(texto) - 1] != '\n' && strlen(texto) < sizeof(texto) - 1)
                strcat(texto, "\n");
            metodo_inyectado(texto);
            break;
        case 2: metodo_asm_int16(texto); break;
        case 3: metodo_directo_memoria(texto); break;
    }
    return 0;
}
