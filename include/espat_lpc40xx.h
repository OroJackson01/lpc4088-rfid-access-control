/**
 * @file    espat_lpc40xx.h
 * @brief   Driver ESP32-C6 via AT + acceso directo a Supabase REST.
 *
 * @author    Angel Lucas - Angel.lucasrubio@alum.uca.es
 * @date      2025/2026
 * @version   4.0
 *
 * @copyright GNU General Public License version 3 or later
 */

#ifndef ESPAT_LPC40XX_H
#define ESPAT_LPC40XX_H

#include <LPC407x_8x_177x_8x.h>
#include "tipos.h"
#include "uart_lpc40xx.h"

/* ---- Configuracion UART ------------------------------------ */
#define UART_ESPAT_TAMANO_BUFFER    512
#define UART_ESPAT                  UART2
#define UART_ESPAT_IRQn             UART2_IRQn
#define UART_ESPAT_IRQHandler       UART2_IRQHandler

/* ---- Configuracion Supabase -------------------------------- */
#define SUPABASE_HOST    "************.supabase.co"
#define SUPABASE_API_KEY "sb_publishable_************_************"

/* ---- Etiquetas AT ------------------------------------------ */
#define AT_OK        "OK"
#define AT_ERROR     "ERROR"
#define AT_PING      "+PING:"

/* ---- Resultado de operaciones Supabase --------------------- */
typedef enum {
    SUPA_OK          =  0,   /* Operacion exitosa (INSERT/UPDATE) */
    SUPA_ACTIVA      =  1,   /* Tarjeta existe y esta activa      */
    SUPA_INACTIVA    =  2,   /* Tarjeta existe pero desactivada   */
    SUPA_NO_EXISTE   =  3,   /* UID no registrado en la BD        */
    SUPA_ERR         = -1,   /* Error de red o HTTP               */
} supa_res_t;

/* ====== General ============================================= */
void     espat_inicializar(LPC_UART_TypeDef *uart_regs, uint32_t baudrate,
                           LPC_GPIO_TypeDef *puerto_txd, uint32_t mascara_pin_txd,
                           LPC_GPIO_TypeDef *puerto_rxd, uint32_t mascara_pin_rxd,
                           LPC_TIM_TypeDef *timer);
bool     espat_comprobar_conexion(uint16_t timeout_ms);
void     espat_transmitir_comando(const char *comando);
bool     espat_inicializar_http(void);

/* ====== WiFi ================================================ */
void     espat_wifi_establecer_modo(uint8_t modo);
void     espat_wifi_conectar_ap(const char *ssid, const char *contrasena);
void     espat_wifi_desconectar_ap(void);
uint32_t espat_wifi_ping(const char *servidor);

/* ====== Supabase directo — sin capas intermedias ============ */
supa_res_t supabase_verificar_tarjeta(const char *uid);
supa_res_t supabase_registrar_acceso(const char *uid, bool concedido);

/* ====== Buffer UART (uso interno / diagnostico) ============= */
bool_t   espat_insertar_en_buffer(char c);
bool_t   espat_extraer_de_buffer(char *ptr);
void     espat_vaciar_buffer(void);
uint32_t espat_caracteres_en_buffer(void);
char     espat_leer(void);
char     espat_esperar_caracter(void);
bool     espat_leer_respuesta(char *ptr_buffer, uint16_t tamano_buffer,
                              uint16_t timeout_ms);
uint8_t  espat_esperar_respuesta(char *respuesta1, char *respuesta2,
                                 uint16_t timeout_ms);

#endif /* ESPAT_LPC40XX_H */