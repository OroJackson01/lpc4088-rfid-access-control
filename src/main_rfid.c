/**
 * @file    main_rfid.c
 * @brief   Prueba de las funciones de manejo del lector de tarjetas RFID MFRC522.
 *
 * @author  Alejandro Lara - alejandro.lara@uca.es
 * @date    2026
 * @version 2.0
 *
 * @copyright GNU General Public License version 3 or later
 *
 * =====================================================================
 * CONEXIÓN HARDWARE  (conector SSP2 de la LPC4088 Developer's Kit)
 * =====================================================================
 *
 *   RC522 pin     Cable      LPC4088 pin    Función SSP2
 *  ─────────────────────────────────────────────────────────────────
 *   SDA  (NSS)    Gris      P2[14]          GPIO manual (CS, funcion_cs=FALSE)
 *   SCK           Lila      P5[2]           SSP2_SCK
 *   MOSI          Azul      P5[0]           SSP2_MOSI
 *   MISO          Verde     P5[1]           SSP2_MISO
 *   GND           Amarillo  GND             —
 *   3.3V          Naranja   3V3             —
 *   RQ            —         (no conectar)   —
 *   RST           —         (no conectar)   —
 *  ─────────────────────────────────────────────────────────────────
 *
 * =====================================================================
 * FUNCIONAMIENTO
 * =====================================================================
 *   El programa inicializa el RC522 y espera en bucle a que se acerque
 *   una tarjeta MIFARE. Al detectarla imprime su UID de 4 bytes en la
 *   pantalla LCD en formato hexadecimal (p. ej. "A3FF1204").
 *
 *   La función mfrc522_esperar_detectar_tarjeta() es bloqueante:
 *   no avanza hasta que hay una tarjeta válida en el campo RF.
 *   Una vez leído el UID, la tarjeta pasa a estado HALT (no responde
 *   a nuevos REQA hasta que se retire y vuelva a acercar).
 * =====================================================================
 *
 *	Tarjeta Blanca: 93:FC:3F:19 y llavero Azul: A3:3A:C7:0D
 */

#include <LPC407x_8x_177x_8x.h>
#include "tipos.h"
#include "glcd.h"
#include "mfrc522_lpc40xx.h"

/* ============================================================
   Definiciones de pines  (deben coincidir con la llamada a
   mfrc522_inicializar más abajo)
   ============================================================ */

/* SSP2 — pines de bus SPI */
#define RFID_SSP        LPC_SSP2   /* Interfaz SSP a usar               */
#define RFID_PORT_SCK   PORT5      /* P5[2] → SSP2_SCK  (cable lila)    */
#define RFID_PIN_SCK    PIN2
#define RFID_PORT_MISO  PORT5      /* P5[1] → SSP2_MISO (cable verde)   */
#define RFID_PIN_MISO   PIN1
#define RFID_PORT_MOSI  PORT5      /* P5[0] → SSP2_MOSI (cable azul)    */
#define RFID_PIN_MOSI   PIN0

/* CS gestionado por GPIO (SDA del RC522 = NSS) */
#define RFID_PORT_CS    PORT2      /* P2[14] → CS       (cable gris)    */
#define RFID_PIN_CS     PIN14
#define RFID_FUNC_CS    FALSE      /* FALSE = GPIO manual, no función HW */

/* Frecuencia de reloj SPI (el RC522 admite hasta 10 MHz) */
#define RFID_FREC_SCK   1000000u   /* 1 MHz — valor conservador          */

/* Timer para retardos internos del driver MFRC522 */
#define RFID_TIMER      LPC_TIMER3

/* ============================================================
   Función auxiliar
   ============================================================ */

/**
 * @brief   Mostrar el UID de la tarjeta en la pantalla LCD.
 *
 * @details Imprime los 4 bytes del UID en hexadecimal, sin separadores,
 * usando fuente 16×32 para facilitar la lectura.
 * Ejemplo de salida: "A3FF1204"
 *
 * @param[in]   numero_serie  Array con los 4 bytes del UID.
 */
static void imprimir_numero_serie_tarjeta(const uint8_t *numero_serie) {
  glcd_xy_texto(0, 0);
  glcd_seleccionar_fuente(FUENTE12X24);
  // Formato XX:XX:XX:XX
	glcd_printf("%02X:%02X:%02X:%02X",
            numero_serie[0], numero_serie[1],
            numero_serie[2], numero_serie[3]);
}

/* ============================================================
   Main
   ============================================================ */

int main(void) {
  uint8_t uid[5];  /* 4 bytes UID + 1 byte BCC */

  /* Inicializar la pantalla LCD */
  glcd_inicializar();
  glcd_borrar(NEGRO);
  glcd_color_texto(BLANCO);
  glcd_fondo_texto(NEGRO);

  /* Mostrar mensaje de espera inicial */
  glcd_xy_texto(0, 0);
  glcd_seleccionar_fuente(FUENTE8X16);
  glcd_printf("Acerque tarjeta...");

  /* Inicializar el lector RFID RC522:
   *   - SSP2 a 1 MHz
   *   - SCK  en P5[2], MISO en P5[1], MOSI en P5[0]
   *   - CS   en P2[14] como GPIO ordinario (funcion_cs = FALSE)
   *   - TIMER3 para retardos internos del driver
   */
  mfrc522_inicializar(RFID_SSP,       RFID_FREC_SCK,
                      RFID_PORT_SCK,  RFID_PIN_SCK,
                      RFID_PORT_MISO, RFID_PIN_MISO,
                      RFID_PORT_MOSI, RFID_PIN_MOSI,
                      RFID_PORT_CS,   RFID_PIN_CS,
                      RFID_FUNC_CS,   RFID_TIMER);

  while (1) {
    /* Esperar (bloqueante) a que se acerque una tarjeta y leer su UID */
    mfrc522_esperar_detectar_tarjeta(uid);

    /* Limpiar zona de texto y mostrar el UID */
    glcd_borrar(NEGRO);
    imprimir_numero_serie_tarjeta(uid);
  }
}