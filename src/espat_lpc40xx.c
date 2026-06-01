/**
 * @file    espat_lpc40xx.c
 * @brief   Driver ESP32-C6 via AT + acceso directo a Supabase REST.
 *
 * @author    Angel Lucas - Angel.lucasrubio@alum.uca.es
 * @date      2025/2026
 * @version   6.0
 *
 * @copyright GNU General Public License version 3 or later
 *
 * CORRECCIONES v6.0 respecto a v5.1:
 *
 * BUG 1 [CRITICO] http_set_url esperaba "OK" pero AT+HTTPURLCFG
 *   responde "SET OK". Con v5.1 http_set_url SIEMPRE retornaba FALSE
 *   y por tanto http_get SIEMPRE retornaba FALSE -> SUPA_ERR.
 *   FIX: esperar_respuesta busca "SET OK" en lugar de "OK".
 *
 * BUG 2 [CRITICO] AT+HTTPCPOST sintaxis incorrecta.
 *   La firma real es: AT+HTTPCPOST=<"url">,<len>[,<cnt>][,<"header">...]
 *   No existe parametro "transport" en HTTPCPOST (solo en HTTPCLIENT).
 *   El transport SSL se hereda de AT+HTTPCFG / la URL https://.
 *   FIX: AT+HTTPCPOST="url",len,1,"Content-Type: application/json"
 *        y ademas se usa AT+HTTPURLCFG para la URL del POST (evita
 *        superar el limite de 256 bytes por comando AT).
 *
 * BUG 3 [MENOR] http_leer_respuesta sobreescribia buf con strncpy
 *   en cada linea +HTTPCLIENT recibida (si la respuesta llega en
 *   varios chunks, solo se guardaba el ultimo).
 *   FIX: acumular con strncat hasta llenar el buffer.
 *
 * BUG 4 [MENOR] header_len en espat_inicializar_http no verificado
 *   si cabe en el buffer cmd[50]. Con la clave actual (39 chars) el
 *   comando "AT+HTTPCHEAD=51\r\n" mide 18 bytes -> OK, pero se
 *   amplia cmd a 64 para mayor seguridad.
 *
 * BUG 5 [MENOR] http_leer_respuesta: contador intentos uint8_t
 *   podia desbordarse silenciosamente. Cambiado a uint16_t.
 *
 * NOTA: AT+HTTPCFG=0 puede no estar soportado en todos los builds
 *   del firmware v4.1. Si espat_inicializar_http() retorna FALSE
 *   en la LCD, comentar esa linea y probar con AT+CIPSSLCCONF=0.
 */

#include "espat_lpc40xx.h"
#include <LPC407x_8x_177x_8x.h>
#include <string.h>
#include <stdlib.h>
#include "tipos.h"
#include "error.h"
#include "glcd.h"
#include "iocon_lpc40xx.h"
#include "uart_lpc40xx.h"
#include "timer_lpc40xx.h"

/* ---- Variables privadas ------------------------------------ */
static char      buffer_recibidos[UART_ESPAT_TAMANO_BUFFER];
static uint32_t  indice_escritura            = 0;
static uint32_t  indice_lectura              = 0;
static uint32_t  numero_caracteres_en_buffer = 0;
static LPC_UART_TypeDef *espat_uart;
static LPC_TIM_TypeDef  *espat_timer;

/* ============================================================
   BUFFER CIRCULAR
   ============================================================ */

void UART2_IRQHandler(void)
{
    if ((espat_uart->IIR & 0x0F) == (0x2 << 1))
        espat_insertar_en_buffer(espat_uart->RBR);
}

bool_t espat_insertar_en_buffer(char c)
{
    if (numero_caracteres_en_buffer == UART_ESPAT_TAMANO_BUFFER) return FALSE;
    NVIC_DisableIRQ(UART_ESPAT_IRQn);
    buffer_recibidos[indice_escritura] = c;
    numero_caracteres_en_buffer++;
    indice_escritura = (indice_escritura < UART_ESPAT_TAMANO_BUFFER - 1)
                       ? indice_escritura + 1 : 0;
    NVIC_EnableIRQ(UART_ESPAT_IRQn);
    return TRUE;
}

bool_t espat_extraer_de_buffer(char *ptr)
{
    ASSERT(ptr != NULL, "Puntero ptr nulo.");
    if (numero_caracteres_en_buffer == 0) return FALSE;
    NVIC_DisableIRQ(UART_ESPAT_IRQn);
    *ptr = buffer_recibidos[indice_lectura];
    numero_caracteres_en_buffer--;
    indice_lectura = (indice_lectura < UART_ESPAT_TAMANO_BUFFER - 1)
                     ? indice_lectura + 1 : 0;
    NVIC_EnableIRQ(UART_ESPAT_IRQn);
    return TRUE;
}

void espat_vaciar_buffer(void)
{
    NVIC_DisableIRQ(UART_ESPAT_IRQn);
    indice_escritura            = 0;
    indice_lectura              = 0;
    numero_caracteres_en_buffer = 0;
    NVIC_EnableIRQ(UART_ESPAT_IRQn);
}

uint32_t espat_caracteres_en_buffer(void)
{
    return numero_caracteres_en_buffer;
}

char espat_leer(void)
{
    char c;
    return espat_extraer_de_buffer(&c) ? c : 0;
}

char espat_esperar_caracter(void)
{
    char c;
    while (!espat_extraer_de_buffer(&c));
    return c;
}

/**
 * @brief   Leer una linea completa hasta '\n' o timeout.
 *          Ignora CR. Retorna TRUE si se completo una linea.
 */
bool espat_leer_respuesta(char *ptr_buffer, uint16_t tamano_buffer,
                          uint16_t timeout_ms)
{
    char c;
    uint16_t pos = 0;

    timer_iniciar_conteo_ms(espat_timer);
    do {
        c = espat_leer();
        if (c == '\r') continue;
        if ((c >= 0x20) && (pos < tamano_buffer - 2)) {
            ptr_buffer[pos++] = c;
        } else if ((c == '\n') && (pos > 0)) {
            ptr_buffer[pos] = 0;
            return TRUE;
        }
    } while (timer_leer(espat_timer) <= timeout_ms);

    ptr_buffer[pos] = 0;
    return FALSE;
}

/**
 * @brief   Esperar una de dos respuestas exactas en cualquier linea.
 * @retval  1=respuesta1, 0=respuesta2, -1=timeout.
 */
uint8_t espat_esperar_respuesta(char *respuesta1, char *respuesta2,
                                uint16_t timeout_ms)
{
    char buf[100];
    uint16_t pos = 0;

    timer_iniciar_conteo_ms(espat_timer);
    while (timer_leer(espat_timer) <= timeout_ms) {
        char c = espat_leer();
        if (c == '\r') continue;
        if (c >= 0x20 && pos < 98) {
            buf[pos++] = c;
        } else if (c == '\n' && pos > 0) {
            buf[pos] = 0;
            if (strcmp(buf, respuesta1) == 0) return 1;
            if (strcmp(buf, respuesta2) == 0) return 0;
            pos = 0;
        }
    }
    return (uint8_t)-1;
}

/* ============================================================
   GENERAL
   ============================================================ */

void espat_transmitir_comando(const char *comando)
{
    uart_transmitir_cadena(espat_uart, (char *)comando);
}

/**
 * @brief   Inicializar modulo ESP32-C6.
 *          Secuencia: RST -> espera 3s -> ATE0 (desactiva eco).
 *          La configuracion HTTP (SSL + apikey) se hace por separado
 *          en espat_inicializar_http(), despues de conectar WiFi.
 */
void espat_inicializar(LPC_UART_TypeDef *uart_regs, uint32_t baudrate,
                       LPC_GPIO_TypeDef *puerto_txd, uint32_t mascara_pin_txd,
                       LPC_GPIO_TypeDef *puerto_rxd, uint32_t mascara_pin_rxd,
                       LPC_TIM_TypeDef *timer)
{
    uart_inicializar(uart_regs, baudrate,
                     UART_BITS_DATOS_8, UART_PARIDAD_NINGUNA, UART_BITS_STOP_1,
                     puerto_txd, mascara_pin_txd,
                     puerto_rxd, mascara_pin_rxd, NULL);
    espat_uart  = uart_regs;
    espat_timer = timer;
    timer_inicializar(espat_timer);

    NVIC_ClearPendingIRQ(UART_ESPAT_IRQn);
    uart_habilitar_interrupciones_dato_recibido(uart_regs, TRUE);
    NVIC_SetPriority(UART_ESPAT_IRQn, 0);
    NVIC_EnableIRQ(UART_ESPAT_IRQn);
    __enable_irq();

    /* Reset hardware y espera arranque completo */
    espat_transmitir_comando("AT+RST\r\n");
    timer_retardo_ms(espat_timer, 3000);
    espat_vaciar_buffer();

    /* Desactivar eco: SIN eco el parseo de respuestas es fiable */
    espat_transmitir_comando("ATE0\r\n");
    timer_retardo_ms(espat_timer, 500);
    espat_vaciar_buffer();
}

bool espat_comprobar_conexion(uint16_t timeout_ms)
{
    espat_vaciar_buffer();
    espat_transmitir_comando("AT\r\n");
    return (espat_esperar_respuesta("OK", "ERROR", timeout_ms) == 1);
}

/**
 * @brief   Configurar HTTP global: SSL sin certificado + cabecera apikey.
 *          Llamar UNA VEZ despues de conectar WiFi y antes del bucle.
 *
 *          Comandos enviados:
 *            AT+HTTPCFG=0          -> SSL, sin verificar cert servidor
 *            AT+HTTPCHEAD=<len>    -> prompt '>'
 *            apikey: <KEY>\r\n     -> cabecera global persistente
 *
 * @retval  TRUE si todo OK, FALSE si algun paso falla (ver LCD en main).
 */

bool espat_inicializar_http(void)
{
    /* --- Paso 1: SSL sin verificacion de certificado --- */
    espat_vaciar_buffer();
    espat_transmitir_comando("AT+HTTPCFG=0\r\n");
    if (espat_esperar_respuesta("OK", "ERROR", 3000) != 1) return FALSE;
    timer_retardo_ms(espat_timer, 200);

    /* --- Paso 2: Cabecera apikey global ---
     * "apikey: " = 8 chars
     * KEY        = strlen(SUPABASE_API_KEY) chars
     * "\r\n"     = 2 chars
     */
    static char cmd1[64];
    uint16_t apikey_len = (uint16_t)(8u + strlen(SUPABASE_API_KEY) + 2u);
    snprintf(cmd1, sizeof(cmd1), "AT+HTTPCHEAD=%u\r\n", apikey_len);
    espat_vaciar_buffer();
    espat_transmitir_comando(cmd1);
    timer_iniciar_conteo_ms(espat_timer);
    while (timer_leer(espat_timer) < 5000) {
        if (espat_leer() == '>') break;
    }
    static char apikey_header[128];
    snprintf(apikey_header, sizeof(apikey_header),
             "apikey: %s\r\n", SUPABASE_API_KEY);
    espat_transmitir_comando(apikey_header);
    if (espat_esperar_respuesta("OK", "ERROR", 3000) != 1) return FALSE;
    timer_retardo_ms(espat_timer, 200);

    /* --- Paso 3: Cabecera Authorization Bearer ---
     * "Authorization: Bearer " = 22 chars
     * KEY                      = strlen(SUPABASE_API_KEY) chars
     * "\r\n"                   = 2 chars
     */
    static char cmd2[64];
    uint16_t auth_len = (uint16_t)(22u + strlen(SUPABASE_API_KEY) + 2u);
    snprintf(cmd2, sizeof(cmd2), "AT+HTTPCHEAD=%u\r\n", auth_len);
    espat_vaciar_buffer();
    espat_transmitir_comando(cmd2);
    timer_iniciar_conteo_ms(espat_timer);
    while (timer_leer(espat_timer) < 5000) {
        if (espat_leer() == '>') break;
    }
    static char auth_header[128];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s\r\n", SUPABASE_API_KEY);
    espat_transmitir_comando(auth_header);
    if (espat_esperar_respuesta("OK", "ERROR", 3000) != 1) return FALSE;

    espat_vaciar_buffer();
    return TRUE;
}

bool espat_inicializar_http_old(void){
    /* --- Paso 1: SSL sin verificacion de certificado --- */
    espat_vaciar_buffer();
    espat_transmitir_comando("AT+HTTPCFG=0\r\n");
    if (espat_esperar_respuesta("OK", "ERROR", 3000) != 1) return FALSE;
    timer_retardo_ms(espat_timer, 200);

    /* --- Paso 2: Cabecera apikey global ---
     * Longitud exacta de "apikey: <KEY>\r\n":
     *   "apikey: " = 8 chars
     *   KEY        = strlen(SUPABASE_API_KEY) chars
     *   "\r\n"     = 2 chars
     */
    static char cmd[64];
    uint16_t header_len = (uint16_t)(8u + strlen(SUPABASE_API_KEY) + 2u);
    snprintf(cmd, sizeof(cmd), "AT+HTTPCHEAD=%u\r\n", header_len);

    espat_vaciar_buffer();
    espat_transmitir_comando(cmd);

    /* Esperar el prompt '>' (llega sin \n, hay que leer caracter a caracter) */
    timer_iniciar_conteo_ms(espat_timer);
    while (timer_leer(espat_timer) < 5000) {
        if (espat_leer() == '>') break;
    }

    /* Enviar la cabecera tal cual (con \r\n al final como exige el firmware) */
    static char header[128];
    snprintf(header, sizeof(header), "apikey: %s\r\n", SUPABASE_API_KEY);
    espat_transmitir_comando(header);

    if (espat_esperar_respuesta("OK", "ERROR", 3000) != 1) return FALSE;

    espat_vaciar_buffer();
    return TRUE;
}

/* ============================================================
   WIFI
   ============================================================ */

void espat_wifi_establecer_modo(uint8_t modo)
{
    char cmd[20];
    ASSERT(modo <= 3, "Modo WiFi invalido");
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d\r\n", modo);
    espat_transmitir_comando(cmd);
    espat_esperar_respuesta("OK", "ERROR", 2000);
    espat_vaciar_buffer();
    timer_retardo_ms(espat_timer, 200);
}

void espat_wifi_conectar_ap(const char *ssid, const char *contrasena)
{
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, contrasena);
    espat_transmitir_comando(cmd);
    espat_esperar_respuesta("OK", "ERROR", 15000);
    espat_vaciar_buffer();
    timer_retardo_ms(espat_timer, 500);
}

void espat_wifi_desconectar_ap(void)
{
    espat_transmitir_comando("AT+CWQAP\r\n");
    timer_retardo_ms(espat_timer, 100);
}

uint32_t espat_wifi_ping(const char *servidor)
{
    char cmd[80];
    char linea[80];
    char *p;

    snprintf(cmd, sizeof(cmd), "AT+PING=\"%s\"\r\n", servidor);
    espat_vaciar_buffer();
    espat_transmitir_comando(cmd);

    uint8_t intentos = 0;
    while (intentos < 15) {
        if (espat_leer_respuesta(linea, sizeof(linea), 3000)) {
            p = strstr(linea, AT_PING);
            if (p) {
                if (strstr(p, "TIMEOUT")) return UINT32_MAX;
                p = strstr(p, ":");
                if (p) return (uint32_t)atoi(p + 1);
            }
            if (strstr(linea, "ERROR")) return UINT32_MAX;
        }
        intentos++;
    }
    return UINT32_MAX;
}

/* ============================================================
   HTTP INTERNO
   ============================================================ */

/**
 * @brief   Esperar el prompt '>' del ESP (llega sin '\n').
 * @retval  TRUE si se recibio '>' antes del timeout.
 */
static bool http_esperar_prompt(uint16_t timeout_ms)
{
    timer_iniciar_conteo_ms(espat_timer);
    while (timer_leer(espat_timer) < timeout_ms) {
        if (espat_leer() == '>') return TRUE;
    }
    return FALSE;
}

/**
 * @brief   Precargar URL con AT+HTTPURLCFG.
 *
 * Segun el manual (seccion 3.6.7):
 *   Comando: AT+HTTPURLCFG=<url_length>
 *   Respuesta: OK > (esperar '>')
 *   Enviar URL raw (sin \r\n al final)
 *   Respuesta final: SET OK   <-- *** NO "OK", sino "SET OK" ***
 *
 * @retval  TRUE si el ESP confirmo "SET OK".
 */
static bool http_set_url(const char *url)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+HTTPURLCFG=%u\r\n", (uint16_t)strlen(url));
    espat_transmitir_comando(cmd);

    /* Esperar OK seguido de '>' */
    if (espat_esperar_respuesta("OK", "ERROR", 3000) != 1) return FALSE;
    if (!http_esperar_prompt(2000)) return FALSE;

    /* Enviar URL (sin \r\n — el firmware cuenta bytes exactos) */
    espat_transmitir_comando(url);

    /* FIX BUG 1: la respuesta es "SET OK", no "OK" */
    return (espat_esperar_respuesta("SET OK", "ERROR", 5000) == 1);
}

/**
 * @brief   Leer todas las lineas +HTTPCLIENT y esperar OK o ERROR.
 *
 * Formato de linea: +HTTPCLIENT:<len>,<datos>
 * Pueden llegar multiples lineas si la respuesta es larga.
 * Se acumulan en buf con strncat (FIX BUG 3).
 *
 * @retval  TRUE si se recibio OK (con o sin datos), FALSE si ERROR o timeout.
 */
static bool http_leer_respuesta(char *buf, uint32_t buf_size)
{
    static char linea[400];
    uint16_t intentos = 0;   /* FIX BUG 5: uint16_t en lugar de uint8_t */
    bool     got_data = FALSE;

    buf[0] = 0;

    while (intentos < 60) {
        if (espat_leer_respuesta(linea, sizeof(linea), 5000)) {

            char *p = strstr(linea, "+HTTPCLIENT:");
            if (p) {
                /* Saltamos "<len>," para apuntar directamente a los datos */
                p = strstr(p, ",");
                if (p) {
                    /* FIX BUG 3: acumular en lugar de sobreescribir */
                    uint32_t usado = strlen(buf);
                    if (usado < buf_size - 1) {
                        strncat(buf, p + 1, buf_size - 1 - usado);
                        buf[buf_size - 1] = 0;
                    }
                    got_data = TRUE;
                }
            }

            /* Comprobar fin de transaccion */
            if (strstr(linea, "ERROR"))  return FALSE;
            if (strstr(linea, "OK"))     return got_data;

            /* Respuesta del POST: "SEND OK" tambien indica exito */
            if (strstr(linea, "SEND OK")) return TRUE;
        }
        intentos++;
    }
    return got_data;
}

/**
 * @brief   GET HTTPS a Supabase.
 *
 * Flujo:
 *   1. Vaciar buffer
 *   2. AT+HTTPURLCFG=<len> -> OK -> '>' -> <url> -> "SET OK"
 *   3. AT+HTTPCLIENT=2,0,"","","",2  (GET, JSON, url vacia, SSL)
 *   4. Leer +HTTPCLIENT y OK
 *
 * @param   path   Ruta + query string, p.ej. "/rest/v1/tarjetas?uid=eq.XX"
 * @param   buf    Buffer de salida para el cuerpo de la respuesta
 * @param   buf_size Tamano del buffer de salida
 */
static bool http_get(const char *path, char *buf, uint32_t buf_size)
{
    static char url[300];

    snprintf(url, sizeof(url), "https://%s%s", SUPABASE_HOST, path);

    espat_vaciar_buffer();
    if (!http_set_url(url)) return FALSE;

    /* GET (opt=2), content-type=0 (form), url/host/path vacios, SSL (transport=2) */
    espat_transmitir_comando("AT+HTTPCLIENT=2,0,\"\",\"\",\"\",2\r\n");
    return http_leer_respuesta(buf, buf_size);
}

/**
 * @brief   POST HTTPS a Supabase con body JSON.
 *
 * Flujo (FIX BUG 2 — sintaxis correcta de HTTPCPOST):
 *   1. Vaciar buffer
 *   2. AT+HTTPURLCFG=<len> -> OK -> '>' -> <url> -> "SET OK"
 *      (necesario porque la URL supera facilmente los 256 bytes del cmd)
 *   3. AT+HTTPCPOST="",<body_len>,1,"Content-Type: application/json"
 *      (url vacia porque ya se cargo con URLCFG; 1 header extra)
 *   4. Esperar '>' y enviar body JSON
 *   5. Leer "SEND OK" / OK / ERROR
 *
 * Nota: AT+HTTPCPOST NO tiene parametro transport. El protocolo se
 *       determina por el esquema https:// de la URL.
 *
 * @param   path       Ruta, p.ej. "/rest/v1/registros_acceso"
 * @param   body_json  Cuerpo JSON a enviar
 * @param   buf        Buffer de salida (respuesta del servidor)
 * @param   buf_size   Tamano del buffer de salida
 */
 static bool http_post(const char *path, const char *body_json,
                      char *buf, uint32_t buf_size)
{
    static char cmd[180];
    static char url[200];
    uint16_t body_len = (uint16_t)strlen(body_json);

    snprintf(url, sizeof(url), "https://%s%s", SUPABASE_HOST, path);

    /* URL inline directamente — suficientemente corta para POST de Supabase */
    snprintf(cmd, sizeof(cmd),
             "AT+HTTPCPOST=\"%s\",%u,1,\"Content-Type: application/json\"\r\n",
             url, body_len);

    espat_vaciar_buffer();
    espat_transmitir_comando(cmd);
    if (!http_esperar_prompt(5000)) return FALSE;
    espat_transmitir_comando(body_json);
    return http_leer_respuesta(buf, buf_size);
}
 
 
static bool http_post_old(const char *path, const char *body_json,
                      char *buf, uint32_t buf_size){
    static char url[200];
    static char cmd[100];
    uint16_t body_len = (uint16_t)strlen(body_json);

    snprintf(url, sizeof(url), "https://%s%s", SUPABASE_HOST, path);

    /* Cargar URL con HTTPURLCFG (evita superar limite de 256 bytes) */
    espat_vaciar_buffer();
    if (!http_set_url(url)) return FALSE;

    /*
     * FIX BUG 2: Sintaxis correcta de AT+HTTPCPOST (seccion 3.6.5 del manual):
     *   AT+HTTPCPOST=<"url">,<length>[,<header_cnt>][,<"header">...]
     *
     * - url vacia "" porque ya cargamos la URL con HTTPURLCFG
     * - body_len = longitud exacta del JSON
     * - 1 cabecera adicional: Content-Type (apikey ya esta configurada globalmente)
     *
     * NO existe parametro transport en HTTPCPOST; el esquema https://
     * ya implica SSL en el firmware v4.1.
     */
    snprintf(cmd, sizeof(cmd),
             "AT+HTTPCPOST=\"\",%u,1,\"Content-Type: application/json\"\r\n",
             body_len);

    espat_transmitir_comando(cmd);

    /* Esperar el prompt '>' para enviar el body */
    if (!http_esperar_prompt(5000)) return FALSE;
    espat_transmitir_comando(body_json);

    return http_leer_respuesta(buf, buf_size);
}

/* ============================================================
   SUPABASE — API publica
   ============================================================ */

/**
 * @brief   Verificar si un UID esta registrado y activo en Supabase.
 *
 * GET /rest/v1/tarjetas?uid=eq.<uid>&select=uid,activa
 * Respuesta esperada: [{"uid":"...","activa":true}]
 *                  o: [{"uid":"...","activa":false}]
 *                  o: []   (no encontrada)
 *
 * @retval  SUPA_ACTIVA, SUPA_INACTIVA, SUPA_NO_EXISTE, o SUPA_ERR
 */
supa_res_t supabase_verificar_tarjeta(const char *uid)
{
    static char path[120];
    static char resp[512];

    snprintf(path, sizeof(path),
             "/rest/v1/tarjetas?uid=eq.%s&select=uid,activa", uid);

    if (!http_get(path, resp, sizeof(resp))) return SUPA_ERR;

    if (strstr(resp, "\"activa\":true"))  return SUPA_ACTIVA;
    if (strstr(resp, "\"activa\":false")) return SUPA_INACTIVA;
    if (strstr(resp, "[]"))              return SUPA_NO_EXISTE;
    if (resp[0] == '[')                  return SUPA_NO_EXISTE; /* array vacio alternativo */

    return SUPA_ERR;
}

/**
 * @brief   Registrar un intento de acceso en Supabase.
 *
 * POST /rest/v1/registros_acceso
 * Body: {"uid_tarjeta":"...","concedido":true/false,"dispositivo":"LPC4088"}
 *
 * Supabase devuelve 201 Created con el registro insertado, o un error.
 * El firmware AT mapea HTTP 2xx a OK en la respuesta serial.
 *
 * @retval  SUPA_OK si el servidor acepto el registro, SUPA_ERR si no.
 */
supa_res_t supabase_registrar_acceso(const char *uid, bool concedido)
{
    static const char path[] = "/rest/v1/registros_acceso";
    static char body[160];
    static char resp[256];

    snprintf(body, sizeof(body),
             "{\"uid_tarjeta\":\"%s\","
             "\"concedido\":%s,"
             "\"dispositivo\":\"LPC4088\"}",
             uid, concedido ? "true" : "false");

    if (!http_post(path, body, resp, sizeof(resp))) return SUPA_ERR;
    return SUPA_OK;
}