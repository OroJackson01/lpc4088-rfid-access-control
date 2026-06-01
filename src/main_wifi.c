/**
 * @file    main_wifi.c
 * @brief   Control de acceso RFID con verificacion Supabase via ESP32-C6.
 *
 * @author    Angel Lucas - Angel.lucasrubio@alum.uca.es
 * @date      2025/2026
 * @version   5.0
 *
 * @copyright GNU General Public License version 3 or later
 *
 * Secuencia de tests (similar al script Python PruebaDB.py):
 *   - Tarjetas activas   -> ACCESO CONCEDIDO + registro OK
 *   - Tarjeta inactiva   -> TARJETA INACTIVA + registro OK
 *   - Tarjeta inexistente-> NO ENCONTRADA    + registro OK
 *   Al terminar el ciclo muestra "-- Fin ciclo --" y repite.
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

/* ---- Configuracion de red ---------------------------------- */
/*
 * Red del laboratorio:
 *   #define WIFI_SSID   "************"
 *   #define WIFI_PASS   "************"
 */
#define WIFI_SSID   "************"
#define WIFI_PASS   "************"

/* ---- Pines UART -------------------------------------------- */
#define ESPAT_UART      LPC_UART2
#define ESPAT_BAUDRATE  UART_BAUDRATE_115200
#define ESPAT_PORT_TXD  PORT4
#define ESPAT_PIN_TXD   PIN22
#define ESPAT_PORT_RXD  PORT4
#define ESPAT_PIN_RXD   PIN23
#define ESPAT_TIMER     LPC_TIMER1

/* ---- LEDs -------------------------------------------------- */
#define LED_VERDE   LED2
#define LED_ROJO    LED1

/* ---- Tarjetas de prueba ------------------------------------ */
/*
 * Ajusta estos UIDs a los que tengas en tu tabla Supabase.
 * El ultimo debe ser NULL para marcar el fin del array.
 */
static const char *uids_prueba[] = {
    "A3:FF:12:04",   /* registrada -> activa   */
    "B1:22:33:44",   /* registrada -> activa   */
    "FF:FF:FF:FF",   /* no existe              */
    NULL
};

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
   LOGICA DE ACCESO
   ============================================================ */

/**
 * @brief   Verificar tarjeta y mostrar resultado en LCD + LEDs.
 * @retval  TRUE si acceso concedido.
 */
static bool verificar_y_mostrar(const char *uid)
{
    static char msg[40];
    bool concedido = FALSE;

    snprintf(msg, sizeof(msg), "UID:%.16s", uid);
    lcd_linea(1, msg);
    lcd_linea(2, "Consultando BD..");

    supa_res_t res = supabase_verificar_tarjeta(uid);

    switch (res) {
        case SUPA_ACTIVA:
            lcd_linea(2, "ACCESO CONCEDIDO");
            leds_encender(LED_VERDE);
            timer_retardo_ms(ESPAT_TIMER, 1500);
            leds_apagar(LED_VERDE);
            concedido = TRUE;
            break;
        case SUPA_INACTIVA:
            lcd_linea(2, "TARJETA INACTIVA");
            leds_encender(LED_ROJO);
            timer_retardo_ms(ESPAT_TIMER, 1500);
            leds_apagar(LED_ROJO);
            break;
        case SUPA_NO_EXISTE:
            lcd_linea(2, "NO ENCONTRADA   ");
            leds_encender(LED_ROJO);
            timer_retardo_ms(ESPAT_TIMER, 1500);
            leds_apagar(LED_ROJO);
            break;
        default:
            lcd_linea(2, "ERROR RED       ");
            leds_encender(LED_ROJO);
            timer_retardo_ms(ESPAT_TIMER, 1500);
            leds_apagar(LED_ROJO);
            break;
    }
    return concedido;
}

/**
 * @brief   Registrar acceso en Supabase y mostrar resultado.
 */
static void registrar_y_mostrar(const char *uid, bool concedido)
{
    lcd_linea(3, "Registrando...  ");

    supa_res_t res = supabase_registrar_acceso(uid, concedido);

    if (res == SUPA_OK) {
        lcd_linea(3, "Registro OK     ");
        leds_encender(LED_VERDE);
        timer_retardo_ms(ESPAT_TIMER, 800);
        leds_apagar(LED_VERDE);
    } else {
        lcd_linea(3, "Error registro  ");
        leds_encender(LED_ROJO);
        timer_retardo_ms(ESPAT_TIMER, 800);
        leds_apagar(LED_ROJO);
    }
}

/* ============================================================
   MAIN
   ============================================================ */
int main(void)
{
    glcd_inicializar();
    glcd_borrar(NEGRO);
    glcd_seleccionar_fuente(FUENTE8X16);
    leds_inicializar();

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

    /* -- 4.5 Ping ------------------------------------------ */
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

    /* -- 4.6 Configurar HTTP (SSL + apikey) ---------------- */
    lcd_linea(5, "Config HTTP...  ");
    if (!espat_inicializar_http()) {
        lcd_linea(5, "HTTP cfg ERROR  ");
        leds_encender(LED_ROJO);
        while (1);
    }
    lcd_linea(5, "HTTP cfg OK     ");
    timer_retardo_ms(ESPAT_TIMER, 500);

    lcd_limpiar();
    lcd_linea(0, "Sistema listo   ");
    timer_retardo_ms(ESPAT_TIMER, 1000);

    /* -- 5. Bucle principal -------------------------------- */
    uint16_t ciclo = 0;

    while (1) {
        for (int i = 0; uids_prueba[i] != NULL; i++) {
            lcd_limpiar();

            /* Cabecera con numero de ciclo y tarjeta */
            static char hdr[30];
            snprintf(hdr, sizeof(hdr), "Ciclo %u T%d     ", ciclo, i + 1);
            lcd_linea(0, hdr);

            bool concedido = verificar_y_mostrar(uids_prueba[i]);
            registrar_y_mostrar(uids_prueba[i], concedido);

            timer_retardo_ms(ESPAT_TIMER, 1000);
        }

        /* Fin de ciclo */
        lcd_limpiar();
        lcd_linea(0, "-- Fin ciclo -- ");
        static char ciclo_msg[30];
        snprintf(ciclo_msg, sizeof(ciclo_msg), "Ciclos OK: %u   ", ciclo + 1);
        lcd_linea(1, ciclo_msg);
        leds_encender(LED_VERDE);
        timer_retardo_ms(ESPAT_TIMER, 2000);
        leds_apagar(LED_VERDE);

        ciclo++;
        lcd_limpiar();
    }
}