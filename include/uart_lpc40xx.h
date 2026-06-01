/**
 * @file    uart_lpc40xx.h
 * @brief   Funciones de manejo de las UARTs del LPC40xx.
 *
 * @details El LPC40xx tiene 5 UARTs. Las UARTs 0, 2 y 3 son exactamente iguales entre sí. La
 * UART 1 añade señales de control de modem y tiene registros adicionales a los de las UARTs 0,
 * 2 y 3. La UART 4 puede funcionar en un modo síncrono y puede comunicar con smart cards, así
 * que también tiene registros adicionales.
 *
 * @note Las funciones de este módulo no usan ninguna de las características especiales de las
 * UARTs 1 y 4 por lo que se trataran como si fueran "normales".
 *
 * @author    Alejandro Lara Doña - alejandro.lara@uca.es | Eduardo Romero
 * @date      2014/2025
 * @version   2.0
 *
 * @copyright GNU General Public License version 3 or later
 */

#ifndef UART_LPC40XX_H
#define UART_LPC40XX_H

#include <LPC407x_8x_177x_8x.h>
#include "tipos.h"
#include "gpio_lpc40xx.h"

// ===== UART - Macros Publicas =====
/**
 * @defgroup  UART_Macros_Publicas UART - Macros Públicas
 * @ingroup   UART
 * @brief Símbolos alternativos para las UARTs.
 * @{
 */
/**
 * @defgroup  UART_REDEF UART - Redefinición de la UART1 y UART4
 * @ingroup   UART_Macros_Publicas
 * @brief Redefinición de la UART1 y la UART4 como UARTs de tipo LPC_UART_TypeDef.
 *
 * @details Las funciones de este módulo no usan ninguna de las características especiales de las
 * UARTs 1 y 4. Las posiciones de los registros comunes a todas las UARTs coinciden, así que todas
 * se manejarán mediante punteros a estructura de tipo LPC_UART_TypeDef.
 * @{
 */
#define LPC_UART1_  ((LPC_UART_TypeDef *) LPC_UART1_BASE)
#define LPC_UART4_  ((LPC_UART_TypeDef *) LPC_UART4_BASE)
//!@}

#define UART0   LPC_UART0
#define UART1   LPC_UART1_
#define UART2   LPC_UART2
#define UART3   LPC_UART3
#define UART4   LPC_UART4_
//!@}

// ===== UART - Tipos Publicos =====
/**
 * @brief   Baudrates estándar.
 * @ingroup UART
 * @note    No es obligatorio escoger uno de estos, aunque es recomendable por compatibilidad.
 */
typedef enum {
  UART_BAUDRATE_110    = 110u,     //!< 110 bps
  UART_BAUDRATE_300    = 300u,     //!< 300 bps
  UART_BAUDRATE_600    = 600u,     //!< 600 bps
  UART_BAUDRATE_1200   = 1200u,    //!< 1200 bps
  UART_BAUDRATE_2400   = 2400u,    //!< 2400 bps
  UART_BAUDRATE_4800   = 4800u,    //!< 4800 bps
  UART_BAUDRATE_9600   = 9600u,    //!< 9600 bps
  UART_BAUDRATE_14400  = 14400u,   //!< 14400 bps
  UART_BAUDRATE_19200  = 19200u,   //!< 19200 bps
  UART_BAUDRATE_28800  = 28800u,   //!< 28800 bps
  UART_BAUDRATE_38400  = 38400u,   //!< 38400 bps
  UART_BAUDRATE_57600  = 57600u,   //!< 57600 bps
  UART_BAUDRATE_115200 = 115200u,  //!< 115200 bps
  UART_BAUDRATE_230400 = 230400u,  //!< 230400 bps
  UART_BAUDRATE_460800 = 460800u   //!< 460800 bps
} uart_baudrate_t;

/**
 * @brief   Tipo para especificar el número de bits de datos.
 * @ingroup UART
 */
typedef enum {
  UART_BITS_DATOS_5,  //!< 5 bits por paquete.
  UART_BITS_DATOS_6,  //!< 6 bits por paquete.
  UART_BITS_DATOS_7,  //!< 7 bits por paquete.
  UART_BITS_DATOS_8   //!< 8 bits por paquete.
} uart_bits_datos_t;

/**
 * @brief   Tipo para especificar el número de bits de stop.
 * @ingroup UART
 */
typedef enum {
  UART_BITS_STOP_1,   //!< 1 bit de Stop.
  UART_BITS_STOP_2    //!< 2 bit de Stop.
} uart_bits_stop_t;

/**
 * @brief   Tipo para especificar los tipos de paridad.
 * @ingroup UART
 */
typedef enum {
  UART_PARIDAD_NINGUNA  = 0,    //!< No se envia bit de paridad.
  UART_PARIDAD_IMPAR    = 1u,   //!< Bit de paridad impar.
  UART_PARIDAD_PAR      = 3u,   //!< Bit de paridad par.
  UART_PARIDAD_UNO      = 5u,   //!< Bit de paridad forzado a 1.
  UART_PARIDAD_CERO     = 7u    //!< Bit de paridad forzado a 0.
} uart_paridad_t;

/**
 * @brief   Tipo para el nivel de disparo de la FIFO de recepción que genera una interrupción.
 * @ingroup UART
 */
typedef enum {
  UART_NIVEL_FIFO_1,  //!< Nivel 0: 1 carácter.
  UART_NIVEL_FIFO_4,  //!< Nivel 1: 4 caracteres.
  UART_NIVEL_FIFO_8,  //!< Nivel 2: 8 caracteres.
  UART_NIVEL_FIFO_14  //!< Nivel 3: 14 caracteres.
} uart_nivel_fifo_t;


// ===== UART - Constantes Públicas =====
/**
 * @brief   Campos del registro LCR.
 * @ingroup UART
 *
 * @details En este registro se realizan los ajustes que definen el formato del paquete de datos:
 * número de bits, número de bits de stop, paridad y tipo. Además tiene bits para deshabilitar la
 * transmisión y habilitar el acceso a los registros de divisor del reloj para establecer la
 * frecuencia de la transmision (baudrate).
 */
enum uart_lcr {
  UART_LCR_BITS_DATO              = 0,          //!< Campo para el número de bits del dato.
  UART_LCR_BITS_STOP              = 2,          //!< Campo para el número de bits de Stop.
  UART_LCR_PARIDAD                = 3,          //!< Campo para la paridad.
  UART_LCR_BREAK_CONTROL          = (1u << 6),  //!< Habilita el bloqueo de la transmisión.
  UART_LCR_ACCESO_DIVISOR_LATCH   = (1u << 7)   //!< Habilita el acceso a los registros DLL y DLM.
};

/**
 * @brief   Campos del registro FCR.
 * @ingroup UART
 *
 * @details En este registro se realizan los ajustes que controlan las FIFOs de transmisión y
 * recepción.
 */
enum uart_fcr {
  UART_FCR_FIFOEN     = (1u << 0),  //!< Habilita las FIFOs.
  UART_FCR_RXFIFORES  = (1u << 1),  //!< Resetea la FIFO de recepción.
  UART_FCR_TXFIFORES  = (1u << 2),  //!< Resetea la FIFO de transmisión.
  UART_FCR_DMAMODE    = (1u << 3),  //!< Habilita el modo DMA.
  UART_FCR_RXTRIGLVL  = 6           //!< Campo para el nivel de disparo de la FIFO de recepción.
};

/**
 * @brief   Campos del registro IER.
 * @ingroup UART
 *
 * @details En este registro se habilitan o deshabilitan las interrupciones de la UART.
 */
enum uart_ier {
  UART_IER_RBR  = (1u << 0),  //!< Interrupción cuando hay un nuevo dato disponible.
  UART_IER_THRE = (1u << 1),  //!< Interrupción cuando la FIFO de transimisión está vacía.
  UART_IER_RLS  = (1u << 2),  //!< Interrupción por estado de la linea RX.
  UART_IER_ABEO = (1u << 8),  //!< Interrupción por finalización del auto-baud.
  UART_IER_ABTO = (1u << 9)   //!< Interrupción por timeout del auto-baud.
};

/**
 * @brief   Campos del registro IIR.
 * @ingroup UART
 *
 * @details Máscaras para comprobar el estado de las interrupciones.
 *
 *  INTID | Significado
 *  :---: | :---------
 *  0x3   | Cambio en el estado de la línea RX
 *  0x2   | Dato disponible
 *  0x6   | Character Time-out
 *  0x1   | FIFO TX vacía
 */
enum uart_iir {
  UART_IIR_STATUS   = (1u << 0),  //!< Activo cuando hay al menos 1 interrupción pendiente.
  UART_IIR_INTID    = (7u << 1),  //!< ID de la interrupción.
  UART_IIR_FIFOEN   = (3u << 6),  //!< Interrupción por estado de la linea RX.
  UART_IIR_ABEOINT  = (1u << 8),  //!< Activo al finalizar con éxito el auto-baud.
  UART_IIR_ABTOINT  = (1u << 9)   //!< Activo si ocurrió un timeout durante el auto-baud.
};

/**
 * @brief   Campos del registro LSR.
 * @ingroup UART
 *
 * @details Máscaras para comprobar el estado de la linea.
 */
enum uart_lsr{
  UART_LSR_RDR  = (1u << 0),  //!< Nuevo dato disponible.
  UART_LSR_OE   = (1u << 1),  //!< Error por sobreescritura de datos.
  UART_LSR_PE   = (1u << 2),  //!< Error de paridad.
  UART_LSR_FE   = (1u << 3),  //!< Error en el paquete recibido.
  UART_LSR_BI   = (1u << 4),  //!< Interrupción por señal de Break.
  UART_LSR_THRE = (1u << 5),  //!< FIFO de Transmisión vacía.
  UART_LSR_TEMT = (1u << 6),  //!< Transmisor vacío.
  UART_LSR_RXFE = (1u << 7)   //!< Error en la FIFO de recepción.
};

// ===== UART - Funciones Publicas =====
void uart_inicializar(LPC_UART_TypeDef *uart_regs, uart_baudrate_t baudrate,
                      uart_bits_datos_t numero_bits_datos, uart_paridad_t tipo_paridad,
                      uart_bits_stop_t numero_bits_stop,
                      LPC_GPIO_TypeDef *puerto_txd, uint32_t mascara_pin_txd,
                      LPC_GPIO_TypeDef *puerto_rxd, uint32_t mascara_pin_rxd,
                      float32_t *baudrate_real_obtenido);

void uart_transmitir_dato(LPC_UART_TypeDef *uart_regs, uint8_t dato);

bool_t uart_hay_dato_disponible(const LPC_UART_TypeDef *uart_regs);
uint8_t uart_leer_dato(const LPC_UART_TypeDef *uart_regs);
uint8_t uart_esperar_recibir_dato(const LPC_UART_TypeDef *uart_regs);

void uart_transmitir_cadena(LPC_UART_TypeDef *uart_regs, const char *cadena);
void uart_recibir_cadena(const LPC_UART_TypeDef *uart_regs, char *ptr_buffer,
                         uint32_t tamano_buffer);

void uart_habilitar_interrupciones_dato_recibido(LPC_UART_TypeDef *uart_regs, bool_t estado);

#endif  // UART_LPC40XX_H
