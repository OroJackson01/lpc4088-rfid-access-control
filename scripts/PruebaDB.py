"""
=============================================================
  TEST SUPABASE REST API — Proyecto LPC4088 RFID
  Simula exactamente las peticiones HTTP del microcontrolador
=============================================================
"""

import requests
import json
import random
import string
from datetime import datetime

SUPABASE_URL    = "https://************.supabase.co"
PUBLISHABLE_KEY = "sb_publishable_************_************"
SECRET_KEY      = "sb_secret_************_************"

HEADERS_ANON = {
    "apikey":        PUBLISHABLE_KEY,
    "Authorization": f"Bearer {PUBLISHABLE_KEY}",
    "Content-Type":  "application/json",
}

HEADERS_ADMIN = {
    "apikey":        SECRET_KEY,
    "Authorization": f"Bearer {SECRET_KEY}",
    "Content-Type":  "application/json",
    "Prefer":        "return=representation",
}

# ── Utilidades ───────────────────────────────────────────────

def sep(titulo):
    print(f"\n{'─'*55}\n  {titulo}\n{'─'*55}")

def mostrar(resp):
    print(f"  Status : {resp.status_code}")
    try:
        print(f"  Body   : {json.dumps(resp.json(), indent=4, ensure_ascii=False)}")
    except Exception:
        print(f"  Body   : {resp.text}")

def uid_aleatorio():
    """Genera un UID estilo RFID: XX:XX:XX:XX con hex aleatorio."""
    return ":".join(f"{random.randint(0,255):02X}" for _ in range(4))

def nombre_aleatorio():
    nombres = ["Luffy", "Zoro", "Usopp", "Nami", "Chopper",
               "Sanji", "jimbei", "Robin", "Franky", "Brook"]
    apellidos = ["El fuerte", "El astuto", "El ingenioso", "El sabio", "Del Mar",]
    return f"{random.choice(nombres)} {random.choice(apellidos)}"

# ── Operaciones base ─────────────────────────────────────────

def insertar_tarjeta(uid, nombre, activa=True):
    url  = f"{SUPABASE_URL}/rest/v1/tarjetas"
    resp = requests.post(url, headers=HEADERS_ADMIN,
                         json={"uid": uid, "nombre": nombre, "activa": activa})
    ok   = resp.status_code in (200, 201)
    print(f"  INSERT tarjeta  uid={uid}  nombre={nombre}  activa={activa}"
          f"  -> {'OK' if ok else 'FAIL ' + str(resp.status_code)}")
    return ok

def verificar_tarjeta(uid):
    url    = f"{SUPABASE_URL}/rest/v1/tarjetas"
    params = {"uid": f"eq.{uid}", "select": "uid,nombre,activa"}
    resp   = requests.get(url, headers=HEADERS_ANON, params=params)
    if resp.status_code == 200:
        data = resp.json()
        if data and data[0].get("activa"):
            print(f"  GET tarjeta     uid={uid}  -> ACTIVA (acceso concedido)")
            return True
        elif data:
            print(f"  GET tarjeta     uid={uid}  -> INACTIVA (acceso denegado)")
            return False
        else:
            print(f"  GET tarjeta     uid={uid}  -> NO ENCONTRADA")
            return False
    print(f"  GET tarjeta     uid={uid}  -> ERROR {resp.status_code}")
    return False

def registrar_acceso(uid, concedido, dispositivo="LPC4088"):
    url  = f"{SUPABASE_URL}/rest/v1/registros_acceso"
    resp = requests.post(url, headers=HEADERS_ANON,
                         json={"uid_tarjeta": uid,
                               "concedido":   concedido,
                               "dispositivo": dispositivo})
    ok   = resp.status_code in (200, 201)
    print(f"  POST acceso     uid={uid}  concedido={concedido}"
          f"  -> {'OK' if ok else 'FAIL ' + str(resp.status_code)}")
    return ok

def borrar_tarjeta(uid):
    url  = f"{SUPABASE_URL}/rest/v1/tarjetas"
    resp = requests.delete(url, headers=HEADERS_ADMIN,
                           params={"uid": f"eq.{uid}"})
    ok   = resp.status_code in (200, 204)
    print(f"  DELETE tarjeta  uid={uid}  -> {'OK' if ok else 'FAIL ' + str(resp.status_code)}")
    return ok

# ── Tests ────────────────────────────────────────────────────

def test_1_conexion():
    sep("TEST 1 — Conexion con Supabase")
    url  = f"{SUPABASE_URL}/rest/v1/tarjetas?limit=1"
    resp = requests.get(url, headers=HEADERS_ANON)
    mostrar(resp)
    print("  ✅ OK" if resp.status_code in (200, 401, 403) else "  ❌ FALLO")

def test_2_insertar_lote():
    sep("TEST 2 — Insertar lote de tarjetas aleatorias")
    tarjetas = []
    # 4 tarjetas activas
    for _ in range(4):
        uid    = uid_aleatorio()
        nombre = nombre_aleatorio()
        if insertar_tarjeta(uid, nombre, activa=True):
            tarjetas.append((uid, True))
    # 2 tarjetas inactivas
    for _ in range(2):
        uid    = uid_aleatorio()
        nombre = nombre_aleatorio()
        if insertar_tarjeta(uid, nombre, activa=False):
            tarjetas.append((uid, False))
    return tarjetas

def test_3_verificar_y_loguear(tarjetas):
    sep("TEST 3 — Verificar tarjetas y registrar accesos")
    for uid, esperado_activo in tarjetas:
        concedido = verificar_tarjeta(uid)
        registrar_acceso(uid, concedido)

def test_4_tarjetas_desconocidas():
    sep("TEST 4 — Intentos con tarjetas desconocidas")
    for _ in range(4):
        uid = uid_aleatorio()
        concedido = verificar_tarjeta(uid)
        registrar_acceso(uid, concedido)

def test_5_multiples_accesos(tarjetas_activas):
    sep("TEST 5 — Multiples accesos con tarjetas activas")
    dispositivos = ["LPC4088", "LPC4088-B", "LPC4088-LAB"]
    for _ in range(8):
        uid  = random.choice(tarjetas_activas)
        disp = random.choice(dispositivos)
        registrar_acceso(uid, True, dispositivo=disp)

def test_6_desactivar_y_verificar(tarjetas):
    sep("TEST 6 — Desactivar tarjeta y verificar denegacion")
    uid_a_desactivar = tarjetas[0][0]
    # Desactivar
    url  = f"{SUPABASE_URL}/rest/v1/tarjetas"
    resp = requests.patch(url, headers=HEADERS_ADMIN,
                          params={"uid": f"eq.{uid_a_desactivar}"},
                          json={"activa": False})
    print(f"  PATCH activa=false  uid={uid_a_desactivar}"
          f"  -> {resp.status_code}")
    # Verificar que ahora deniega
    concedido = verificar_tarjeta(uid_a_desactivar)
    registrar_acceso(uid_a_desactivar, concedido)
    print(f"  {'✅ Denegado correctamente' if not concedido else '❌ Deberia haber denegado'}")

def test_7_leer_ultimos_registros():
    sep("TEST 7 — Ultimos 10 registros de acceso")
    url    = f"{SUPABASE_URL}/rest/v1/registros_acceso"
    params = {"select": "*", "order": "created_at.desc", "limit": "10"}
    resp   = requests.get(url, headers=HEADERS_ADMIN, params=params)
    mostrar(resp)

def test_8_limpiar_tarjetas(tarjetas):
    sep("TEST 8 — Limpiar tarjetas de prueba")
    for uid, _ in tarjetas:
        borrar_tarjeta(uid)

# ── Main ─────────────────────────────────────────────────────

if __name__ == "__main__":
    print(f"\n  Supabase : {SUPABASE_URL}")
    print(f"  Hora     : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    test_1_conexion()

    tarjetas          = test_2_insertar_lote()
    tarjetas_activas  = [uid for uid, activo in tarjetas if activo]

    test_3_verificar_y_loguear(tarjetas)
    test_4_tarjetas_desconocidas()
    test_5_multiples_accesos(tarjetas_activas)
    test_6_desactivar_y_verificar(tarjetas)
    test_7_leer_ultimos_registros()
    test_8_limpiar_tarjetas(tarjetas)

    print(f"\n{'─'*55}")
    print("  Todos los tests completados")
    print(f"{'─'*55}\n")