# Architecture Notes · Notas de Arquitectura

## Communication Flow

```
[MIFARE Card] ──RF──> [RC522] ──SPI/SSP2──> [LPC4088]
                                                  │
                                             UART2 (115200)
                                                  │
                                             [ESP32-C6]
                                                  │
                                          WiFi (802.11)
                                                  │
                                        HTTPS (AT commands)
                                                  │
                                          [Supabase Cloud]
                                        REST API / PostgreSQL
```

## AT Command Sequence for a card verification (GET)

```
LPC4088 → ESP32:  AT+HTTPURLCFG=72\r\n
ESP32   → LPC4088: OK\r\n>\r\n
LPC4088 → ESP32:  https://project.supabase.co/rest/v1/tarjetas?uid=eq.XX:XX:XX:XX&select=uid,activa
ESP32   → LPC4088: SET OK\r\n
LPC4088 → ESP32:  AT+HTTPCLIENT=2,0,"","","",2\r\n
ESP32   → LPC4088: +HTTPCLIENT:40,[{"uid":"XX:XX:XX:XX","activa":true}]\r\nOK\r\n
```

## AT Command Sequence for access logging (POST)

```
LPC4088 → ESP32:  AT+HTTPCPOST="https://.../rest/v1/registros_acceso",60,1,"Content-Type: application/json"\r\n
ESP32   → LPC4088: >\r\n
LPC4088 → ESP32:  {"uid_tarjeta":"XX:XX:XX:XX","concedido":true,"dispositivo":"LPC4088"}
ESP32   → LPC4088: SEND OK\r\nOK\r\n
```

## Interrupt-Driven UART Buffer

The ESP-AT driver uses a 512-byte circular buffer fed by `UART2_IRQHandler`.
All AT response parsing is done by polling `espat_leer()` with a timer-based timeout,
keeping the architecture simple and predictable on a bare-metal system.

## RC522 SPI Protocol

The MFRC522 uses a custom SPI addressing scheme:
- **Write**: `(register << 1) & 0x7E`  → bit7=0, bit0=0
- **Read**:  `((register << 1) & 0x7E) | 0x80`  → bit7=1, bit0=0

CS is managed manually as a GPIO (P2[14]) with 1 µs setup/hold delays.
