# 🔐 LPC4088 RFID Access Control System

> **Sistema de control de acceso RFID con verificación en la nube · RFID Access Control with Cloud Verification**

---

## 📋 Tabla de Contenidos · Table of Contents

- [Español](#-descripción-del-proyecto)
- [English](#-project-description)
- [Hardware](#-hardware)
- [Software Architecture](#-software-architecture--arquitectura-software)
- [Project Structure](#-project-structure--estructura-del-proyecto)
- [Setup & Usage](#-setup--uso)
- [API Reference](#-api-reference)
- [Database Schema](#-database-schema--esquema-de-base-de-datos)

---

## 🇪🇸 Descripción del Proyecto

Sistema embebido de **control de acceso mediante tarjetas RFID** desarrollado sobre la placa **LPC4088 Developer's Kit** de Embedded Artists. El sistema lee el UID de tarjetas MIFARE Classic a través de un lector RC522 (SPI/SSP), consulta en tiempo real una base de datos **Supabase** a través de un módulo **ESP32-C6** (comandos AT + HTTPS), y muestra el resultado en pantalla LCD TFT de 4.3".

### Características principales
- ✅ Lectura de tarjetas RFID MIFARE Classic (4-byte UID)
- ✅ Verificación en la nube vía HTTPS/REST (Supabase)
- ✅ Registro automático de cada intento de acceso
- ✅ Comunicación WiFi mediante ESP32-C6 con comandos AT
- ✅ Feedback visual inmediato: LCD TFT + LEDs bicolor
- ✅ Driver AT con buffer circular por interrupción UART
- ✅ Script Python de test para validar la API REST independientemente

---

## 🇬🇧 Project Description

Embedded **RFID-based access control system** developed on the **LPC4088 Developer's Kit** by Embedded Artists. The system reads MIFARE Classic card UIDs via an RC522 reader (SPI/SSP), queries a **Supabase** cloud database in real time through an **ESP32-C6** module (AT commands + HTTPS), and displays results on a 4.3" TFT LCD screen.

### Key Features
- ✅ MIFARE Classic RFID card reading (4-byte UID)
- ✅ Cloud verification via HTTPS/REST (Supabase)
- ✅ Automatic logging of every access attempt
- ✅ WiFi communication via ESP32-C6 with AT commands
- ✅ Immediate visual feedback: TFT LCD + dual-color LEDs
- ✅ AT driver with interrupt-driven circular UART buffer
- ✅ Python test script for standalone REST API validation

---

## 🔧 Hardware

| Component | Description | Interface |
|-----------|-------------|-----------|
| **LPC4088FET208** | ARM Cortex-M4 @ 120 MHz (main MCU) | — |
| **LPC4088 Developer's Kit** | Embedded Artists dev board | — |
| **LCD TFT 4.3"** | 480×272 px, 16 bpp | Parallel |
| **MFRC522 (RC522)** | RFID reader, ISO/IEC 14443-A | SPI (SSP2) |
| **ESP32-C6** | WiFi module with AT firmware | UART2 |
| **4× LEDs** | Red (LED1) + Green (LED2/3/4) | GPIO |
| **5-way Joystick** | Navigation input | GPIO PORT2 |

### Pin Mapping — RC522 (SSP2)

| RC522 Pin | Color | LPC4088 Pin | Function |
|-----------|-------|-------------|----------|
| SDA (NSS) | Grey | P2[14] | GPIO CS (manual) |
| SCK | Purple | P5[2] | SSP2_SCK |
| MOSI | Blue | P5[0] | SSP2_MOSI |
| MISO | Green | P5[1] | SSP2_MISO |
| GND | Yellow | GND | — |
| 3.3V | Orange | 3V3 | — |

### Pin Mapping — ESP32-C6 (UART2)

| Signal | LPC4088 Pin |
|--------|-------------|
| TXD | P4[22] |
| RXD | P4[23] |
| Baudrate | 115200 |

---

## 🏗 Software Architecture · Arquitectura Software

```
┌─────────────────────────────────────────────────────────┐
│                      main_final.c                       │
│          (Application logic / Lógica de aplicación)     │
└──────────────┬───────────────────────┬──────────────────┘
               │                       │
    ┌──────────▼──────────┐ ┌─────────▼────────────┐
    │  espat_lpc40xx.c/h  │ │ mfrc522_lpc40xx.c/h  │
    │  ESP32-C6 AT Driver │ │   RC522 RFID Driver   │
    │  + Supabase REST    │ │   SPI/SSP Protocol    │
    └──────────┬──────────┘ └─────────┬────────────┘
               │                       │
    ┌──────────▼──────────┐ ┌─────────▼────────────┐
    │   uart_lpc40xx.c    │ │    spi_lpc40xx.c      │
    │   UART2 + IRQ buf   │ │    SSP2 @ 1 MHz       │
    └──────────┬──────────┘ └──────────────────────┘
               │
    ┌──────────▼──────────┐
    │   Supabase Cloud    │
    │   REST API (HTTPS)  │
    │  /rest/v1/tarjetas  │
    │  /rest/v1/accesos   │
    └─────────────────────┘
```

### Startup Sequence · Secuencia de arranque

```
1. Init LCD + LEDs
2. espat_inicializar()    → UART2 + RST + ATE0
3. espat_comprobar_conexion()  → AT → OK
4. espat_wifi_establecer_modo(1)  → Station mode
5. espat_wifi_conectar_ap()   → Join AP
6. espat_wifi_ping("8.8.8.8")  → Internet check
7. espat_inicializar_http()   → SSL cfg + apikey header
8. Loop: mfrc522 → supabase_verificar → supabase_registrar
```

---

## 📁 Project Structure · Estructura del Proyecto

```
lpc4088-rfid-access-control/
│
├── src/
│   ├── main_wifi.c          # Main application (RFID + WiFi + Supabase)
│   ├── main_rfid.c          # Standalone RFID test (UID display)
│   ├── espat_lpc40xx.c      # ESP32-C6 AT driver + Supabase REST client
│   └── mfrc522_lpc40xx.c    # MFRC522 RFID reader driver (SPI/SSP)
│
├── include/
│   ├── espat_lpc40xx.h      # ESP-AT driver public API
│   └── mfrc522_lpc40xx.h    # MFRC522 driver public API + register map
│
├── scripts/
│   └── PruebaDB.py          # Python test suite for Supabase REST API
│
├── docs/
│   └── architecture.md      # Detailed architecture notes
│
├── .gitignore
└── README.md
```

---

## ⚙️ Setup · Uso

### Prerequisites · Requisitos

- **Keil µVision 5** with LPC4088 support pack
- **ESP32-C6** flashed with AT firmware v4.1+
- **Supabase** project with tables `tarjetas` and `registros_acceso`
- Python 3.x + `requests` library (for the test script)

### 1. Database Setup · Configuración de la base de datos

```sql
-- Table: tarjetas
CREATE TABLE tarjetas (
  id        SERIAL PRIMARY KEY,
  uid       TEXT UNIQUE NOT NULL,   -- e.g. "A3:FF:12:04"
  nombre    TEXT,
  activa    BOOLEAN DEFAULT TRUE,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Table: registros_acceso
CREATE TABLE registros_acceso (
  id          SERIAL PRIMARY KEY,
  uid_tarjeta TEXT NOT NULL,
  concedido   BOOLEAN NOT NULL,
  dispositivo TEXT DEFAULT 'LPC4088',
  created_at  TIMESTAMPTZ DEFAULT NOW()
);
```

### 2. Configure credentials · Configurar credenciales

Edit `include/espat_lpc40xx.h`:

```c
#define SUPABASE_HOST    "your-project.supabase.co"
#define SUPABASE_API_KEY "your_publishable_key"
```

Edit `src/main_wifi.c`:

```c
#define WIFI_SSID   "YourNetwork"
#define WIFI_PASS   "YourPassword"
```

### 3. Build & Flash

1. Open the `.uvprojx` project in Keil µVision 5
2. Build → Flash
3. Monitor UART or LCD for status messages

### 4. Run Python tests · Ejecutar tests Python

```bash
pip install requests
python scripts/PruebaDB.py
```

---

## 📡 API Reference

### `espat_lpc40xx` — ESP32-C6 / Supabase Driver

```c
// Initialize ESP module (UART + reset + echo off)
void espat_inicializar(uart, baudrate, port_tx, pin_tx, port_rx, pin_rx, timer);

// Check AT communication
bool espat_comprobar_conexion(uint16_t timeout_ms);

// WiFi
void espat_wifi_establecer_modo(uint8_t modo);   // 1=Station
void espat_wifi_conectar_ap(ssid, password);
uint32_t espat_wifi_ping(const char *host);      // returns ms, UINT32_MAX on fail

// HTTP configuration (call once after WiFi connect)
bool espat_inicializar_http(void);

// Supabase operations
supa_res_t supabase_verificar_tarjeta(const char *uid);
// → SUPA_ACTIVA | SUPA_INACTIVA | SUPA_NO_EXISTE | SUPA_ERR

supa_res_t supabase_registrar_acceso(const char *uid, bool concedido);
// → SUPA_OK | SUPA_ERR
```

### `mfrc522_lpc40xx` — RC522 RFID Driver

```c
// Initialize SPI + RC522 hardware
void mfrc522_inicializar(ssp, freq_sck, port_sck, pin_sck,
                         port_miso, pin_miso, port_mosi, pin_mosi,
                         port_cs, pin_cs, func_cs, timer);

// Non-blocking card detection (returns UID in p_uid[0..3])
bool_t mfrc522_detectar_tarjeta(uint8_t *p_uid);

// Blocking — waits until a card is presented
void mfrc522_esperar_detectar_tarjeta(uint8_t *p_uid);

// Low-level MIFARE commands
bool_t mfrc522_picc_leer_bloque(uint8_t block_addr, uint8_t *data);
bool_t mfrc522_picc_escribir_bloque(uint8_t block_addr, uint8_t *data, uint8_t len);
```

---

## 🗃 Database Schema · Esquema de Base de Datos

```
tarjetas
├── id          (PK, serial)
├── uid         (text, unique)  ← "A3:FF:12:04"
├── nombre      (text)
├── activa      (boolean)
└── created_at  (timestamptz)

registros_acceso
├── id          (PK, serial)
├── uid_tarjeta (text, FK → tarjetas.uid)
├── concedido   (boolean)
├── dispositivo (text)          ← "LPC4088"
└── created_at  (timestamptz)
```

---

## 🐛 Known Issues & Fixes · Correcciones aplicadas

The ESP-AT driver (`espat_lpc40xx.c`) incorporates several fixes over earlier revisions:

| # | Severity | Description |
|---|----------|-------------|
| BUG1 | **Critical** | `AT+HTTPURLCFG` responds `"SET OK"` not `"OK"` — caused all GETs to fail |
| BUG2 | **Critical** | `AT+HTTPCPOST` syntax: no `transport` param; SSL inferred from `https://` URL |
| BUG3 | Minor | Response accumulation: used `strncat` instead of overwriting with `strncpy` |
| BUG4 | Minor | `cmd[]` buffer enlarged to 64 bytes to safely hold header length command |
| BUG5 | Minor | Attempt counter changed from `uint8_t` to `uint16_t` to prevent silent overflow |

---

## 👨‍💻 Authors · Autores

| Module | Author | Contact |
|--------|--------|---------|
| ESP-AT Driver + Supabase | Ángel Lucas Rubio | angel.lucasrubio@alum.uca.es |
| MFRC522 RFID Driver | Alejandro Lara Doña | alejandro.lara@uca.es |

Universidad de Cádiz (UCA) · 2025/2026

---

## 📄 License

This project is licensed under the **GNU General Public License v3.0** — see the [LICENSE](LICENSE) file for details.

```
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
```
