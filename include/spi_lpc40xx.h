/**
 * @file    spi_lpc40xx.h
 * @brief   Funciones básicas para usar las interfaces SSP del LPC40xx en modo maestro SPI.
 *
 * @author      Alejandro Lara Doña - alejandro.lara@uca.es | Eduardo Romero
 * @date        2014/2025
 * @version     2.0
 *
 * @copyright   GNU General Public License version 3 or later
 */

#ifndef SPI_LPC40XX_H
#define SPI_LPC40XX_H

#include <LPC407x_8x_177x_8x.h>
#include "tipos.h"

/**
 * @defgroup  SPI_Macros_Publicas SPI - Macros Públicas
 * @ingroup   SPI
 * @brief Símbolos alternativos de las interfaces SSP.
 * @{
 */
#define SSP0  LPC_SSP0
#define SSP1  LPC_SSP1
#define SSP2  LPC_SSP2
 //!<@}

// ===== SPI - Constantes Publicas =====
/**
 * @brief     Constantes para la polaridad del reloj (CPOL).
 * @ingroup   SPI
 *
 * @details Cuando el bit de control de polaridad del reloj CPOL es 0, se genera un valor bajo de 
 * estado estable en el pin SCK. Si el bit de control de polaridad del reloj CPOL es 1, se genera 
 * un valor alto de estado estable en el pin CLK cuando no se transfieren datos.
 *
 * El bit de control CPHA selecciona el flanco de reloj que captura los datos y permite que cambien 
 * de estado. Tiene el mayor impacto en el primer bit transmitido, permitiendo o no una transición 
 * de reloj antes del primer flanco de captura de datos. Cuando el bit de control de fase CPHA es 0,
 * los datos se capturan en la primera transición de flanco de reloj. Si el bit de control de fase 
 * de reloj CPHA es 1, los datos se capturan en la segunda transición de flanco de reloj.
 */
enum spi_reloj_polaridad {
  SPI_CPOL_0 = 0,   //!< SCK estable a nivel bajo. 
  SPI_CPOL_1 = 1    //!< SCK estable a nivel alto.
};

/**
 * @brief     Constantes para la fase del reloj (CPHA).
 * @ingroup   SPI
 *
 * @details El bit de control CPHA selecciona el flanco de reloj que captura los datos y permite 
 * que cambien de estado. Tiene el mayor impacto en el primer bit transmitido, permitiendo o no una
 * transición  de reloj antes del primer flanco de captura de datos. Cuando el bit de control de
 * fase CPHA es 0, los datos se capturan en la primera transición de flanco de reloj. Si el bit de
 * control de fase de reloj CPHA es 1, los datos se capturan en la segunda transición de flanco de
 * reloj.
 */
enum spi_reloj_fase {
  SPI_CPHA_0 = 0,   //!< Captura de los datos en el primer flanco.
  SPI_CPHA_1 = 1    //!< Captura de los datos en el segundo flanco.
};

/**
 * @brief     Constantes del número de bits de datos manejado por la interfaz.
 * @ingroup   SPI
 */
enum spi_tamano_datos {
  SPI_DATOS_4_BITS = 4,
  SPI_DATOS_5_BITS = 5,
  SPI_DATOS_6_BITS = 6,
  SPI_DATOS_7_BITS = 7,
  SPI_DATOS_8_BITS = 8,
  SPI_DATOS_9_BITS = 9,
  SPI_DATOS_10_BITS = 10,   
  SPI_DATOS_11_BITS = 11,
  SPI_DATOS_12_BITS = 12,
  SPI_DATOS_13_BITS = 13,
  SPI_DATOS_14_BITS = 14,
  SPI_DATOS_15_BITS = 15,
  SPI_DATOS_16_BITS = 16
};


// ===== SPI - Funciones Publicas =====
void spi_inicializar(LPC_SSP_TypeDef  *ssp_regs, uint32_t numero_bits_datos,
                     uint32_t frecuencia_sck, uint32_t cpol, uint32_t cpha,
                     LPC_GPIO_TypeDef *puerto_sck, uint32_t mascara_pin_sck,
                     LPC_GPIO_TypeDef *puerto_miso, uint32_t mascara_pin_miso,
                     LPC_GPIO_TypeDef *puerto_mosi, uint32_t mascara_pin_mosi,
                     LPC_GPIO_TypeDef *puerto_cs, uint32_t mascara_pin_cs,
                     bool_t funcion_cs);
uint16_t spi_transferir(LPC_SSP_TypeDef *ssp_regs, uint16_t dato_a_transmitir);

#endif // SPI_LPC40XX_H
