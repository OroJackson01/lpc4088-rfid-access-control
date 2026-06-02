"""
=============================================================
  RFID ACCESS DASHBOARD — Proyecto LPC4088
  Dashboard Streamlit con auto-refresco del log de accesos
  Angel Lucas - Angel.lucasrubio@alum.uca.es || DBM/2026
=============================================================
  Instalacion Paquetes:
      pip install streamlit requests pandas

  Ejecucion Paquete:
      python -m streamlit run RFID_DashBoard.py
=============================================================
"""

import streamlit as st
import requests
import pandas as pd
from datetime import datetime
import time

# ─────────────────────────────────────────────
#  CONFIGURACIÓN
# ─────────────────────────────────────────────
SUPABASE_URL    = "https://XXXXXXX.supabase.co"
PUBLISHABLE_KEY = "sb_publishable_XXXXXXX_XXXXXXX"
SECRET_KEY      = "sb_secret_XXXXXXX_XXXXXXX"

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

# ─────────────────────────────────────────────
#  FUNCIONES DE API
# ─────────────────────────────────────────────
def get_registros(limit: int = 100) -> list[dict]:
    """Obtiene los últimos N registros de acceso ordenados por fecha desc."""
    url = f"{SUPABASE_URL}/rest/v1/registros_acceso"
    params = {
        "select": "id,uid_tarjeta,concedido,dispositivo,created_at",  
        "order":  "created_at.desc",                                   
        "limit":  str(limit),
    }
    try:
        resp = requests.get(url, headers=HEADERS_ADMIN, params=params, timeout=8)
        if resp.status_code == 200:
            return resp.json()
    except requests.RequestException:
        pass
    return []


def get_tarjetas() -> list[dict]:
    """Obtiene todas las tarjetas registradas."""
    url = f"{SUPABASE_URL}/rest/v1/tarjetas"
    params = {"select": "uid,nombre,activa,created_at,modified_at"}  
    try:
        resp = requests.get(url, headers=HEADERS_ADMIN, params=params, timeout=8)
        if resp.status_code == 200:
            return resp.json()
    except requests.RequestException:
        pass
    return []


def insertar_tarjeta(uid: str, nombre: str) -> tuple[bool, str]:
    """Inserta una nueva tarjeta. Devuelve (éxito, mensaje)."""
    uid = uid.strip().upper()
    if not uid or not nombre.strip():
        return False, "UID y nombre son obligatorios."
    url = f"{SUPABASE_URL}/rest/v1/tarjetas"
    payload = {"uid": uid, "nombre": nombre.strip(), "activa": True}
    try:
        resp = requests.post(url, headers=HEADERS_ADMIN, json=payload, timeout=8)
        if resp.status_code in (200, 201):
            return True, f"Tarjeta **{uid}** insertada correctamente."
        if resp.status_code == 409:
            return False, f"La tarjeta **{uid}** ya existe en la base de datos."
        return False, f"Error HTTP {resp.status_code}: {resp.text[:120]}"
    except requests.RequestException as e:
        return False, f"Error de red: {e}"


def cambiar_estado_tarjeta(uid: str, activa: bool) -> bool:
    """Activa o desactiva una tarjeta existente."""
    url = f"{SUPABASE_URL}/rest/v1/tarjetas"
    params = {"uid": f"eq.{uid}"}
    try:
        resp = requests.patch(url, headers=HEADERS_ADMIN, params=params,
                              json={"activa": activa}, timeout=8)
        return resp.status_code in (200, 204)
    except requests.RequestException:
        return False


def ping_supabase() -> bool:
    """Comprueba si Supabase responde."""
    try:
        resp = requests.get(
            f"{SUPABASE_URL}/rest/v1/tarjetas?limit=1",
            headers=HEADERS_ANON, timeout=5
        )
        return resp.status_code in (200, 401, 403)
    except requests.RequestException:
        return False


# ─────────────────────────────────────────────
#  UTILIDADES DE FORMATO
# ─────────────────────────────────────────────
def formatear_timestamp(ts: str) -> str:
    """Convierte ISO 8601 a formato legible."""
    if not ts:
        return "—"
    try:
        dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
        return dt.strftime("%d/%m/%Y  %H:%M:%S")
    except Exception:
        return ts


# ─────────────────────────────────────────────
#  CONFIGURACIÓN DE PÁGINA
# ─────────────────────────────────────────────
st.set_page_config(
    page_title="RFID Dashboard — LPC4088",
    page_icon="🔐",
    layout="wide",
    initial_sidebar_state="expanded",
)

st.markdown("""
<style>
    thead tr th { background-color: #f0f2f6 !important; }
    .block-container { padding-top: 1.5rem; }
    div[data-testid="metric-container"] { border: 1px solid #e0e0e0; border-radius: 8px; padding: 0.5rem; }
</style>
""", unsafe_allow_html=True)

# ─────────────────────────────────────────────
#  SIDEBAR — controles
# ─────────────────────────────────────────────
with st.sidebar:
    st.title("⚙️ Configuración")

    st.subheader("🔄 Auto-refresco")
    auto_refresh = st.toggle("Activar refresco automático", value=True)
    intervalo    = st.slider("Intervalo (segundos)", min_value=5, max_value=120,
                              value=15, step=5, disabled=not auto_refresh)

    st.divider()
    st.subheader("🔍 Filtros del log")
    limit_registros = st.selectbox("Registros a mostrar", [25, 50, 100, 200], index=1)
    filtro_estado   = st.radio("Filtrar por estado",
                                ["Todos", "Solo concedidos", "Solo denegados"])

    st.divider()
    st.caption(f"Última carga: {datetime.now().strftime('%H:%M:%S')}")

    if st.button("🔌 Comprobar conexión"):
        with st.spinner("Conectando…"):
            ok = ping_supabase()
        if ok:
            st.success("Supabase OK ✅")
        else:
            st.error("Sin conexión ❌")

# ─────────────────────────────────────────────
#  CABECERA
# ─────────────────────────────────────────────
st.title("🔐 RFID Access Dashboard")
st.caption(f"Proyecto LPC4088 · Supabase · Actualizado: {datetime.now().strftime('%H:%M:%S')}")

# ─────────────────────────────────────────────
#  TABS PRINCIPALES
# ─────────────────────────────────────────────
tab_log, tab_tarjetas, tab_nueva = st.tabs(["📋 Log de accesos", "💳 Gestión de tarjetas", "➕ Nueva tarjeta"])

# ══════════════════════════════════════════════
#  TAB 1 — LOG DE ACCESOS
# ══════════════════════════════════════════════
with tab_log:

    with st.spinner("Cargando registros…"):
        registros = get_registros(limit=limit_registros)

    if not registros:
        st.warning("No se pudieron cargar los registros. Comprueba la conexión.")
        st.stop()

    df = pd.DataFrame(registros)

    # ── Métricas resumen ──────────────────────
    total      = len(df)
    concedidos = int(df["concedido"].sum())
    denegados  = total - concedidos
    tasa       = round(concedidos / total * 100, 1) if total else 0

    c1, c2, c3, c4 = st.columns(4)
    c1.metric("Total accesos",  total)
    c2.metric("✅ Concedidos",  concedidos)
    c3.metric("❌ Denegados",   denegados)
    c4.metric("Tasa de éxito", f"{tasa}%")

    st.divider()

    # ── Filtro por estado ─────────────────────
    if filtro_estado == "Solo concedidos":
        df = df[df["concedido"] == True]
    elif filtro_estado == "Solo denegados":
        df = df[df["concedido"] == False]

    # ── Formatear tabla ───────────────────────
    df_vista = df.copy()
    df_vista["created_at"] = df_vista["created_at"].apply(formatear_timestamp)  
    df_vista["concedido"]  = df_vista["concedido"].apply(
        lambda v: "✅ Concedido" if v else "❌ Denegado"
    )
    df_vista = df_vista.rename(columns={
        "id":          "ID",
        "uid_tarjeta": "UID Tarjeta",
        "concedido":   "Estado",
        "dispositivo": "Dispositivo",
        "created_at":  "Fecha y hora",  
    })

    cols_mostrar = ["Fecha y hora", "UID Tarjeta", "Estado", "Dispositivo"]
    st.dataframe(
        df_vista[cols_mostrar],
        use_container_width=True,
        hide_index=True,
    )

    st.caption(f"Mostrando {len(df_vista)} registros · filtro: {filtro_estado}")

    # ── Descarga CSV ──────────────────────────
    csv = df_vista[cols_mostrar].to_csv(index=False).encode("utf-8")
    st.download_button(
        label="⬇️ Descargar CSV",
        data=csv,
        file_name=f"rfid_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv",
        mime="text/csv",
    )

    # ── Mini gráfica de accesos ───────────────
    if len(df) >= 2:
        st.divider()
        st.subheader("📊 Accesos recientes")
        try:
            df_chart = df.copy()
            df_chart["hora"] = pd.to_datetime(
                df_chart["created_at"].str.replace("Z", "+00:00")  
            ).dt.floor("h").dt.strftime("%H:%M")

            pivot = df_chart.groupby(["hora", "concedido"]).size().unstack(fill_value=0)
            pivot.columns = ["Denegados" if c == False else "Concedidos" for c in pivot.columns]
            st.bar_chart(pivot, color=["#FF4B4B", "#21C354"] if "Denegados" in pivot.columns else ["#21C354"])
        except Exception:
            pass


# ══════════════════════════════════════════════
#  TAB 2 — GESTIÓN DE TARJETAS
# ══════════════════════════════════════════════
with tab_tarjetas:
    st.subheader("💳 Tarjetas registradas")

    with st.spinner("Cargando tarjetas…"):
        tarjetas = get_tarjetas()

    if not tarjetas:
        st.info("No hay tarjetas registradas aún.")
    else:
        for t in tarjetas:
            uid         = t.get("uid", "—")
            nombre      = t.get("nombre", "Sin nombre")
            activa      = t.get("activa", False)
            created_at  = formatear_timestamp(t.get("created_at", ""))   
            modified_at = formatear_timestamp(t.get("modified_at", ""))  

            with st.container(border=True):
                col_info, col_btn = st.columns([4, 1])
                with col_info:
                    estado_icon = "🟢" if activa else "🔴"
                    st.markdown(f"**{nombre}**")
                    st.caption(
                        f"UID: `{uid}`  ·  {estado_icon} {'Activa' if activa else 'Desactivada'}"
                        f"  ·  Creada: {created_at}  ·  Modificada: {modified_at}"  
                    )
                with col_btn:
                    label_btn = "🔴 Desactivar" if activa else "🟢 Activar"
                    if st.button(label_btn, key=f"btn_{uid}"):
                        ok = cambiar_estado_tarjeta(uid, not activa)
                        if ok:
                            st.success("Estado actualizado.")
                            time.sleep(0.8)
                            st.rerun()
                        else:
                            st.error("Error al actualizar.")


# ══════════════════════════════════════════════
#  TAB 3 — NUEVA TARJETA
# ══════════════════════════════════════════════
with tab_nueva:
    st.subheader("➕ Registrar nueva tarjeta RFID")
    st.caption("Formato UID: `XX:XX:XX:XX` (p.ej. `A3:FF:12:04`)")

    with st.form("form_nueva_tarjeta", clear_on_submit=True):
        uid_input    = st.text_input("UID de la tarjeta", placeholder="A3:FF:12:04",
                                      max_chars=23)
        nombre_input = st.text_input("Nombre / propietario", placeholder="John Doe",
                                      max_chars=60)
        submitted    = st.form_submit_button("💾 Registrar tarjeta")

    if submitted:
        ok, msg = insertar_tarjeta(uid_input, nombre_input)
        if ok:
            st.success(msg)
        else:
            st.error(msg)


# ─────────────────────────────────────────────
#  AUTO-REFRESCO  (al final, después de renderizar todo)
# ─────────────────────────────────────────────
if auto_refresh:
    time.sleep(intervalo)
    st.rerun()