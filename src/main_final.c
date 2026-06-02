/**
 * @file    main_final.c
 * @brief   Control de acceso RFID real con verificacion Supabase.
 *
 *          Integra los dos subsistemas del proyecto:
 *            - MFRC522 (lectura de UID por SPI/SSP2)
 *            - ESP32-C6 (WiFi + Supabase REST por UART2)
 *
 * @author    Angel Lucas - Angel.lucasrubio@alum.uca.es
 * @date      2025/2026
 * @version   1.0
 *
 * @copyright GNU General Public License version 3 or later
 *
 * =====================================================================
 * FLUJO DEL SISTEMA
 * =====================================================================
 *   ARRANQUE (una sola vez):
 *     1. Inicializar LCD y LEDs.
 *     2. Inicializar ESP -> AT -> modo Station -> WiFi -> Ping.
 *     3. Configurar HTTP global (SSL + cabeceras apikey/Authorization).
 *     4. Inicializar el lector MFRC522.
 *
 *   BUCLE PRINCIPAL (por cada tarjeta acercada):
 *     a. Esperar (bloqueante) a que se detecte una tarjeta y leer su UID.
 *     b. Formatear el UID a "XX:XX:XX:XX" (igual que en Supabase).
 *     c. GET  -> supabase_verificar_tarjeta()
 *               ACTIVA   -> ACCESO CONCEDIDO (LED verde)
 *               INACTIVA -> ACCESO DENEGADO  (LED rojo)
 *               NO EXISTE-> TARJETA DESCONOCIDA (LED rojo)
 *               ERROR    -> ERROR DE RED (LED rojo)
 *     d. POST -> supabase_registrar_acceso() (deja traza del intento).
 *
 * =====================================================================
 * RECURSOS HARDWARE  (sin conflictos entre subsistemas)
 * =====================================================================
 *   ESP32-C6   : UART2 (P4[22] TXD, P4[23] RXD) + TIMER1
 *   MFRC522    : SSP2  (P5[2] SCK, P5[1] MISO, P5[0] MOSI) + CS P2[14] + TIMER3
 *   LEDs       : LED1 rojo (P1[5]), LED2 verde (P0[14])
 * =====================================================================
 */

#include <LPC407x_8x_177x_8x.h>
#include <stdlib.h>
#include <string.h>
#include "tipos.h"
#include "error.h"
#include "glcd.h"
#include "timer_lpc40xx.h"
#include "leds.h"
#include "espat_lpc40xx.h"
#include "mfrc522_lpc40xx.h"

/* ============================================================
   CONFIGURACION
   ============================================================ */

/* ---- Red WiFi ---------------------------------------------- */
/*
 * Red del laboratorio:
 *   #define WIFI_SSID   "Cudy-B1F8"
 *   #define WIFI_PASS   "XXXXXXXXX"
 */
#define WIFI_SSID   "XXXXXXXX"
#define WIFI_PASS   "XXXXXXXX"

/* ---- ESP-AT (UART2) ---------------------------------------- */
#define ESPAT_UART      LPC_UART2
#define ESPAT_BAUDRATE  UART_BAUDRATE_115200
#define ESPAT_PORT_TXD  PORT4
#define ESPAT_PIN_TXD   PIN22
#define ESPAT_PORT_RXD  PORT4
#define ESPAT_PIN_RXD   PIN23
#define ESPAT_TIMER     LPC_TIMER1

/* ---- MFRC522 (SSP2) ---------------------------------------- */
#define RFID_SSP        LPC_SSP2
#define RFID_PORT_SCK   PORT5
#define RFID_PIN_SCK    PIN2
#define RFID_PORT_MISO  PORT5
#define RFID_PIN_MISO   PIN1
#define RFID_PORT_MOSI  PORT5
#define RFID_PIN_MOSI   PIN0
#define RFID_PORT_CS    PORT2
#define RFID_PIN_CS     PIN14
#define RFID_FUNC_CS    FALSE       /* CS por GPIO manual */
#define RFID_FREC_SCK   1000000u    /* 1 MHz (conservador) */
#define RFID_TIMER      LPC_TIMER3

/* ---- LEDs -------------------------------------------------- */
#define LED_VERDE   LED2
#define LED_ROJO    LED1

/* ============================================================
   HELPERS LCD
   ============================================================ */

static void lcd_linea(uint8_t fila, const char *texto)
{
    glcd_rectangulo_relleno(0,   (fila * 16) + 2,
                            479, (fila * 16) + 17,
                            NEGRO);
    glcd_xy_texto(0, (fila * 16) + 2);
    glcd_color_texto(BLANCO);
    glcd_fondo_texto(NEGRO);
    glcd_printf("%s", texto);
}

static void lcd_limpiar(void)
{
    for (uint8_t f = 0; f < 6; f++)
        lcd_linea(f, "                              ");
}

/* ============================================================
   UTILIDADES
   ============================================================ */

/**
 * @brief   Convertir los 4 bytes del UID al formato de Supabase.
 *
 *          El lector entrega bytes crudos (p.ej. 0xA3 0xFF 0x12 0x04);
 *          en la tabla 'tarjetas' el campo uid es texto "A3:FF:12:04".
 *          Es IMPRESCINDIBLE usar mayusculas y ':' o el GET no casara.
 *
 * @param[in]   uid       Array de >=4 bytes con el UID leido.
 * @param[out]  dst       Buffer destino (>=12 bytes: "XX:XX:XX:XX\0").
 * @param[in]   dst_size  Tamano del buffer destino.
 */
static void uid_a_cadena(const uint8_t *uid, char *dst, uint32_t dst_size)
{
    snprintf(dst, dst_size, "%02X:%02X:%02X:%02X",
             uid[0], uid[1], uid[2], uid[3]);
}

/* ============================================================
   LOGICA DE ACCESO
   ============================================================ */

/**
 * @brief   Verificar la tarjeta en Supabase y mostrar el resultado.
 * @param[in]   uid_str  UID ya formateado "XX:XX:XX:XX".
 * @retval  TRUE si el acceso queda concedido.
 */
static bool verificar_y_mostrar(const char *uid_str)
{
    bool concedido = FALSE;

    lcd_linea(2, "Consultando BD..");

    supa_res_t res = supabase_verificar_tarjeta(uid_str);

    switch (res) {
        case SUPA_ACTIVA:
            lcd_linea(2, "ACCESO CONCEDIDO");
            leds_encender(LED_VERDE);
            timer_retardo_ms(ESPAT_TIMER, 2000);
            leds_apagar(LED_VERDE);
            concedido = TRUE;
            break;
        case SUPA_INACTIVA:
            lcd_linea(2, "TARJETA INACTIVA");
            leds_encender(LED_ROJO);
            timer_retardo_ms(ESPAT_TIMER, 2000);
            leds_apagar(LED_ROJO);
            break;
        case SUPA_NO_EXISTE:
            lcd_linea(2, "DESCONOCIDA     ");
            leds_encender(LED_ROJO);
            timer_retardo_ms(ESPAT_TIMER, 2000);
            leds_apagar(LED_ROJO);
            break;
        default:  /* SUPA_ERR */
            lcd_linea(2, "ERROR DE RED    ");
            leds_encender(LED_ROJO);
            timer_retardo_ms(ESPAT_TIMER, 2000);
            leds_apagar(LED_ROJO);
            break;
    }
    return concedido;
}

/**
 * @brief   Registrar el intento de acceso en Supabase.
 */
static void registrar_y_mostrar(const char *uid_str, bool concedido)
{
    lcd_linea(3, "Registrando...  ");

    supa_res_t res = supabase_registrar_acceso(uid_str, concedido);

    if (res == SUPA_OK) {
        lcd_linea(3, "Registro OK     ");
    } else {
        lcd_linea(3, "Error registro  ");
    }
}

/* ============================================================
   ARRANQUE DE SUBSISTEMAS
   ============================================================ */

/**
 * @brief   Inicializar ESP, conectar WiFi y configurar HTTP.
 *          Si algo falla, queda bloqueado mostrando el error (LED rojo).
 */
static void arrancar_red(void)
{
    /* -- 1. Inicializar ESP -------------------------------- */
    lcd_linea(0, "Iniciando ESP...");
    espat_inicializar(ESPAT_UART,    ESPAT_BAUDRATE,
                      ESPAT_PORT_TXD, ESPAT_PIN_TXD,
                      ESPAT_PORT_RXD, ESPAT_PIN_RXD,
                      ESPAT_TIMER);

    /* -- 2. Comprobar AT ----------------------------------- */
    lcd_linea(1, "Comprobando AT..");
    if (!espat_comprobar_conexion(5000)) {
        lcd_linea(1, "ESPAT: sin resp.");
        lcd_linea(2, "Revisa conexion ");
        leds_encender(LED_ROJO);
        while (1);
    }
    lcd_linea(1, "AT OK           ");
    timer_retardo_ms(ESPAT_TIMER, 400);

    /* -- 3. Modo Station ----------------------------------- */
    lcd_linea(2, "Modo Station... ");
    espat_wifi_establecer_modo(1);
    lcd_linea(2, "Modo OK         ");
    timer_retardo_ms(ESPAT_TIMER, 300);

    /* -- 4. Conectar WiFi ---------------------------------- */
    lcd_linea(3, "Conectando WiFi.");
    espat_wifi_conectar_ap(WIFI_SSID, WIFI_PASS);
    lcd_linea(3, "WiFi OK         ");
    timer_retardo_ms(ESPAT_TIMER, 800);

    /* -- 5. Ping ------------------------------------------- */
    lcd_linea(4, "Ping 8.8.8.8... ");
    uint32_t ping_ms = espat_wifi_ping("8.8.8.8");
    if (ping_ms == UINT32_MAX) {
        lcd_linea(4, "Sin internet!   ");
        leds_encender(LED_ROJO);
        while (1);
    }
    static char ping_msg[30];
    snprintf(ping_msg, sizeof(ping_msg), "Ping OK: %u ms  ", ping_ms);
    lcd_linea(4, ping_msg);
    timer_retardo_ms(ESPAT_TIMER, 500);

    /* -- 6. Configurar HTTP (SSL + apikey) ----------------- */
    lcd_linea(5, "Config HTTP...  ");
    if (!espat_inicializar_http()) {
        lcd_linea(5, "HTTP cfg ERROR  ");
        leds_encender(LED_ROJO);
        while (1);
    }
    lcd_linea(5, "HTTP cfg OK     ");
    timer_retardo_ms(ESPAT_TIMER, 500);
}

/* ============================================================
   MAIN
   ============================================================ */
int main(void)
{
    uint8_t uid[5];          /* 4 bytes UID + 1 byte BCC */
    static char uid_str[16]; /* "XX:XX:XX:XX" + margen */

    /* -- Perifericos basicos ------------------------------- */
    glcd_inicializar();
    glcd_borrar(NEGRO);
    glcd_seleccionar_fuente(FUENTE8X16);
    leds_inicializar();

    /* -- Arrancar red (ESP + WiFi + HTTP) ------------------ */
    arrancar_red();

    /* -- Arrancar lector RFID ------------------------------ */
    /*
     * Se inicializa DESPUES de la red: usa SSP2 + TIMER3, recursos
     * independientes del ESP (UART2 + TIMER1), por lo que el orden
     * no genera conflictos.
     */
    mfrc522_inicializar(RFID_SSP,       RFID_FREC_SCK,
                        RFID_PORT_SCK,  RFID_PIN_SCK,
                        RFID_PORT_MISO, RFID_PIN_MISO,
                        RFID_PORT_MOSI, RFID_PIN_MOSI,
                        RFID_PORT_CS,   RFID_PIN_CS,
                        RFID_FUNC_CS,   RFID_TIMER);

    lcd_limpiar();
    lcd_linea(0, "Sistema listo   ");
    timer_retardo_ms(ESPAT_TIMER, 1000);

    /* -- Bucle principal: control de acceso real ----------- */
    uint16_t total = 0;

    while (1) {
        lcd_limpiar();
        lcd_linea(0, "Acerque tarjeta ");

        /* Bloqueante: no avanza hasta detectar una tarjeta valida.
         * Tras leerla, la tarjeta pasa a HALT, asi que no se vuelve
         * a leer hasta que se retira y se vuelve a acercar. */
        mfrc522_esperar_detectar_tarjeta(uid);

        /* Formatear UID al estilo de la tabla 'tarjetas' */
        uid_a_cadena(uid, uid_str, sizeof(uid_str));

        static char hdr[30];
        snprintf(hdr, sizeof(hdr), "UID: %s", uid_str);
        lcd_linea(0, hdr);

        /* GET: verificar + POST: registrar */
        bool concedido = verificar_y_mostrar(uid_str);
        registrar_y_mostrar(uid_str, concedido);

        /* Contador de lecturas (diagnostico) */
        total++;
        static char cont[30];
        snprintf(cont, sizeof(cont), "Lecturas: %u   ", total);
        lcd_linea(4, cont);

        /* Margen para retirar la tarjeta antes de la siguiente lectura */
        timer_retardo_ms(ESPAT_TIMER, 1500);
    }
}