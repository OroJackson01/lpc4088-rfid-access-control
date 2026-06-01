# Post — LinkedIn / Redes Sociales

---

## 🇪🇸 Versión española

---

🔐 **Control de acceso RFID con verificación en la nube — desde un microcontrolador bare-metal**

Acabo de publicar en GitHub el código completo de uno de los proyectos más interesantes que he desarrollado este curso:

Un **sistema de control de acceso** construido sobre una placa **LPC4088 Developer's Kit** (ARM Cortex-M4 @ 120 MHz) que combina hardware embebido, comunicación SPI, WiFi y una base de datos en la nube — todo sin sistema operativo.

---

**¿Cómo funciona?**

1️⃣ El lector RFID **RC522** lee el UID de una tarjeta MIFARE Classic via SPI/SSP
2️⃣ El **LPC4088** envía el UID al módulo **ESP32-C6** por UART (comandos AT)
3️⃣ El ESP32-C6 realiza una petición **HTTPS GET** a **Supabase** para verificar si la tarjeta está activa
4️⃣ Acceso concedido o denegado → feedback en pantalla LCD TFT 4.3" y LEDs
5️⃣ Se registra automáticamente cada intento en la base de datos (HTTPS POST)

---

**Lo más interesante del proyecto:**

🔧 Driver AT propio para ESP32-C6 con buffer circular por interrupción UART
🔧 Cliente REST directo a Supabase (HTTPS sobre AT+HTTPCLIENT / AT+HTTPCPOST) — sin intermediarios
🔧 Driver MFRC522 completo: REQA → anticollisión → SELECT → autenticación MIFARE → lectura/escritura de bloques
🔧 Suite de tests en Python que simula exactamente las peticiones HTTP del microcontrolador
🔧 6 bugs documentados y corregidos en el driver AT, incluyendo dos críticos en el parsing de respuestas AT y la sintaxis de AT+HTTPCPOST

---

El reto más duro fue depurar la comunicación AT sin un debugger de red: solo la LCD y los LEDs como herramientas de diagnóstico. Aprendes muy rápido a valorar un buen buffer circular 😅

---

🔗 Repositorio: [github.com/tu-usuario/lpc4088-rfid-access-control]
📚 Stack: C bare-metal · ARM Cortex-M4 · SPI · UART · ESP32-C6 AT · Supabase · PostgreSQL · Python

---

#EmbeddedSystems #ARM #RFID #IoT #Supabase #LPC4088 #CortexM4 #BareMetalProgramming #WiFi #ESP32 #OpenSource #UCA #IngenieríaInformática

---
---

## 🇬🇧 English Version

---

🔐 **RFID Access Control with Cloud Verification — from a bare-metal microcontroller**

Just published on GitHub the full source code of one of the most interesting projects I've built this year:

An **RFID access control system** running on an **LPC4088 Developer's Kit** (ARM Cortex-M4 @ 120 MHz) that combines embedded hardware, SPI communication, WiFi, and a cloud database — all without an operating system.

---

**How it works:**

1️⃣ The **RC522** RFID reader captures a MIFARE Classic card UID via SPI/SSP
2️⃣ The **LPC4088** sends the UID to an **ESP32-C6** module over UART (AT commands)
3️⃣ The ESP32-C6 fires an **HTTPS GET** request to **Supabase** to verify whether the card is active
4️⃣ Access granted or denied → feedback on a 4.3" TFT LCD + LEDs
5️⃣ Every attempt is automatically logged to the cloud database (HTTPS POST)

---

**Highlights:**

🔧 Custom ESP32-C6 AT driver with interrupt-driven circular UART buffer
🔧 Direct REST client to Supabase (HTTPS over AT+HTTPCLIENT / AT+HTTPCPOST) — no middleware
🔧 Full MFRC522 driver: REQA → anticollision → SELECT → MIFARE auth → block read/write
🔧 Python test suite that mirrors the exact HTTP requests made by the MCU
🔧 6 documented & fixed bugs in the AT driver, including two critical ones in response parsing and AT+HTTPCPOST syntax

---

The hardest part? Debugging AT communication without a network sniffer — just an LCD and a couple of LEDs as diagnostic tools. You really learn to appreciate a good circular buffer 😅

---

🔗 Repository: [github.com/your-username/lpc4088-rfid-access-control]
📚 Stack: Bare-metal C · ARM Cortex-M4 · SPI · UART · ESP32-C6 AT · Supabase · PostgreSQL · Python

---

#EmbeddedSystems #ARM #RFID #IoT #Supabase #LPC4088 #CortexM4 #BareMetalProgramming #WiFi #ESP32 #OpenSource #EmbeddedC #Microcontrollers
