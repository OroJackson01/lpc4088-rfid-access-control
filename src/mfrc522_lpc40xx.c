/**
 * @file    mfrc522_lpc40xx.c
 * @brief   Funciones para acceder al lector RFID MFRC522 en modo SPI a través de una interfaz SSP
 * del LPC40xx.
 *
 * @author
 * @date
 * @version
 *
 * @copyright GNU General Public License version 3 or later
 *
 * Conexión hardware (según main_rfid.c):
 *   SSP2  SCK  → P5[2]  (lila)
 *   SSP2  MISO → P5[1]  (verde)
 *   SSP2  MOSI → P5[0]  (azul)
 *   GPIO  CS   → P2[14] (gris)   funcion_cs = FALSE
 *   GND        →  GND   (amarillo)
 *   3V3        →  3V3   (naranja)
 */

#include "mfrc522_lpc40xx.h"
#include <LPC407x_8x_177x_8x.h>
#include <string.h>
#include "tipos.h"
#include "error.h"
#include "gpio_lpc40xx.h"
#include "iocon_lpc40xx.h"
#include "spi_lpc40xx.h"
#include "timer_lpc40xx.h"

/* ============================================================
   Variables privadas del módulo
   ============================================================ */
static LPC_SSP_TypeDef   *mfrc522_ssp;            //!< Interfaz SSP a usar
static LPC_GPIO_TypeDef  *mfrc522_puerto_cs;       //!< Puerto para la señal /CS
static uint32_t           mfrc522_mascara_pin_cs;  //!< Máscara de pin para la señal /CS
static bool_t             mfrc522_funcion_cs;      //!< TRUE si el pin tiene función hardware SSEL
static LPC_TIM_TypeDef   *mfrc522_timer;           //!< Timer para retardos

/* ============================================================
   Constantes para mfrc522_detectar_tarjeta_con_escritura
   ============================================================
   TRAILER_BLOCK_HARDCODED: bloque trailer del sector 0.
     En MIFARE Classic 1K los sectores tienen 4 bloques (0-3, 4-7, ...).
     El bloque trailer (con las claves) es siempre el último del sector:
       sector 0 → trailer = bloque 3
       sector 1 → trailer = bloque 7, etc.
     Cambia este valor al sector que quieras autenticar.

   clave_A_sector: clave A por defecto de fábrica de MIFARE Classic (FF FF FF FF FF FF).
     Modifícala si tus tarjetas tienen una clave personalizada.
   ============================================================ */
#define TRAILER_BLOCK_HARDCODED   3u   /*!< Trailer del sector 0 (bloque 3) */

static const uint8_t clave_A_sector[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* ============================================================
   Prototipos de funciones privadas
   ============================================================ */
static void    mfrc522_seleccionar_esclavo(bool_t nivel);
static void    mfrc522_escribir_registro(uint8_t registro, uint8_t dato);
static uint8_t mfrc522_leer_registro(uint8_t registro);
static void    mfrc522_activar_bits(uint8_t registro, uint8_t mascara);
static void    mfrc522_borrar_bits(uint8_t registro, uint8_t mascara);

/* ============================================================
   Inicialización
   ============================================================ */

/**
 * @brief   Inicializa la comunicación SPI entre el uC y el módulo MFRC522.
 *
 * @param[in]   interfaz_ssp      Ptr. a registros de la interfaz SSP a usar (p. ej. LPC_SSP2).
 * @param[in]   frecuencia_sck    Frecuencia de reloj SCK en Hz (máx. 10 MHz para el RC522).
 * @param[in]   puerto_sck        Puerto GPIO del pin SCK.
 * @param[in]   mascara_pin_sck   Máscara del pin SCK.
 * @param[in]   puerto_miso       Puerto GPIO del pin MISO.
 * @param[in]   mascara_pin_miso  Máscara del pin MISO.
 * @param[in]   puerto_mosi       Puerto GPIO del pin MOSI.
 * @param[in]   mascara_pin_mosi  Máscara del pin MOSI.
 * @param[in]   puerto_cs         Puerto GPIO del pin CS (/NSS).
 * @param[in]   mascara_pin_cs    Máscara del pin CS.
 * @param[in]   funcion_cs        TRUE  → el pin CS tiene función hardware SSEL del SSP.
 *                                FALSE → el pin CS es un GPIO ordinario (caso habitual con RC522).
 * @param[in]   timer             Ptr. a registros del timer para retardos (se inicializa aquí
 *                                cuando funcion_cs = FALSE).
 */
void mfrc522_inicializar(LPC_SSP_TypeDef  *interfaz_ssp, uint32_t frecuencia_sck,
                         LPC_GPIO_TypeDef *puerto_sck,   uint32_t mascara_pin_sck,
                         LPC_GPIO_TypeDef *puerto_miso,  uint32_t mascara_pin_miso,
                         LPC_GPIO_TypeDef *puerto_mosi,  uint32_t mascara_pin_mosi,
                         LPC_GPIO_TypeDef *puerto_cs,    uint32_t mascara_pin_cs,
                         bool_t funcion_cs, LPC_TIM_TypeDef *timer) {

  /* Inicializar el interfaz SPI/SSP:
   *   - 8 bits de datos
   *   - CPOL = 0, CPHA = 0  (RC522 muestrea en flanco de subida, modo SPI 0)
   *   - El SSP gestiona SSEL solo si funcion_cs = TRUE; si es FALSE lo gestionamos
   *     nosotros a mano desde mfrc522_seleccionar_esclavo().
   */
  spi_inicializar(interfaz_ssp, SPI_DATOS_8_BITS, frecuencia_sck, SPI_CPOL_0, SPI_CPHA_0,
                  puerto_sck,  mascara_pin_sck,
                  puerto_miso, mascara_pin_miso,
                  puerto_mosi, mascara_pin_mosi,
                  puerto_cs,   mascara_pin_cs,
                  funcion_cs);

  /* Guardar parámetros para uso en el resto de funciones del módulo */
  mfrc522_ssp           = interfaz_ssp;
  mfrc522_puerto_cs     = puerto_cs;
  mfrc522_mascara_pin_cs = mascara_pin_cs;
  mfrc522_funcion_cs    = funcion_cs;
  mfrc522_timer         = timer;

  /* Cuando CS es GPIO manual inicializamos el timer para poder hacer
   * retardos de microsegundos entre el flanco de CS y los pulsos de SCK */
  if (!funcion_cs) {
    timer_inicializar(timer);
    /* Asegurar que el CS empieza en alto (esclavo desactivado) */
    gpio_ajustar_dir(puerto_cs, mascara_pin_cs, DIR_SALIDA);
    gpio_pin_a_1(puerto_cs, mascara_pin_cs);
  }

  /* ---- Secuencia de inicialización del RC522 ---- */

  /* Reset por software: pone todos los registros a su valor por defecto */
  mfrc522_escribir_registro(MFRC522_COMMANDREG, MFRC522_SOFTRESET);

  /* Esperar ~37 µs que tarda el oscilador en estabilizarse tras el reset
   * (td = 1024 / 27.12 MHz ≈ 37.7 µs, ver datasheet sección 8.8.2) */
  timer_retardo_us(mfrc522_timer, 50);

  /* TModeReg:
   *   TAuto = 1         → el timer se inicia automáticamente al final de cada transmisión
   *   TPrescaler_Hi = 0x0D → parte alta del prescaler (junto con 0x3E forma 0xD3E = 3390)
   *   Con TPrescaler = 0xD3E y TReload = 0x0030:
   *     ftimer = 13.56 MHz / (2*3390 + 1) ≈ 2 kHz  → periodo ~500 µs
   *     tiempo espera = (0x0030 + 1) * 500 µs ≈ 24.5 ms  (más que suficiente para ISO14443)
   */
  mfrc522_escribir_registro(MFRC522_TMODEREG,     (1u << 7) | 0x0D);
  mfrc522_escribir_registro(MFRC522_TPRESCALERREG, 0x3E);

  /* TReloadReg: valor de recarga de 16 bits = 0x0030 */
  mfrc522_escribir_registro(MFRC522_TRELOADREGH, 0x00);
  mfrc522_escribir_registro(MFRC522_TRELOADREGL, 0x30);

  /* RFCfgReg: RxGain = 111b → ganancia del receptor máxima (48 dB) */
  mfrc522_escribir_registro(MFRC522_RFCFGREG, (7u << 4));

  /* TxASKReg: Force100ASK = 1 → fuerza modulación ASK al 100 % */
  mfrc522_escribir_registro(MFRC522_TXASKREG, (1u << 6));

  /* ModeReg:
   *   TxWaitRF  = 1  → el transmisor solo arranca si hay campo RF
   *   PolMFin   = 1  → pin MFIN activo a nivel alto
   *   CRCPreset = 01 → preset del coprocesador CRC = 0x6363 (ISO/IEC 14443)
   */
  mfrc522_escribir_registro(MFRC522_MODEREG, (1u << 5) | (1u << 3) | 0x01);

  /* Activar la antena (pines TX1 y TX2) */
  mfrc522_antena_on();
}

/* ============================================================
   Operaciones sobre el MFRC522
   ============================================================ */

/**
 * @brief   Activar la antena (habilitar TX1 y TX2).
 */
void mfrc522_antena_on(void) {
  uint8_t valor = mfrc522_leer_registro(MFRC522_TXCONTROLREG);
  if (!(valor & 0x03)) {
    mfrc522_activar_bits(MFRC522_TXCONTROLREG, 0x03);
  }
}

/**
 * @brief   Desactivar la antena (deshabilitar TX1 y TX2).
 */
void mfrc522_antena_off(void) {
  mfrc522_borrar_bits(MFRC522_TXCONTROLREG, 0x03);
}

/**
 * @brief   Ejecutar un comando Transceive (transmisión + recepción automática).
 *
 * @details Carga los bytes de p_buffer_tx en la FIFO, ejecuta el comando TRANSCEIVE,
 * espera a que finalice (timeout ~200 iteraciones × retardo interno del SSP) y lee
 * los bytes recibidos de la FIFO.
 *
 * @param[in]   p_buffer_tx    Bytes a transmitir (máx. 64).
 * @param[in]   tam_buffer_tx  Número de bytes a transmitir.
 * @param[out]  p_buffer_rx    Buffer de recepción (mín. 64 bytes recomendado).
 * @param[out]  bytes_rx       Número de bytes recibidos.
 *
 * @retval  TRUE   Comunicación correcta, sin errores.
 * @retval  FALSE  Timeout o error de comunicación.
 */
bool_t mfrc522_comando_transceive(uint8_t *p_buffer_tx, uint32_t tam_buffer_tx,
                                  uint8_t *p_buffer_rx, uint32_t *bytes_rx) {
  uint8_t  commirqreg, error, recibidos;
  uint16_t i;

  *bytes_rx = 0;

  /* Preparar el RC522 para la operación */
  mfrc522_escribir_registro(MFRC522_COMMANDREG,  MFRC522_IDLE);
  mfrc522_escribir_registro(MFRC522_COMMIENREG,  0xF7);          /* Habilitar todas las IRQ */
  mfrc522_escribir_registro(MFRC522_COMMIRQREG,  0x80);          /* Limpiar flags de IRQ    */
  mfrc522_escribir_registro(MFRC522_FIFOLEVELREG, MFRC522_BIT_VACIARFIFO); /* Vaciar FIFO  */

  /* Cargar datos de transmisión en la FIFO */
  for (i = 0; i < (uint16_t)tam_buffer_tx; i++) {
    mfrc522_escribir_registro(MFRC522_FIFODATAREG, p_buffer_tx[i]);
  }

  /* Arrancar el comando TRANSCEIVE y activar el bit StartSend */
  mfrc522_escribir_registro(MFRC522_COMMANDREG, MFRC522_TRANSCEIVE);
  mfrc522_activar_bits(MFRC522_BITFRAMINGREG, 0x80);

  /* Esperar a que finalice: TimerIRq (bit 0) = timeout,  RxIRq|IdleIRq (bits 5,4) = datos recibidos */
  i = 2000;
  do {
    commirqreg = mfrc522_leer_registro(MFRC522_COMMIRQREG);
    i--;
  } while ((i != 0) && !(commirqreg & 0x01) && !(commirqreg & 0x30));

  /* Desactivar StartSend independientemente del resultado */
  mfrc522_borrar_bits(MFRC522_BITFRAMINGREG, 0x80);

  if (i == 0) {
    return FALSE;  /* Timeout: el RC522 no respondió a tiempo */
  }

  /* Comprobar errores en ERRORREG (máscara 0x1B = ProtocolErr|ParityErr|CRCErr|CollErr) */
  error = mfrc522_leer_registro(MFRC522_ERRORREG);
  if ((error & 0x1B) != 0) {
    return FALSE;
  }

  /* Pequeño retardo para que los bytes se estabilicen en la FIFO */
  timer_retardo_ms(mfrc522_timer, 1);

  /* Leer cuántos bytes hay en la FIFO y extraerlos */
  recibidos = mfrc522_leer_registro(MFRC522_FIFOLEVELREG);
  for (i = 0; i < recibidos; i++) {
    p_buffer_rx[i] = mfrc522_leer_registro(MFRC522_FIFODATAREG);
  }
  *bytes_rx = recibidos;

  return TRUE;
}

/**
 * @brief   Calcular el CRC de un bloque de datos usando el coprocesador interno del RC522.
 *
 * @param[in]   p_buffer_entrada  Datos de entrada (máx. 64 bytes).
 * @param[in]   tam_buffer        Número de bytes de entrada.
 * @param[out]  p_crc             Array de 2 bytes donde se almacena [CRC_LSB, CRC_MSB].
 *
 * @retval  TRUE   CRC calculado correctamente.
 * @retval  FALSE  Timeout.
 */
bool_t mfrc522_comando_calccrc(uint8_t *p_buffer_entrada, uint32_t tam_buffer, uint8_t *p_crc) {
  uint8_t  i, divirqreg;

  /* Limpiar el flag CRCIRQ y vaciar la FIFO */
  mfrc522_borrar_bits(MFRC522_DIVIRQREG,    MFRC522_BIT_CRCIRQ);
  mfrc522_escribir_registro(MFRC522_FIFOLEVELREG, MFRC522_BIT_VACIARFIFO);

  /* Cargar los datos en la FIFO */
  for (i = 0; i < (uint8_t)tam_buffer; i++) {
    mfrc522_escribir_registro(MFRC522_FIFODATAREG, p_buffer_entrada[i]);
  }

  /* Arrancar el cálculo */
  mfrc522_escribir_registro(MFRC522_COMMANDREG, MFRC522_CALCCRC);

  /* Esperar hasta que CRCIRQ se active */
  i = 0xFF;
  do {
    divirqreg = mfrc522_leer_registro(MFRC522_DIVIRQREG);
    i--;
  } while ((i != 0) && !(divirqreg & MFRC522_BIT_CRCIRQ));

  if (i == 0) {
    return FALSE;  /* Timeout */
  }

  /* Leer resultado: LSB primero, MSB segundo */
  p_crc[0] = mfrc522_leer_registro(MFRC522_CRCRESULTREGL);
  p_crc[1] = mfrc522_leer_registro(MFRC522_CRCRESULTREGH);

  return TRUE;
}

/**
 * @brief   Ejecutar autentificación MIFARE Classic (comando MFAuthent).
 *
 * @param[in]   modo              PICC_AUTHENTA (0x60) o PICC_AUTHENTB (0x61).
 * @param[in]   direccion_bloque  Dirección del trailer block del sector a autenticar.
 * @param[in]   p_clave_sector    Puntero a los 6 bytes de la clave del sector.
 * @param[in]   p_uid             Puntero a los 4 bytes del UID de la tarjeta.
 *
 * @retval  TRUE   Autenticación exitosa (bit CRYPTO1ON activado en STATUS2REG).
 * @retval  FALSE  Fallo o timeout.
 *
 * @note ¡Función no comprobada!
 */
bool_t mfrc522_comando_mfauthent(uint8_t modo, uint8_t direccion_bloque,
                                 const uint8_t *p_clave_sector, uint8_t *p_uid) {
  uint8_t status2, i, commirq_reg, command_reg, intentos = 0, fifo_buffer[12];

  /* Construir los 12 bytes del FIFO: modo + bloque + clave (6) + UID (4) */
  fifo_buffer[0] = modo;
  fifo_buffer[1] = direccion_bloque;
  memcpy(&fifo_buffer[2], p_clave_sector, 6);
  memcpy(&fifo_buffer[8], p_uid, 4);

  /* Vaciar FIFO y cargar los datos */
  mfrc522_escribir_registro(MFRC522_FIFOLEVELREG, 0x80);
  for (i = 0; i < 12; i++) {
    mfrc522_escribir_registro(MFRC522_FIFODATAREG, fifo_buffer[i]);
  }

  /* Ejecutar el comando */
  mfrc522_escribir_registro(MFRC522_COMMANDREG, MFRC522_MFAUTHENT);

  /* Esperar a que el comando finalice */
  i = 0xFF;
  do {
    commirq_reg = mfrc522_leer_registro(MFRC522_COMMIRQREG);
    command_reg = mfrc522_leer_registro(MFRC522_COMMANDREG);
    i--;
  } while ((i != 0) && !(commirq_reg & 0x01) && ((command_reg & 0x0F) == 0x0E));

  if (i == 0) {
    return FALSE;
  }

  /* Verificar que el bit CRYPTO1ON se activó (autenticación exitosa) */
  do {
    status2 = mfrc522_leer_registro(MFRC522_STATUS2REG);
    if (status2 & 0x08) {
      return TRUE;
    }
    intentos++;
  } while (intentos < 255);

  return FALSE;
}

/* ============================================================
   Operaciones sobre la tarjeta PICC
   ============================================================ */

/**
 * @brief   Enviar el comando REQA a la tarjeta (ISO/IEC 14443 Request).
 *
 * @param[out]  p_atqa  Buffer de 2 bytes donde se almacena la respuesta ATQA de la tarjeta.
 *
 * @retval  TRUE   La tarjeta respondió con 2 bytes de ATQA.
 * @retval  FALSE  Sin tarjeta o error de comunicación.
 */
bool_t mfrc522_picc_request(uint8_t *p_atqa) {
  uint32_t bytes_rx;
  uint8_t  buffer_tx[1];

  /* ISO/IEC 14443-3A: REQA usa 7 bits, no el byte completo */
  mfrc522_escribir_registro(MFRC522_BITFRAMINGREG, 7);

  buffer_tx[0] = PICC_REQUEST;
  if (mfrc522_comando_transceive(buffer_tx, 1, p_atqa, &bytes_rx) && bytes_rx == 2) {
    return TRUE;
  }
  return FALSE;
}

/**
 * @brief   Enviar el comando WUPA a la tarjeta (Wake-Up, reactiva tarjetas en HALT).
 *
 * @param[out]  p_atqa  Buffer de 2 bytes para la respuesta ATQA.
 *
 * @retval  TRUE   Tarjeta despertada correctamente.
 * @retval  FALSE  Sin tarjeta o error.
 */
bool_t mfrc522_picc_wakeup(uint8_t *p_atqa) {
  uint32_t bytes_rx;
  uint8_t  buffer_tx[1];

  mfrc522_escribir_registro(MFRC522_BITFRAMINGREG, 7);

  buffer_tx[0] = PICC_WAKEUP;
  if (mfrc522_comando_transceive(buffer_tx, 1, p_atqa, &bytes_rx) && bytes_rx == 2) {
    return TRUE;
  }
  return FALSE;
}

/**
 * @brief   Ejecutar el procedimiento de anticollisión (Cascade Level 1).
 *
 * @details Solicita el UID de la tarjeta y valida la integridad mediante el BCC
 * (XOR de los 4 bytes del UID). Solo retorna TRUE si se reciben exactamente
 * 5 bytes (4 UID + 1 BCC) y el BCC es correcto.
 *
 * @param[out]  p_uid_bcc  Buffer de ≥5 bytes: [UID0, UID1, UID2, UID3, BCC].
 * @param[out]  bytes_rx   Número de bytes recibidos.
 *
 * @retval  TRUE   UID recibido y BCC válido.
 * @retval  FALSE  Error de comunicación o BCC inválido.
 */
bool_t mfrc522_picc_anticollision(uint8_t *p_uid_bcc, uint32_t *bytes_rx) {
  uint8_t buffer_tx[2];
  uint8_t i, bcc_calculado = 0;

  mfrc522_escribir_registro(MFRC522_BITFRAMINGREG, 0);

  buffer_tx[0] = PICC_CL1;
  buffer_tx[1] = PICC_ANTICOLLISION;

  if (mfrc522_comando_transceive(buffer_tx, 2, p_uid_bcc, bytes_rx) && *bytes_rx == 5) {
    /* Validar BCC: XOR de los 4 bytes del UID */
    for (i = 0; i < 4; i++) {
      bcc_calculado ^= p_uid_bcc[i];
    }
    if (bcc_calculado == p_uid_bcc[4]) {
      return TRUE;
    }
  }
  return FALSE;
}

/**
 * @brief   Enviar el comando HALT para finalizar la comunicación con la tarjeta.
 *
 * @retval  TRUE   Comando enviado correctamente.
 * @retval  FALSE  Error al calcular el CRC o en la transmisión.
 */
bool_t mfrc522_picc_halt(void) {
  uint8_t  crc[2], buffer[4];
  uint32_t bytes_rx;

  buffer[0] = PICC_HALT;
  buffer[1] = 0;

  if (!mfrc522_comando_calccrc(buffer, 2, crc)) {
    return FALSE;
  }

  buffer[2] = crc[0];
  buffer[3] = crc[1];

  return mfrc522_comando_transceive(buffer, 4, buffer, &bytes_rx);
}

/**
 * @brief   Seleccionar la tarjeta usando su UID (Cascade Level 1 SELECT).
 *
 * @details Envía el comando SELECT con el UID + BCC y espera el SAK (0x08 para MIFARE 1K).
 * Reintenta hasta 10 veces si no se recibe un SAK válido.
 *
 * @param[in]   p_uid_bcc  Array de 5 bytes: [UID0..UID3, BCC].
 *
 * @retval  TRUE   Tarjeta seleccionada (SAK = 0x08 recibido).
 * @retval  FALSE  Error o SAK no válido tras 10 intentos.
 */
bool_t mfrc522_picc_select(uint8_t *p_uid_bcc) {
  bool_t   resp;
  uint32_t bytes_rx;
  uint8_t  crc[2], buffer_tx[9], buffer_rx[3], intentos = 0;

  buffer_tx[0] = PICC_CL1;
  buffer_tx[1] = PICC_SELECT;
  memcpy(&buffer_tx[2], p_uid_bcc, 5);  /* 4 bytes UID + 1 byte BCC */

  resp = mfrc522_comando_calccrc(buffer_tx, 7, crc);
  ASSERT(resp, "Error al calcular el CRC en PICC_SELECT.");

  buffer_tx[7] = crc[0];
  buffer_tx[8] = crc[1];

  do {
    resp = mfrc522_comando_transceive(buffer_tx, 9, buffer_rx, &bytes_rx);
    if (resp && bytes_rx == 1 && buffer_rx[0] == 0x08) {
      return TRUE;
    }
    intentos++;
  } while (intentos < 10);

  return FALSE;
}

/**
 * @brief   Leer un bloque de 16 bytes de la tarjeta MIFARE Classic.
 *
 * @details Requiere autenticación previa del sector con mfrc522_comando_mfauthent().
 *
 * @param[in]   direccion_bloque   Número del bloque a leer (0-63 para MIFARE 1K).
 * @param[out]  p_datos_bloque_rx  Buffer de ≥16 bytes donde se almacenan los datos leídos.
 *
 * @retval  TRUE   Lectura correcta (se recibieron 18 bytes: 16 datos + 2 CRC).
 * @retval  FALSE  Error en la comunicación o longitud inesperada.
 */
bool_t mfrc522_picc_leer_bloque(uint8_t direccion_bloque, uint8_t *p_datos_bloque_rx) {
  uint8_t  buffer_tx[4], crc[2], buffer_rx[18];
  bool_t   resp;
  uint32_t bytes_rx;
  uint8_t  i;

  ASSERT(p_datos_bloque_rx != NULL, "p_datos_bloque_rx no puede ser nulo.");
  ASSERT(direccion_bloque < 64, "direccion_bloque fuera de rango (0-63).");

  buffer_tx[0] = PICC_READ;
  buffer_tx[1] = direccion_bloque;

  resp = mfrc522_comando_calccrc(buffer_tx, 2, crc);
  ASSERT(resp, "Error al calcular CRC en PICC_READ.");

  buffer_tx[2] = crc[0];
  buffer_tx[3] = crc[1];

  resp = mfrc522_comando_transceive(buffer_tx, 4, buffer_rx, &bytes_rx);
  if (!resp) {
    return FALSE;
  }

  timer_retardo_ms(mfrc522_timer, 2);

  if (bytes_rx != 18) {
    return FALSE;
  }

  for (i = 0; i < 16; i++) {
    p_datos_bloque_rx[i] = buffer_rx[i];
  }

  return TRUE;
}

/**
 * @brief   Escribir un bloque de 16 bytes en la tarjeta MIFARE Classic.
 *
 * @details Requiere autenticación previa del sector. El protocolo consta de dos partes:
 *          1) Enviar WRITE + dirección + CRC  → la tarjeta responde ACK (0x0A).
 *          2) Enviar los 16 bytes de datos + CRC → la tarjeta responde ACK (0x0A).
 *
 * @param[in]   direccion_bloque    Número del bloque a escribir (0-63).
 * @param[in]   p_datos_bloque_tx   Buffer de 16 bytes con los datos a escribir.
 * @param[in]   tam_datos           Debe ser 16.
 *
 * @retval  TRUE   Escritura completada (ambos ACK recibidos).
 * @retval  FALSE  Error en comunicación, CRC o respuesta inesperada.
 */
bool_t mfrc522_picc_escribir_bloque(uint8_t direccion_bloque, uint8_t *p_datos_bloque_tx,
                                    uint8_t tam_datos) {
  uint8_t  buffer_tx[4], buffer_datos[18], buffer_rx[4], crc[2];
  uint32_t bytes_rx;
  bool_t   resp;
  uint8_t  i;

  ASSERT(p_datos_bloque_tx != NULL, "p_datos_bloque_tx no puede ser nulo.");
  ASSERT(tam_datos == 16, "tam_datos debe ser 16 bytes para MIFARE Classic.");

  /* Parte 1: enviar comando WRITE + dirección + CRC */
  buffer_tx[0] = PICC_WRITE;
  buffer_tx[1] = direccion_bloque;

  resp = mfrc522_comando_calccrc(buffer_tx, 2, crc);
  ASSERT(resp, "Error al calcular CRC del comando PICC_WRITE.");

  buffer_tx[2] = crc[0];
  buffer_tx[3] = crc[1];

  resp = mfrc522_comando_transceive(buffer_tx, 4, buffer_rx, &bytes_rx);
  ASSERT(resp, "Error en transceive durante WRITE Parte 1.");
  ASSERT(buffer_rx[0] == 0x0A, "ACK no recibido en WRITE Parte 1.");

  /* Parte 2: enviar los 16 bytes de datos + CRC */
  resp = mfrc522_comando_calccrc(p_datos_bloque_tx, tam_datos, crc);
  ASSERT(resp, "Error al calcular CRC de datos en WRITE Parte 2.");

  for (i = 0; i < tam_datos; i++) {
    buffer_datos[i] = p_datos_bloque_tx[i];
  }
  buffer_datos[tam_datos]     = crc[0];
  buffer_datos[tam_datos + 1] = crc[1];

  resp = mfrc522_comando_transceive(buffer_datos, tam_datos + 2, buffer_rx, &bytes_rx);
  ASSERT(resp, "Error en transceive durante WRITE Parte 2.");
  ASSERT(buffer_rx[0] == 0x0A, "ACK no recibido en WRITE Parte 2.");

  return TRUE;
}

/* ============================================================
   Detección de tarjetas
   ============================================================ */

/**
 * @brief   Detectar si hay una tarjeta y obtener su UID (no bloqueante).
 *
 * @details Ejecuta la secuencia: REQA → ANTICOLLISION → HALT.
 * Si la tarjeta responde, su UID (4 bytes) queda en p_uid[0..3].
 *
 * @param[out]  p_uid  Buffer de ≥5 bytes. Los 4 primeros contendrán el UID si retorna TRUE.
 *
 * @retval  TRUE   Tarjeta detectada, UID almacenado en p_uid.
 * @retval  FALSE  Sin tarjeta o BCC inválido.
 */
bool_t mfrc522_detectar_tarjeta(uint8_t *p_uid) {
  bool_t   resp;
  uint32_t num_bytes;

  resp = mfrc522_picc_request(p_uid);

  if (resp) {
    resp = mfrc522_picc_anticollision(p_uid, &num_bytes);
  }

  mfrc522_picc_halt();

  if (resp && num_bytes == 5) {
    return TRUE;
  }
  return FALSE;
}

/**
 * @brief   Esperar (bloqueante) hasta que se detecte una tarjeta.
 *
 * @param[out]  p_uid  Buffer donde se almacenará el UID (≥5 bytes).
 *
 * @warning Nunca retorna si no se presenta una tarjeta.
 */
void mfrc522_esperar_detectar_tarjeta(uint8_t *p_uid) {
  while (!mfrc522_detectar_tarjeta(p_uid)) {
    ;
  }
}

/**
 * @brief   Detectar tarjeta, autenticar, escribir un bloque y hacer HALT (no bloqueante).
 *
 * @param[out]  p_uid              UID de la tarjeta detectada (≥5 bytes).
 * @param[in]   direccion_bloque   Bloque a escribir (0-63).
 * @param[in]   p_datos_bloque_tx  16 bytes a escribir.
 * @param[in]   tam_datos          Debe ser 16.
 *
 * @retval  TRUE   Operación completada.
 * @retval  FALSE  Fallo en cualquier paso.
 */
bool_t mfrc522_detectar_tarjeta_con_escritura(uint8_t *p_uid, uint8_t direccion_bloque,
                                              uint8_t *p_datos_bloque_tx, uint8_t tam_datos) {
  bool_t   resp;
  uint32_t bytes_rx;

  resp = mfrc522_picc_request(p_uid);
  if (!resp) {
    return FALSE;
  }

  resp = mfrc522_picc_anticollision(p_uid, &bytes_rx);
  if (!resp || bytes_rx != 5) {
    return FALSE;
  }

  resp = mfrc522_picc_select(p_uid);
  if (!resp) {
    return FALSE;
  }

  resp = mfrc522_comando_mfauthent(PICC_AUTHENTA, TRAILER_BLOCK_HARDCODED,
                                   clave_A_sector, p_uid);
  if (!resp) {
    return FALSE;
  }

  resp = mfrc522_picc_escribir_bloque(direccion_bloque, p_datos_bloque_tx, tam_datos);
  if (!resp) {
    return FALSE;
  }

  mfrc522_picc_halt();
  mfrc522_borrar_bits(MFRC522_STATUS2REG, 0x08);  /* Detener cifrado CRYPTO1 */

  return TRUE;
}

/**
 * @brief   Esperar (bloqueante) hasta completar detección + autenticación + escritura.
 *
 * @warning Nunca retorna si no se presenta una tarjeta válida.
 */
void mfrc522_esperar_detectar_tarjeta_con_escritura(uint8_t *p_uid, uint8_t direccion_bloque,
                                                    uint8_t *p_datos_bloque_tx, uint8_t tam_datos) {
  while (!mfrc522_detectar_tarjeta_con_escritura(p_uid, direccion_bloque,
                                                 p_datos_bloque_tx, tam_datos)) {
    ;
  }
}

/* ============================================================
   Funciones privadas
   ============================================================ */

/**
 * @brief   Controlar la señal Chip Select (/NSS) del MFRC522.
 * @private
 *
 * @details Cuando funcion_cs = FALSE (GPIO manual, caso habitual con el RC522) se
 * controla el pin directamente y se añade un retardo de 1 µs para respetar los
 * tiempos de setup/hold del RC522 (datasheet tabla 154: tNHNL ≥ 50 ns, pero
 * en práctica 1 µs es suficiente para evitar problemas con el jitter del bus).
 *
 * Cuando funcion_cs = TRUE el hardware SSP gestiona SSEL automáticamente,
 * por lo que esta función no hace nada (spi_transferir ya lo controla).
 *
 * @param[in]   nivel   TRUE  → CS en alto (fin de trama, esclavo desactivado).
 *                      FALSE → CS en bajo (inicio de trama, esclavo activado).
 */
static void mfrc522_seleccionar_esclavo(bool_t nivel) {
  if (!mfrc522_funcion_cs) {
    if (nivel) {
      gpio_pin_a_1(mfrc522_puerto_cs, mfrc522_mascara_pin_cs);  /* CS alto: fin de trama    */
    } else {
      gpio_pin_a_0(mfrc522_puerto_cs, mfrc522_mascara_pin_cs);  /* CS bajo: inicio de trama */
    }
    /* Retardo de 1 µs para respetar tsetup/thold del RC522 */
    timer_retardo_us(mfrc522_timer, 1);
  }
  /* funcion_cs = TRUE: el SSP gestiona SSEL → nada que hacer aquí */
}

/**
 * @brief   Escribir un byte en un registro del MFRC522 via SPI.
 * @private
 *
 * @details Protocolo SPI del RC522 (datasheet sección 8.1.2.3):
 *   Byte de dirección: [ 0 | A5 A4 A3 A2 A1 A0 | 0 ]
 *                        ^                        ^
 *                      bit7=0 (escritura)        bit0=0 siempre
 *   → (registro << 1) & 0x7E
 *
 * Secuencia: CS↓ → enviar dirección → enviar dato → CS↑
 *
 * @param[in]   registro  Dirección del registro destino.
 * @param[in]   dato      Valor a escribir.
 */
static void mfrc522_escribir_registro(uint8_t registro, uint8_t dato) {
  uint8_t direccion = (registro << 1) & 0x7E;  /* bit7=0 → escritura, bit0=0 */

  mfrc522_seleccionar_esclavo(FALSE);       /* CS bajo: activar esclavo          */
  spi_transferir(mfrc522_ssp, direccion);   /* Enviar dirección (byte dummy en RX) */
  spi_transferir(mfrc522_ssp, dato);        /* Enviar dato      (byte dummy en RX) */
  mfrc522_seleccionar_esclavo(TRUE);        /* CS alto: desactivar esclavo        */
}

/**
 * @brief   Leer el contenido de un registro del MFRC522 via SPI.
 * @private
 *
 * @details Protocolo SPI del RC522 (datasheet sección 8.1.2.3):
 *   Byte de dirección: [ 1 | A5 A4 A3 A2 A1 A0 | 0 ]
 *                        ^                        ^
 *                      bit7=1 (lectura)           bit0=0 siempre
 *   → ((registro << 1) & 0x7E) | 0x80
 *
 * Secuencia: CS↓ → enviar dirección → enviar dummy (0x00) y capturar respuesta → CS↑
 *
 * @param[in]   registro  Dirección del registro a leer.
 * @return      Byte leído del registro.
 */
static uint8_t mfrc522_leer_registro(uint8_t registro) {
  uint8_t valor;
  uint8_t direccion = ((registro << 1) & 0x7E) | 0x80;  /* bit7=1 → lectura */

  mfrc522_seleccionar_esclavo(FALSE);              /* CS bajo: activar esclavo             */
  spi_transferir(mfrc522_ssp, direccion);          /* Enviar dirección (respuesta ignorada) */
  valor = spi_transferir(mfrc522_ssp, 0x00);       /* Dummy TX → capturar dato del RC522   */
  mfrc522_seleccionar_esclavo(TRUE);               /* CS alto: desactivar esclavo           */

  return valor;
}

/**
 * @brief   Poner a 1 los bits indicados por mascara en un registro del MFRC522.
 * @private
 *
 * @details Lee el valor actual, aplica OR con la máscara y escribe el resultado.
 *
 * @param[in]   registro  Dirección del registro a modificar.
 * @param[in]   mascara   Bits a poner a 1 (los bits a 0 en la máscara no se alteran).
 */
static void mfrc522_activar_bits(uint8_t registro, uint8_t mascara) {
  uint8_t valor = mfrc522_leer_registro(registro);
  mfrc522_escribir_registro(registro, valor | mascara);
}

/**
 * @brief   Poner a 0 los bits indicados por mascara en un registro del MFRC522.
 * @private
 *
 * @details Lee el valor actual, aplica AND con el complemento de la máscara y escribe el resultado.
 *
 * @param[in]   registro  Dirección del registro a modificar.
 * @param[in]   mascara   Bits a poner a 0 (los bits a 0 en la máscara no se alteran).
 */
static void mfrc522_borrar_bits(uint8_t registro, uint8_t mascara) {
  uint8_t valor = mfrc522_leer_registro(registro);
  mfrc522_escribir_registro(registro, valor & ~mascara);
}