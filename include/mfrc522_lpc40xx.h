/**
 * @file    mfrc522_lpc40xx.h
 * @brief   Funciones para acceder al lector RFID MFRC522 en modo SPI a través de una interfaz SSP
 * del LPC40xx.
 *
 * @author    Alejandro Lara Doña - alejandro.lara@uca.es
 * @date      2025
 * @version   2.0
 *
 * @copyright GNU General Public License version 3 or later
 */

#ifndef MFRC522_LPC40XX_H
#define MFRC522_LPC40XX_H

#include <LPC407x_8x_177x_8x.h>
#include "tipos.h"
#include "gpio_lpc40xx.h"
#include "spi_lpc40xx.h"
#include "timer_lpc40xx.h"

// ===== MFRC522 - Constantes Publicas =====
/**
 * @brief   Direcciones de los registros internos del MFRC522.
 * @ingroup MFRC522
 *
 * @details El MFRC522 organiza sus registros en 4 páginas de memoria (0x00-0x3F).
 *          Estos registros controlan el comportamiento del chip, estados de comunicación,
 *          configuración RF, timers, y funciones de test.
 *
 * @note    El acceso a registros se realiza mediante protocolo SPI con direccionamiento
 *          de 6 bits (bits 1-6), donde bit 7 indica lectura (1) o escritura (0).
 *
 * @see     MFRC522 Datasheet, Sección 9 (Register description).
 */
enum mfrc522_registros {
  // Página 0: Command and Status (0x00-0x0F) - Comandos y estados de operación
  MFRC522_RESERVED00      = 0x00, //!< Reservado para uso futuro.
  MFRC522_COMMANDREG      = 0x01, //!< Registro de comandos (IDLE, TRANSCEIVE, CALCCRC, etc.).
  MFRC522_COMMIENREG      = 0x02, //!< Habilitar interrupciones de comunicación.
  MFRC522_DIVLENREG       = 0x03, //!< Habilitar interrupciones del divisor de frecuencia.
  MFRC522_COMMIRQREG      = 0x04, //!< Flags de interrupciones de comunicación (TimerIRQ, RxIRQ, etc.).
  MFRC522_DIVIRQREG       = 0x05, //!< Flags de interrupciones del divisor.
  MFRC522_ERRORREG        = 0x06, //!< Registro de errores (CRC, paridad, colisión, etc.).
  MFRC522_STATUS1REG      = 0x07, //!< Estado de comunicación y estados internos.
  MFRC522_STATUS2REG      = 0x08, //!< Estado del receptor y cifrado (CRYPTO1ON, etc.).
  MFRC522_FIFODATAREG     = 0x09, //!< Entrada/salida del buffer FIFO (64 bytes).
  MFRC522_FIFOLEVELREG    = 0x0A, //!< Nivel actual del FIFO y control de vaciado.
  MFRC522_WATERLEVELREG   = 0x0B, //!< Nivel de agua del FIFO para generar interrupciones.
  MFRC522_CONTROLREG      = 0x0C, //!< Control de inicio/parada de timer, receptor, y estado último bit.
  MFRC522_BITFRAMINGREG   = 0x0D, //!< Control de framing de bits (inicio de transmisión, alineación).
  MFRC522_COLLREG         = 0x0E, //!< Posición del primer bit de colisión detectado.
  MFRC522_RESERVED0F      = 0x0F, //!< Reservado para uso futuro.

  // Página 1: Command (0x10-0x1F) - Configuración de transmisión y recepción
  MFRC522_RESERVED10      = 0x10, //!< Reservado para uso futuro.
  MFRC522_MODEREG         = 0x11, //!< Modo de operación general (polaridad CRC, preset CRC).
  MFRC522_TXMODEREG       = 0x12, //!< Configuración del transmisor (CRC, velocidad, inversión).
  MFRC522_RXMODEREG       = 0x13, //!< Configuración del receptor (CRC, velocidad, detección múltiple).
  MFRC522_TXCONTROLREG    = 0x14, //!< Control de pines de antena TX1 y TX2.
  MFRC522_TXASKREG        = 0x15, //!< Modulación ASK del transmisor (fuerza modulación 100%).
  MFRC522_TXSELREG        = 0x16, //!< Selección de fuente de modulación del transmisor.
  MFRC522_RXSELREG        = 0x17, //!< Configuración del demodulador del receptor.
  MFRC522_RXTHRESHOLDREG  = 0x18, //!< Umbrales de detección de bits para el receptor.
  MFRC522_DEMODREG        = 0x19, //!< Configuración del demodulador (ganancia, tau, filtros).
  MFRC522_RESERVED1A      = 0x1A, //!< Reservado para uso futuro.
  MFRC522_RESERVED1B      = 0x1B, //!< Reservado para uso futuro.
  MFRC522_MFTXREG         = 0x1C, //!< Control de transmisión MIFARE (ParityDisable).
  MFRC522_MFRXREG         = 0x1D, //!< Control de recepción MIFARE (ParityDisable).
  MFRC522_RESERVED1E      = 0x1E, //!< Reservado para uso futuro.
  MFRC522_SERIALSPEEDREG  = 0x1F, //!< Velocidad de comunicación serie (UART).

  // Página 2: Configuration (0x20-0x2F) - Configuración RF, CRC, timers
  MFRC522_RESERVED20      = 0x20, //!< Reservado para uso futuro.
  MFRC522_CRCRESULTREGH   = 0x21, //!< Resultado del cálculo CRC (byte alto).
  MFRC522_CRCRESULTREGL   = 0x22, //!< Resultado del cálculo CRC (byte bajo).
  MFRC522_RESERVED23      = 0x23, //!< Reservado para uso futuro.
  MFRC522_MODWIDTHREG     = 0x24, //!< Ancho de pulso de modulación.
  MFRC522_RESERVED25      = 0x25, //!< Reservado para uso futuro.
  MFRC522_RFCFGREG        = 0x26, //!< Configuración del campo RF (ganancia del receptor).
  MFRC522_GSNREG          = 0x27, //!< Conductancia de los transistores N-MOS de TX1 y TX2.
  MFRC522_CWGSPREG        = 0x28, //!< Conductancia en modo portadora no modulada.
  MFRC522_MODGSPREG       = 0x29, //!< Conductancia en modo portadora modulada.
  MFRC522_TMODEREG        = 0x2A, //!< Configuración del timer (TAuto, prescaler MSB).
  MFRC522_TPRESCALERREG   = 0x2B, //!< Prescaler del timer (divide frecuencia base 13.56 MHz).
  MFRC522_TRELOADREGH     = 0x2C, //!< Valor de recarga del timer (byte alto, 16 bits total).
  MFRC522_TRELOADREGL     = 0x2D, //!< Valor de recarga del timer (byte bajo).
  MFRC522_TCOUNTERVALREGH = 0x2E, //!< Valor actual del contador del timer (byte alto, solo lectura).
  MFRC522_TCOUNTERVALREGL = 0x2F, //!< Valor actual del contador del timer (byte bajo, solo lectura).

  // Página 3: Test register (0x30-0x3F) - Registros de prueba y diagnóstico
  MFRC522_RESERVED30      = 0x30, //!< Reservado para uso futuro.
  MFRC522_TESTSEL1REG     = 0x31, //!< Selección de señales de test en pin MFIN.
  MFRC522_TESTSEL2REG     = 0x32, //!< Selección de señales de test en pines AUX1 y AUX2.
  MFRC522_TESTPINENREG    = 0x33, //!< Habilitar pines de test (D1-D7).
  MFRC522_TESTPINVALUEREG = 0x34, //!< Valores de pines de test (solo lectura de D1-D7).
  MFRC522_TESTBUSREG      = 0x35, //!< Estado del bus interno de test.
  MFRC522_AUTOTESTREG     = 0x36, //!< Control y estado del auto-test integrado.
  MFRC522_VERSIONREG      = 0x37, //!< Versión del hardware (v1.0 = 0x91, v2.0 = 0x92).
  MFRC522_ANALOGTESTREG   = 0x38, //!< Control de auto-test analógico.
  MFRC522_TESTDAC1REG     = 0x39, //!< Valor de test del DAC1 (pin TSTBUS1).
  MFRC522_TESTDAC2REG     = 0x3A, //!< Valor de test del DAC2 (pin TSTBUS2).
  MFRC522_TESTADCREG      = 0x3B, //!< Resultado de conversión ADC de test.
  MFRC522_RESERVED3C      = 0x3C, //!< Reservado para uso futuro.
  MFRC522_RESERVED3D      = 0x3D, //!< Reservado para uso futuro.
  MFRC522_RESERVED3E      = 0x3E, //!< Reservado para uso futuro.
  MFRC522_RESERVED3F      = 0x3F  //!< Reservado para uso futuro.
};

/**
 * @brief   Comandos internos del MFRC522.
 * @ingroup MFRC522
 *
 * @details Códigos de comandos que se escriben en COMMANDREG para ejecutar operaciones
 *          específicas del chip (transmisión, recepción, cálculo CRC, autenticación, etc.).
 *          El chip permanece en el comando hasta que se escribe un nuevo comando.
 *
 * @note    Los comandos se cancelan automáticamente al detectar errores o al escribir
 *          el comando IDLE. Algunos comandos finalizan automáticamente al completarse.
 *
 * @see     MFRC522 Datasheet, Sección 10 (Command description).
 */
enum mfrc522_comandos {
  MFRC522_IDLE             = 0x00, //!< Sin acción, cancela comando actual.
  MFRC522_MEM              = 0x01, //!< Almacena 25 bytes del FIFO en buffer interno.
  MFRC522_GENERATERANDOMID = 0x02, //!< Genera un ID aleatorio de 10 bytes en el FIFO.
  MFRC522_CALCCRC          = 0x03, //!< Activa el coprocesador CRC para calcular CRC de datos en FIFO.
  MFRC522_TRANSMIT         = 0x04, //!< Transmite datos del FIFO a la antena (sin esperar respuesta).
  MFRC522_NOCMDCHANGE      = 0x07, //!< No cambia el comando actual (útil para modificar registros).
  MFRC522_RECEIVE          = 0x08, //!< Activa el receptor, datos recibidos se almacenan en FIFO.
  MFRC522_TRANSCEIVE       = 0x0C, //!< Transmite datos del FIFO y activa receptor automáticamente.
  MFRC522_MFAUTHENT        = 0x0E, //!< Ejecuta autenticación MIFARE con tarjeta PICC (CRYPTO1).
  MFRC522_SOFTRESET        = 0x0F  //!< Resetea el chip (similar a power-on reset, excepto registros E2).
};

/**
 * @brief   Comandos del protocolo ISO/IEC 14443-3A para tarjetas PICC (Mifare Classic).
 * @ingroup MFRC522
 *
 * @details Códigos de comandos que se envían a las tarjetas RFID compatibles con el protocolo
 *          ISO/IEC 14443-3 Type A (MIFARE Classic, MIFARE Plus, NTAG, etc.). Estos comandos
 *          se transmiten mediante el comando TRANSCEIVE del MFRC522.
 *
 * @note    Cada comando tiene una estructura específica de bytes adicionales (dirección de
 *          bloque, UID, CRC, etc.) según el protocolo MIFARE.
 *
 * @see     MIFARE Classic EV1 1K Datasheet (MF1S50YYX_V1), Sección 9.2 (Command overview).
 * @see     ISO/IEC 14443-3:2018, Type A initialization and anticollision.
 */
enum mfrc522_picc_comandos {
  PICC_REQUEST          = 0x26, //!< REQA: Despertar tarjetas en estado IDLE (respuesta: ATQA 2 bytes).
  PICC_WAKEUP           = 0x52, //!< WUPA: Despertar tarjetas en estado HALT/IDLE (respuesta: ATQA).
  PICC_CL1              = 0x93, //!< Cascade Level 1: Anticolisión/selección para UID de 4 bytes.
  PICC_CL2              = 0x95, //!< Cascade Level 2: Anticolisión/selección para UID de 7 bytes.
  PICC_ANTICOLLISION    = 0x20, //!< NVB (Number of Valid Bits): Parámetro para anticolisión (2 bytes: 0x20).
  PICC_SELECT           = 0x70, //!< NVB para SELECT: Seleccionar tarjeta específica (respuesta: SAK).
  PICC_HALT             = 0x50, //!< HLTA: Poner tarjeta en estado HALT (no responde a REQA).
  PICC_AUTHENTA         = 0x60, //!< Autenticación con clave A (6 bytes + dirección bloque trailer).
  PICC_AUTHENTB         = 0x61, //!< Autenticación con clave B (6 bytes + dirección bloque trailer).
  PICC_PERSONALIZEUID   = 0x40, //!< Personalizar UID (solo MIFARE Classic UID modificable).
  PICC_READ             = 0x30, //!< Leer bloque de 16 bytes (requiere autenticación previa).
  PICC_WRITE            = 0xA0, //!< Escribir bloque de 16 bytes (requiere autenticación previa).
  PICC_DECREMENT        = 0xC0, //!< Decrementar valor de bloque (bloques de valor MIFARE).
  PICC_INCREMENT        = 0xC1, //!< Incrementar valor de bloque (bloques de valor MIFARE).
  PICC_RESTORE          = 0xC2, //!< Restaurar valor desde buffer de transferencia.
  PICC_TRANSFER         = 0xB0  //!< Transferir valor a buffer interno (sin escribir en EEPROM aún).
};

/**
 * @brief   Máscaras de bits más relevantes de los registros del MFRC522.
 * @ingroup MFRC522
 */
enum mfrc522_bits {
  // COMMIRQREG (0x04) - Registro de interrupciones de comunicación
  MFRC522_BIT_TIMERINT = (1u << 0),     //!< Interrupción de timer (timeout).
  MFRC522_BIT_CRCIRQ = (1u << 2),       //!< Se ha terminado de procesar y calcular el CRC.
  MFRC522_BIT_IDLEINT = (1u << 4),      //!< Interrupción de estado IDLE.
  MFRC522_BIT_RXINT = (1u << 5),        //!< Interrupción de recepción.
  MFRC522_BIT_IRQ_MASK = 0x30,          //!< Máscara para bits de interrupción (RxInt | IdleInt).

  // ERRORREG (0x06) - Registro de errores
  MFRC522_BIT_PROTOCOL_ERR = (1u << 0), //!< Error de protocolo.
  MFRC522_BIT_PARITY_ERR = (1u << 1),   //!< Error de paridad.
  MFRC522_BIT_CRC_ERR = (1u << 2),      //!< Error de CRC.
  MFRC522_BIT_COLL_ERR = (1u << 3),     //!< Error de colisión.
  MFRC522_BIT_BUFFER_OVFL = (1u << 4),  //!< Desbordamiento del buffer.
  MFRC522_BIT_ERROR_MASK = 0x1B,        //!< Máscara de errores relevantes (sin temp error ni WrErr).

  // STATUS2REG (0x08) - Registro de estado 2
  MFRC522_BIT_CRYPTO1ON = (1u << 3),    //!< Cifrado CRYPTO1 activo (autenticación exitosa).

  // FIFOLEVELREG (0x0A) - Registro de nivel del FIFO
  MFRC522_BIT_VACIARFIFO = (1u << 7),   //!< Vaciar la FIFO.

  // BITFRAMINGREG (0x0D) - Registro de framing de bits
  MFRC522_BIT_STARTSEND = (1u << 7),    //!< Iniciar transmisión inmediata.

  // TXCONTROLREG (0x14) - Registro de control de transmisión
  MFRC522_BIT_TX1RFEN = (1u << 0),      //!< Habilitar salida TX1 de antena.
  MFRC522_BIT_TX2RFEN = (1u << 1),      //!< Habilitar salida TX2 de antena.
  MFRC522_BIT_ANTENA_MASK = 0x03,       //!< Máscara para control de antena (TX1 | TX2).

  // COMMANDREG (0x01) - Registro de comandos
  MFRC522_BIT_RCVOFF = (1u << 5),       //!< Desactivar receptor analógico.
  MFRC522_BIT_POWERDOWN = (1u << 4),    //!< Entrar en modo de bajo consumo.
  MFRC522_BIT_COMMAND_MASK = 0x0F       //!< Máscara para extraer comando activo.
};

// ===== Funciones Públicas =====
void mfrc522_inicializar(LPC_SSP_TypeDef  *interfaz_ssp, uint32_t frecuencia_sck,
                         LPC_GPIO_TypeDef *puerto_sck, uint32_t mascara_pin_sck,
                         LPC_GPIO_TypeDef *puerto_miso, uint32_t mascara_pin_miso,
                         LPC_GPIO_TypeDef *puerto_mosi, uint32_t mascara_pin_mosi,
                         LPC_GPIO_TypeDef *puerto_cs, uint32_t mascara_pin_cs, bool_t funcion_cs,
                         LPC_TIM_TypeDef  *timer);

// ===== Funciones de operaciones sobre el MFRC522 =====
void mfrc522_antena_on(void);
void mfrc522_antena_off(void);

bool_t mfrc522_comando_transceive(uint8_t *p_buffer_tx, uint32_t tam_buffer_tx,
                                  uint8_t *p_buffer_rx, uint32_t *bytes_rx);
bool_t mfrc522_comando_calccrc(uint8_t *p_buffer_entrada, uint32_t tam_buffer, uint8_t *p_crc);
bool_t mfrc522_comando_mfauthent(uint8_t modo, uint8_t direccion_bloque,
                                 const uint8_t *p_clave_sector, uint8_t *p_uid);

// ===== Funciones de operaciones sobre la tarjeta PICC (Proximity Integrated Circuit Card) =====
bool_t mfrc522_picc_request(uint8_t *p_atqa);
bool_t mfrc522_picc_wakeup(uint8_t *p_atqa);
bool_t mfrc522_picc_anticollision(uint8_t *p_uid_bcc, uint32_t *bytes_rx);

bool_t mfrc522_picc_halt(void);
bool_t mfrc522_picc_select(uint8_t *p_uid_bcc);
bool_t mfrc522_picc_leer_bloque(uint8_t direccion_bloque, uint8_t *p_datos_bloque_rx);
bool_t mfrc522_picc_escribir_bloque(uint8_t direccion_bloque, uint8_t *p_datos_bloque_tx, uint8_t tam_datos);

// ===== Funciones para detectar tarjeta =====
bool_t mfrc522_detectar_tarjeta(uint8_t *p_uid);
void mfrc522_esperar_detectar_tarjeta(uint8_t *p_uid);

void mfrc522_esperar_detectar_tarjeta_con_lectura(uint8_t *p_uid, uint8_t direccion_bloque, uint8_t *p_datos_bloque_rx);

bool_t mfrc522_detectar_tarjeta_con_escritura(uint8_t *p_uid, uint8_t direccion_bloque, uint8_t *p_datos_bloque_tx, uint8_t tam_datos);
void mfrc522_esperar_detectar_tarjeta_con_escritura(uint8_t *p_uid, uint8_t direccion_bloque, uint8_t *p_datos_bloque_tx, uint8_t tam_datos);

#endif  // MFRC522_LPC40XX_H