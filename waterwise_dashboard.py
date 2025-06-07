"""
WaterWise - Dashboard Streamlit
Sistema de Monitoramento em Tempo Real
Global Solution 2025 - FIAP

Equipe:
- Felipe Amador (553528)
- Leonardo de Oliveira (554024)
- Sara Sousa (552656)
"""

import streamlit as st
import cx_Oracle
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go
from datetime import datetime, timedelta
import time
import json

# Configuração Oracle FIAP
ORACLE_CONFIG = {
    'host': 'oracle.fiap.com.br',
    'port': 1521,
    'service': 'ORCL',
    'user': 'RM553528',
    'password': '150592'
}

# Configuração da página
st.set_page_config(
    page_title="WaterWise Dashboard",
    page_icon="🌊",
    layout="wide",
    initial_sidebar_state="expanded"
)

# CSS customizado
st.markdown("""
<style>
    .main-header {
        background: linear-gradient(90deg, #1e3c72, #2a5298);
        padding: 1rem;
        border-radius: 10px;
        color: white;
        text-align: center;
        margin-bottom: 2rem;
    }

    .metric-card {
        background: white;
        padding: 1rem;
        border-radius: 10px;
        box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        border-left: 4px solid #2a5298;
    }

    .alert-critical {
        background: #ff4444;
        color: white;
        padding: 0.5rem;
        border-radius: 5px;
    }

    .alert-high {
        background: #ff8800;
        color: white;
        padding: 0.5rem;
        border-radius: 5px;
    }

    .alert-medium {
        background: #ffcc00;
        color: black;
        padding: 0.5rem;
        border-radius: 5px;
    }

    .alert-low {
        background: #44aa44;
        color: white;
        padding: 0.5rem;
        border-radius: 5px;
    }
</style>
""", unsafe_allow_html=True)


@st.cache_resource
def init_connection():
    """Inicializa conexão com Oracle Database FIAP"""
    try:
        dsn = cx_Oracle.makedsn(
            ORACLE_CONFIG['host'],
            ORACLE_CONFIG['port'],
            service_name=ORACLE_CONFIG['service']
        )

        connection = cx_Oracle.connect(
            user=ORACLE_CONFIG['user'],
            password=ORACLE_CONFIG['password'],
            dsn=dsn
        )
        return connection
    except Exception as e:
        st.error(f"❌ Erro de conexão Oracle FIAP: {e}")
        return None


@st.cache_data(ttl=30)
def get_system_metrics(_conn):
    """Busca métricas gerais do sistema"""
    if not _conn:
        return {}

    query = """
    SELECT 
        'Total Produtores' as METRICA, COUNT(*) as VALOR 
    FROM GS_WW_PRODUTOR_RURAL
    UNION ALL
    SELECT 'Total Propriedades', COUNT(*) FROM GS_WW_PROPRIEDADE_RURAL
    UNION ALL  
    SELECT 'Total Sensores', COUNT(*) FROM GS_WW_SENSOR_IOT
    UNION ALL
    SELECT 'Leituras Hoje', COUNT(*) FROM GS_WW_LEITURA_SENSOR 
    WHERE TIMESTAMP_LEITURA >= TRUNC(SYSDATE)
    UNION ALL
    SELECT 'Alertas Hoje', COUNT(*) FROM GS_WW_ALERTA 
    WHERE TIMESTAMP_ALERTA >= TRUNC(SYSDATE)
    """

    try:
        df = pd.read_sql(query, _conn)
        return dict(zip(df['METRICA'], df['VALOR']))
    except Exception as e:
        st.error(f"Erro ao buscar métricas: {e}")
        return {}


@st.cache_data(ttl=30)
def get_recent_readings(_conn, limit=50):
    """Busca leituras recentes"""
    if not _conn:
        return pd.DataFrame()

    query = f"""
    SELECT 
        TIMESTAMP_LEITURA,
        UMIDADE_SOLO,
        TEMPERATURA_AR,
        PRECIPITACAO_MM,
        PRODUTOR,
        NOME_PROPRIEDADE,
        TIPO_SENSOR
    FROM VW_LEITURAS_COMPLETAS 
    WHERE ROWNUM <= {limit}
    ORDER BY TIMESTAMP_LEITURA DESC
    """

    try:
        df = pd.read_sql(query, _conn)
        if not df.empty:
            df['TIMESTAMP_LEITURA'] = pd.to_datetime(df['TIMESTAMP_LEITURA'])
        return df
    except Exception as e:
        st.error(f"Erro ao buscar leituras: {e}")
        return pd.DataFrame()


@st.cache_data(ttl=60)
def get_active_alerts(_conn):
    """Busca alertas ativos"""
    if not _conn:
        return pd.DataFrame()

    query = """
    SELECT 
        TIMESTAMP_ALERTA,
        DESCRICAO_ALERTA,
        CODIGO_SEVERIDADE,
        DESCRICAO_SEVERIDADE,
        ACOES_RECOMENDADAS,
        PRODUTOR,
        NOME_PROPRIEDADE,
        UMIDADE_SOLO,
        TEMPERATURA_AR,
        PRECIPITACAO_MM
    FROM VW_ALERTAS_ATIVOS
    ORDER BY TIMESTAMP_ALERTA DESC
    """

    try:
        df = pd.read_sql(query, _conn)
        if not df.empty:
            df['TIMESTAMP_ALERTA'] = pd.to_datetime(df['TIMESTAMP_ALERTA'])
        return df
    except Exception as e:
        st.error(f"Erro ao buscar alertas: {e}")
        return pd.DataFrame()


@st.cache_data(ttl=120)
def get_properties_summary(_conn):
    """Busca resumo das propriedades"""
    if not _conn:
        return pd.DataFrame()

    query = """
    SELECT * FROM VW_DASHBOARD_EXECUTIVO
    ORDER BY ULTIMA_LEITURA DESC NULLS LAST
    """

    try:
        df = pd.read_sql(query, _conn)
        if not df.empty and 'ULTIMA_LEITURA' in df.columns:
            df['ULTIMA_LEITURA'] = pd.to_datetime(df['ULTIMA_LEITURA'])
        return df
    except Exception as e:
        st.error(f"Erro ao buscar resumo das propriedades: {e}")
        return pd.DataFrame()


def render_alert_badge(severity):
    """Renderiza badge de alerta baseado na severidade"""
    colors = {
        'CRITICO': '#ff4444',
        'ALTO': '#ff8800',
        'MEDIO': '#ffcc00',
        'BAIXO': '#44aa44'
    }

    color = colors.get(severity, '#666666')
    return f'<span style="background:{color}; color:white; padding:0.2rem 0.5rem; border-radius:3px; font-size:0.8rem;">{severity}</span>'


def main():
    """Função principal do dashboard"""

    # Header principal
    st.markdown("""
    <div class="main-header">
        <h1>🌊 WaterWise Dashboard</h1>
        <p>Sistema Inteligente de Prevenção a Enchentes - Global Solution 2025 FIAP</p>
        <p><strong>Equipe:</strong> Felipe Amador | Leonardo de Oliveira | Sara Sousa</p>
    </div>
    """, unsafe_allow_html=True)

    # Inicializar conexão
    conn = init_connection()

    if not conn:
        st.error("❌ Não foi possível conectar ao banco de dados. Verifique as configurações.")
        st.info("💡 Configure as credenciais no arquivo config.py")
        return

    # Sidebar para controles
    st.sidebar.title("🎛️ Controles")

    # Botão de refresh
    if st.sidebar.button("🔄 Atualizar Dados"):
        st.cache_data.clear()
        st.rerun()

    # Filtros
    st.sidebar.subheader("📊 Filtros")
    auto_refresh = st.sidebar.checkbox("Auto-refresh (30s)", value=False)

    if auto_refresh:
        time.sleep(10)
        st.rerun()

    # Buscar dados
    metrics = get_system_metrics(conn)
    recent_readings = get_recent_readings(conn)
    active_alerts = get_active_alerts(conn)
    properties_summary = get_properties_summary(conn)

    # Layout principal com abas
    tab1, tab2, tab3, tab4 = st.tabs(["📊 Visão Geral", "📈 Leituras", "🚨 Alertas", "🏠 Propriedades"])

    with tab1:
        render_overview_tab(metrics, recent_readings, active_alerts)

    with tab2:
        render_readings_tab(recent_readings)

    with tab3:
        render_alerts_tab(active_alerts)

    with tab4:
        render_properties_tab(properties_summary)

    # Footer
    st.markdown("---")
    st.markdown(f"**Última atualização:** {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}")
    st.markdown("**Status Conexão:** ✅ Oracle Database Conectado")


def render_overview_tab(metrics, recent_readings, active_alerts):
    """Renderiza aba de visão geral"""

    # Métricas principais
    st.subheader("📊 Métricas do Sistema")

    col1, col2, col3, col4, col5 = st.columns(5)

    with col1:
        st.metric(
            "👥 Produtores",
            metrics.get('Total Produtores', 0)
        )

    with col2:
        st.metric(
            "🏠 Propriedades",
            metrics.get('Total Propriedades', 0)
        )

    with col3:
        st.metric(
            "📡 Sensores",
            metrics.get('Total Sensores', 0)
        )

    with col4:
        st.metric(
            "📊 Leituras Hoje",
            metrics.get('Leituras Hoje', 0)
        )

    with col5:
        st.metric(
            "🚨 Alertas Hoje",
            metrics.get('Alertas Hoje', 0),
            delta_color="inverse"
        )

    # Gráficos de resumo
    col1, col2 = st.columns(2)

    with col1:
        if not recent_readings.empty:
            st.subheader("🌡️ Temperatura nas Últimas Horas")
            fig = px.line(
                recent_readings.head(20),
                x='TIMESTAMP_LEITURA',
                y='TEMPERATURA_AR',
                color='NOME_PROPRIEDADE',
                title="Variação de Temperatura por Propriedade"
            )
            fig.update_layout(height=400)
            st.plotly_chart(fig, use_container_width=True)

    with col2:
        if not recent_readings.empty:
            st.subheader("💧 Umidade do Solo")
            fig = px.line(
                recent_readings.head(20),
                x='TIMESTAMP_LEITURA',
                y='UMIDADE_SOLO',
                color='NOME_PROPRIEDADE',
                title="Umidade do Solo por Propriedade"
            )
            fig.update_layout(height=400)
            st.plotly_chart(fig, use_container_width=True)

    # Status dos alertas
    if not active_alerts.empty:
        st.subheader("🚨 Alertas Recentes")

        # Contador por severidade
        alert_counts = active_alerts['CODIGO_SEVERIDADE'].value_counts()

        col1, col2, col3, col4 = st.columns(4)

        with col1:
            st.metric("🔴 Críticos", alert_counts.get('CRITICO', 0))
        with col2:
            st.metric("🟠 Altos", alert_counts.get('ALTO', 0))
        with col3:
            st.metric("🟡 Médios", alert_counts.get('MEDIO', 0))
        with col4:
            st.metric("🟢 Baixos", alert_counts.get('BAIXO', 0))


def render_readings_tab(readings_df):
    """Renderiza aba de leituras"""

    st.subheader("📈 Leituras dos Sensores")

    if readings_df.empty:
        st.warning("📭 Nenhuma leitura encontrada")
        st.info("💡 Verifique se o sistema de coleta de dados está funcionando")
        return

    # Filtros
    col1, col2 = st.columns(2)

    with col1:
        propriedades = readings_df['NOME_PROPRIEDADE'].unique()
        selected_property = st.selectbox("🏠 Filtrar por Propriedade", ["Todas"] + list(propriedades))

    with col2:
        time_range = st.selectbox("⏰ Período", ["Última Hora", "Últimas 6 Horas", "Últimas 24 Horas", "Todos"])

    # Aplicar filtros
    filtered_df = readings_df.copy()

    if selected_property != "Todas":
        filtered_df = filtered_df[filtered_df['NOME_PROPRIEDADE'] == selected_property]

    now = datetime.now()
    if time_range == "Última Hora":
        filtered_df = filtered_df[filtered_df['TIMESTAMP_LEITURA'] >= now - timedelta(hours=1)]
    elif time_range == "Últimas 6 Horas":
        filtered_df = filtered_df[filtered_df['TIMESTAMP_LEITURA'] >= now - timedelta(hours=6)]
    elif time_range == "Últimas 24 Horas":
        filtered_df = filtered_df[filtered_df['TIMESTAMP_LEITURA'] >= now - timedelta(hours=24)]

    # Gráficos
    if not filtered_df.empty:
        col1, col2 = st.columns(2)

        with col1:
            fig = px.line(
                filtered_df,
                x='TIMESTAMP_LEITURA',
                y='UMIDADE_SOLO',
                color='NOME_PROPRIEDADE',
                title="🌱 Umidade do Solo (%)",
                labels={'UMIDADE_SOLO': 'Umidade (%)', 'TIMESTAMP_LEITURA': 'Horário'}
            )
            fig.add_hline(y=25, line_dash="dash", line_color="red", annotation_text="Limite Crítico")
            st.plotly_chart(fig, use_container_width=True)

        with col2:
            fig = px.line(
                filtered_df,
                x='TIMESTAMP_LEITURA',
                y='PRECIPITACAO_MM',
                color='NOME_PROPRIEDADE',
                title="🌧️ Precipitação (mm)",
                labels={'PRECIPITACAO_MM': 'Precipitação (mm)', 'TIMESTAMP_LEITURA': 'Horário'}
            )
            st.plotly_chart(fig, use_container_width=True)

        # Gauge da última leitura
        if len(filtered_df) > 0:
            latest = filtered_df.iloc[0]

            st.subheader(f"📊 Última Leitura - {latest['NOME_PROPRIEDADE']}")

            col1, col2, col3 = st.columns(3)

            with col1:
                fig = go.Figure(go.Indicator(
                    mode="gauge+number",
                    value=latest['UMIDADE_SOLO'],
                    domain={'x': [0, 1], 'y': [0, 1]},
                    title={'text': "Umidade Solo (%)"},
                    gauge={
                        'axis': {'range': [None, 100]},
                        'bar': {'color': "darkblue"},
                        'steps': [
                            {'range': [0, 25], 'color': "red"},
                            {'range': [25, 60], 'color': "yellow"},
                            {'range': [60, 100], 'color': "green"}
                        ],
                        'threshold': {
                            'line': {'color': "red", 'width': 4},
                            'thickness': 0.75,
                            'value': 25
                        }
                    }
                ))
                fig.update_layout(height=250)
                st.plotly_chart(fig, use_container_width=True)

            with col2:
                fig = go.Figure(go.Indicator(
                    mode="gauge+number",
                    value=latest['TEMPERATURA_AR'],
                    domain={'x': [0, 1], 'y': [0, 1]},
                    title={'text': "Temperatura (°C)"},
                    gauge={
                        'axis': {'range': [0, 50]},
                        'bar': {'color': "orange"},
                        'steps': [
                            {'range': [0, 20], 'color': "lightblue"},
                            {'range': [20, 35], 'color': "yellow"},
                            {'range': [35, 50], 'color': "red"}
                        ]
                    }
                ))
                fig.update_layout(height=250)
                st.plotly_chart(fig, use_container_width=True)

            with col3:
                fig = go.Figure(go.Indicator(
                    mode="gauge+number",
                    value=latest['PRECIPITACAO_MM'],
                    domain={'x': [0, 1], 'y': [0, 1]},
                    title={'text': "Precipitação (mm)"},
                    gauge={
                        'axis': {'range': [0, 100]},
                        'bar': {'color': "blue"},
                        'steps': [
                            {'range': [0, 10], 'color': "lightgreen"},
                            {'range': [10, 50], 'color': "yellow"},
                            {'range': [50, 100], 'color': "red"}
                        ]
                    }
                ))
                fig.update_layout(height=250)
                st.plotly_chart(fig, use_container_width=True)

    # Tabela de dados
    st.subheader("📋 Dados Detalhados")
    st.dataframe(
        filtered_df[[
            'TIMESTAMP_LEITURA', 'NOME_PROPRIEDADE', 'PRODUTOR',
            'UMIDADE_SOLO', 'TEMPERATURA_AR', 'PRECIPITACAO_MM'
        ]].sort_values('TIMESTAMP_LEITURA', ascending=False),
        use_container_width=True
    )


def render_alerts_tab(alerts_df):
    """Renderiza aba de alertas"""

    st.subheader("🚨 Alertas Ativos")

    if alerts_df.empty:
        st.success("✅ Nenhum alerta ativo no momento")
        st.info("🛡️ Sistema funcionando dentro dos parâmetros normais")
        return

    # Resumo dos alertas
    col1, col2, col3 = st.columns(3)

    with col1:
        critical_alerts = len(alerts_df[alerts_df['CODIGO_SEVERIDADE'] == 'CRITICO'])
        st.metric("🔴 Alertas Críticos", critical_alerts)

    with col2:
        high_alerts = len(alerts_df[alerts_df['CODIGO_SEVERIDADE'] == 'ALTO'])
        st.metric("🟠 Alertas Altos", high_alerts)

    with col3:
        total_properties = alerts_df['NOME_PROPRIEDADE'].nunique()
        st.metric("🏠 Propriedades Afetadas", total_properties)

    # Gráfico de alertas por hora
    if 'TIMESTAMP_ALERTA' in alerts_df.columns:
        alerts_df['HORA'] = alerts_df['TIMESTAMP_ALERTA'].dt.floor('H')
        alerts_by_hour = alerts_df.groupby(['HORA', 'CODIGO_SEVERIDADE']).size().reset_index(name='COUNT')

        fig = px.bar(
            alerts_by_hour,
            x='HORA',
            y='COUNT',
            color='CODIGO_SEVERIDADE',
            title="📊 Distribuição de Alertas por Hora",
            color_discrete_map={
                'CRITICO': '#ff4444',
                'ALTO': '#ff8800',
                'MEDIO': '#ffcc00',
                'BAIXO': '#44aa44'
            }
        )
        st.plotly_chart(fig, use_container_width=True)

    # Lista de alertas
    st.subheader("📋 Lista de Alertas")

    for idx, alert in alerts_df.iterrows():
        severity_color = {
            'CRITICO': '#ff4444',
            'ALTO': '#ff8800',
            'MEDIO': '#ffcc00',
            'BAIXO': '#44aa44'
        }.get(alert['CODIGO_SEVERIDADE'], '#666666')

        with st.expander(
                f"🚨 {alert['NOME_PROPRIEDADE']} - {alert['CODIGO_SEVERIDADE']} "
                f"({alert['TIMESTAMP_ALERTA'].strftime('%d/%m %H:%M')})"
        ):
            col1, col2 = st.columns(2)

            with col1:
                st.markdown(f"**Propriedade:** {alert['NOME_PROPRIEDADE']}")
                st.markdown(f"**Produtor:** {alert['PRODUTOR']}")
                st.markdown(f"**Severidade:** {alert['CODIGO_SEVERIDADE']}")
                st.markdown(f"**Horário:** {alert['TIMESTAMP_ALERTA'].strftime('%d/%m/%Y %H:%M:%S')}")

            with col2:
                st.markdown(f"**Umidade Solo:** {alert['UMIDADE_SOLO']:.1f}%")
                st.markdown(f"**Temperatura:** {alert['TEMPERATURA_AR']:.1f}°C")
                st.markdown(f"**Precipitação:** {alert['PRECIPITACAO_MM']:.1f}mm")

            st.markdown(f"**Descrição:** {alert['DESCRICAO_ALERTA']}")
            st.markdown(f"**Ações Recomendadas:** {alert['ACOES_RECOMENDADAS']}")


def render_properties_tab(properties_df):
    """Renderiza aba de propriedades"""

    st.subheader("🏠 Resumo das Propriedades")

    if properties_df.empty:
        st.warning("📭 Nenhuma propriedade encontrada")
        return

    # Métricas gerais
    col1, col2, col3, col4 = st.columns(4)

    with col1:
        st.metric("🏠 Total Propriedades", len(properties_df))

    with col2:
        active_properties = len(properties_df[properties_df['TOTAL_LEITURAS'] > 0])
        st.metric("📡 Propriedades Ativas", active_properties)

    with col3:
        avg_sensors = properties_df['TOTAL_SENSORES'].mean()
        st.metric("📊 Média Sensores/Propriedade", f"{avg_sensors:.1f}")

    with col4:
        total_readings = properties_df['TOTAL_LEITURAS'].sum()
        st.metric("📈 Total Leituras", int(total_readings))

    # Tabela detalhada
    st.subheader("📊 Detalhes das Propriedades")

    # Preparar dados para exibição
    display_df = properties_df.copy()

    if 'ULTIMA_LEITURA' in display_df.columns:
        display_df['ULTIMA_LEITURA'] = display_df['ULTIMA_LEITURA'].dt.strftime('%d/%m/%Y %H:%M')
        display_df = display_df.fillna('N/A')

    # Colorir baseado na atividade
    def highlight_activity(row):
        if row['TOTAL_LEITURAS'] == 0:
            return ['background-color: #ffcccc'] * len(row)  # Vermelho claro para inativo
        elif row['TOTAL_ALERTAS'] > 5:
            return ['background-color: #ffffcc'] * len(row)  # Amarelo para muitos alertas
        else:
            return ['background-color: #ccffcc'] * len(row)  # Verde claro para normal

    st.dataframe(
        display_df.style.apply(highlight_activity, axis=1),
        use_container_width=True
    )

    # Gráfico de atividade
    if 'TOTAL_LEITURAS' in properties_df.columns:
        fig = px.bar(
            properties_df,
            x='NOME_PROPRIEDADE',
            y='TOTAL_LEITURAS',
            title="📊 Atividade das Propriedades (Total de Leituras)",
            labels={'TOTAL_LEITURAS': 'Total de Leituras', 'NOME_PROPRIEDADE': 'Propriedade'}
        )
        fig.update_xaxes(tickangle=45)
        st.plotly_chart(fig, use_container_width=True)


if __name__ == "__main__":
    main()