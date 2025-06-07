"""
WaterWise API - Backend para Oracle FIAP
Configurado especificamente para oracle.fiap.com.br

Instalação:
pip install flask cx_Oracle python-dotenv

Uso:
python waterwise_api.py
"""

from flask import Flask, request, jsonify
import os
import json
from datetime import datetime
import logging
from dotenv import load_dotenv

# Carregar variáveis de ambiente
load_dotenv()

# Configurar logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

app = Flask(__name__)

# =============================================================================
# CONFIGURAÇÕES DO BANCO ORACLE FIAP
# =============================================================================

# Configurações específicas do Oracle FIAP
ORACLE_CONFIG = {
    'host': 'oracle.fiap.com.br',
    'port': 1521,
    'service': 'ORCL',
    'user': 'RM553528',
    'password': '150592'
}

# Tentar importar cx_Oracle
try:
    import cx_Oracle

    ORACLE_AVAILABLE = True
    logger.info("✅ cx_Oracle disponível")
except ImportError:
    ORACLE_AVAILABLE = False
    logger.warning("⚠️ cx_Oracle não encontrado - instale com: pip install cx_Oracle")

# Modo fallback - salvar dados em arquivo JSON
FALLBACK_DATA_FILE = 'waterwise_data.json'


def create_dsn_variations():
    """Criar diferentes variações de DSN para tentar conectar"""
    config = ORACLE_CONFIG
    variations = [
        # Formato 1: host:port/service
        f"{config['host']}:{config['port']}/{config['service']}",
        # Formato 2: Easy Connect
        f"{config['host']}:{config['port']}/{config['service']}",
        # Formato 3: TNS Connect descriptor
        f"(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST={config['host']})(PORT={config['port']}))(CONNECT_DATA=(SERVICE_NAME={config['service']})))",
        # Formato 4: Usando SID em vez de SERVICE
        f"{config['host']}:{config['port']}/{config['service']}",
        # Formato 5: Com protocolo explícito
        f"tcp://{config['host']}:{config['port']}/{config['service']}",
    ]
    return variations


def get_db_connection():
    """Criar conexão com Oracle FIAP"""
    if not ORACLE_AVAILABLE:
        logger.warning("cx_Oracle não está disponível")
        return None

    try:
        config = ORACLE_CONFIG
        dsn_variations = create_dsn_variations()

        for i, dsn in enumerate(dsn_variations, 1):
            try:
                logger.info(f"🔄 Tentativa {i}: Conectando {config['user']}@{dsn}")

                # Tentar conexão com timeout
                connection = cx_Oracle.connect(
                    user=config['user'],
                    password=config['password'],
                    dsn=dsn,
                    encoding="UTF-8"
                )

                # Testar a conexão
                cursor = connection.cursor()
                cursor.execute("SELECT 'Connected to Oracle FIAP!' FROM DUAL")
                result = cursor.fetchone()
                cursor.close()

                logger.info(f"✅ Conectado com sucesso usando DSN {i}: {dsn}")
                return connection

            except cx_Oracle.Error as e:
                logger.warning(f"❌ DSN {i} falhou: {e}")
                continue
            except Exception as e:
                logger.warning(f"❌ Erro geral DSN {i}: {e}")
                continue

        logger.error("❌ Todas as tentativas de conexão falharam")
        return None

    except Exception as error:
        logger.error(f"❌ Erro crítico ao conectar: {error}")
        return None


def test_db_connection():
    """Testar conexão com o banco Oracle FIAP"""
    if not ORACLE_AVAILABLE:
        logger.info("🔄 cx_Oracle não disponível - modo fallback ativo")
        return False

    try:
        conn = get_db_connection()
        if conn:
            cursor = conn.cursor()

            # Testar consulta simples
            cursor.execute("SELECT USER, SYS_CONTEXT('USERENV','SERVER_HOST') FROM DUAL")
            result = cursor.fetchone()

            logger.info(f"✅ Conectado como: {result[0]} no servidor: {result[1]}")

            # Testar se as tabelas existem
            cursor.execute("""
                SELECT COUNT(*) FROM USER_TABLES 
                WHERE TABLE_NAME IN ('GS_WW_LEITURA_SENSOR', 'GS_WW_ALERTA', 'GS_WW_PRODUTOR_RURAL')
            """)
            table_count = cursor.fetchone()[0]
            logger.info(f"📊 Tabelas WaterWise encontradas: {table_count}/3")

            cursor.close()
            conn.close()

            return True
    except Exception as e:
        logger.error(f"❌ Erro no teste de conexão: {e}")

    logger.info("🔄 Usando modo fallback - dados em arquivo JSON")
    return False


def load_fallback_data():
    """Carregar dados do arquivo JSON"""
    try:
        if os.path.exists(FALLBACK_DATA_FILE):
            with open(FALLBACK_DATA_FILE, 'r', encoding='utf-8') as f:
                return json.load(f)
    except Exception as e:
        logger.error(f"Erro ao carregar dados fallback: {e}")
    return {'leituras': [], 'alertas': []}


def save_fallback_data(data):
    """Salvar dados no arquivo JSON"""
    try:
        with open(FALLBACK_DATA_FILE, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, default=str, ensure_ascii=False)
        return True
    except Exception as e:
        logger.error(f"Erro ao salvar dados fallback: {e}")
        return False


# =============================================================================
# ENDPOINTS DA API
# =============================================================================

@app.route('/', methods=['GET'])
def home():
    """Endpoint de teste"""
    db_status = test_db_connection()

    return jsonify({
        'message': 'WaterWise API - Sistema IoT para Oracle FIAP',
        'version': '2.0.0',
        'status': 'online',
        'oracle_config': {
            'host': ORACLE_CONFIG['host'],
            'port': ORACLE_CONFIG['port'],
            'service': ORACLE_CONFIG['service'],
            'user': ORACLE_CONFIG['user']
        },
        'oracle_available': ORACLE_AVAILABLE,
        'oracle_connected': db_status,
        'storage_mode': 'oracle' if db_status else 'json_fallback',
        'timestamp': datetime.now().isoformat()
    })


@app.route('/health', methods=['GET'])
def health_check():
    """Health check completo da API"""
    db_status = test_db_connection()

    # Verificar arquivo fallback
    fallback_data = load_fallback_data()

    return jsonify({
        'api_status': 'healthy',
        'oracle_module': ORACLE_AVAILABLE,
        'oracle_connection': db_status,
        'storage_mode': 'oracle' if db_status else 'json_fallback',
        'fallback_data': {
            'leituras': len(fallback_data['leituras']),
            'alertas': len(fallback_data['alertas'])
        },
        'config': {
            'host': ORACLE_CONFIG['host'],
            'port': ORACLE_CONFIG['port'],
            'user': ORACLE_CONFIG['user']
        },
        'timestamp': datetime.now().isoformat()
    }), 200 if (ORACLE_AVAILABLE and db_status) else 206


@app.route('/api/leituras', methods=['POST'])
def inserir_leitura():
    """Inserir leitura de sensor no Oracle FIAP"""
    try:
        # Validar dados recebidos
        data = request.get_json()

        if not data:
            return jsonify({'error': 'Nenhum dado JSON recebido'}), 400

        # Log dos dados recebidos
        logger.info(f"📡 Dados recebidos: {data}")

        # Campos obrigatórios
        required_fields = ['id_sensor', 'umidade_solo', 'temperatura_ar', 'precipitacao_mm']
        missing_fields = [field for field in required_fields if field not in data]

        if missing_fields:
            return jsonify({
                'error': f'Campos obrigatórios ausentes: {missing_fields}',
                'required_fields': required_fields,
                'received_fields': list(data.keys())
            }), 400

        # Tentar inserir no Oracle FIAP primeiro
        conn = get_db_connection()
        if conn:
            try:
                cursor = conn.cursor()

                # SQL para inserir leitura
                sql_leitura = """
                INSERT INTO GS_WW_LEITURA_SENSOR (
                    ID_SENSOR, 
                    UMIDADE_SOLO, 
                    TEMPERATURA_AR, 
                    PRECIPITACAO_MM,
                    TIMESTAMP_LEITURA
                ) VALUES (
                    :id_sensor,
                    :umidade_solo,
                    :temperatura_ar,
                    :precipitacao_mm,
                    CURRENT_TIMESTAMP
                )
                """

                # Preparar dados para inserção
                insert_data = {
                    'id_sensor': int(data['id_sensor']),
                    'umidade_solo': float(data['umidade_solo']),
                    'temperatura_ar': float(data['temperatura_ar']),
                    'precipitacao_mm': float(data['precipitacao_mm'])
                }

                # Executar inserção
                cursor.execute(sql_leitura, insert_data)

                # Obter ID da leitura inserida (se houver sequence)
                try:
                    cursor.execute("SELECT GS_WW_LEITURA_SENSOR_SEQ.CURRVAL FROM DUAL")
                    leitura_id = cursor.fetchone()[0]
                except:
                    # Se não há sequence, usar um ID genérico
                    cursor.execute("SELECT MAX(ID_LEITURA) FROM GS_WW_LEITURA_SENSOR")
                    result = cursor.fetchone()
                    leitura_id = result[0] if result and result[0] else 1

                # Commit da transação
                conn.commit()

                # Fechar conexões
                cursor.close()
                conn.close()

                logger.info(f"✅ Leitura inserida no Oracle FIAP - ID: {leitura_id}")

                return jsonify({
                    'success': True,
                    'message': 'Leitura inserida no Oracle FIAP com sucesso',
                    'storage': 'oracle_fiap',
                    'leitura_id': leitura_id,
                    'data_inserted': insert_data,
                    'timestamp': datetime.now().isoformat()
                }), 201

            except cx_Oracle.Error as db_error:
                logger.error(f"❌ Erro no Oracle FIAP: {db_error}")
                if conn:
                    conn.close()
                # Continuar para fallback
            except Exception as e:
                logger.error(f"❌ Erro geral Oracle: {e}")
                if conn:
                    conn.close()
                # Continuar para fallback

        # Modo fallback - salvar em arquivo JSON
        logger.info("🔄 Usando modo fallback - salvando em JSON")

        fallback_data = load_fallback_data()

        leitura = {
            'id_leitura': len(fallback_data['leituras']) + 1,
            'id_sensor': int(data['id_sensor']),
            'umidade_solo': float(data['umidade_solo']),
            'temperatura_ar': float(data['temperatura_ar']),
            'precipitacao_mm': float(data['precipitacao_mm']),
            'timestamp': datetime.now().isoformat(),
            'farm_id': data.get('farm_id', 'FARM_WaterWise_2025'),
            'team_name': data.get('team_name', 'GRUPO_WATERWISE')
        }

        fallback_data['leituras'].append(leitura)

        # Manter apenas últimas 100 leituras
        if len(fallback_data['leituras']) > 100:
            fallback_data['leituras'] = fallback_data['leituras'][-100:]

        if save_fallback_data(fallback_data):
            logger.info(f"✅ Leitura salva em JSON - Total: {len(fallback_data['leituras'])}")

            return jsonify({
                'success': True,
                'message': 'Leitura salva em arquivo JSON (fallback)',
                'storage': 'json_fallback',
                'leitura_id': leitura['id_leitura'],
                'total_leituras': len(fallback_data['leituras']),
                'data_saved': leitura,
                'timestamp': datetime.now().isoformat()
            }), 201
        else:
            return jsonify({'error': 'Erro ao salvar dados em fallback'}), 500

    except Exception as e:
        logger.error(f"❌ Erro geral na inserção: {e}")
        return jsonify({
            'error': 'Erro interno do servidor',
            'details': str(e),
            'timestamp': datetime.now().isoformat()
        }), 500


@app.route('/api/alertas', methods=['POST'])
def inserir_alerta():
    """Inserir alerta no Oracle FIAP"""
    try:
        data = request.get_json()

        if not data:
            return jsonify({'error': 'Nenhum dado JSON recebido'}), 400

        logger.info(f"🚨 Alerta recebido: {data}")

        # Campos obrigatórios para alerta
        required_fields = ['id_produtor', 'codigo_severidade', 'descricao_alerta']
        missing_fields = [field for field in required_fields if field not in data]

        if missing_fields:
            return jsonify({
                'error': f'Campos obrigatórios ausentes: {missing_fields}'
            }), 400

        # Tentar inserir no Oracle FIAP
        conn = get_db_connection()
        if conn:
            try:
                cursor = conn.cursor()

                # SQL para inserir alerta
                sql_alerta = """
                INSERT INTO GS_WW_ALERTA (
                    ID_PRODUTOR,
                    ID_LEITURA,
                    ID_NIVEL_SEVERIDADE,
                    DESCRICAO_ALERTA,
                    TIMESTAMP_ALERTA
                ) VALUES (
                    :id_produtor,
                    (SELECT MAX(ID_LEITURA) FROM GS_WW_LEITURA_SENSOR),
                    (SELECT ID_NIVEL_SEVERIDADE FROM GS_WW_NIVEL_SEVERIDADE WHERE CODIGO_SEVERIDADE = :codigo_severidade),
                    :descricao_alerta,
                    CURRENT_TIMESTAMP
                )
                """

                cursor.execute(sql_alerta, {
                    'id_produtor': int(data['id_produtor']),
                    'codigo_severidade': data['codigo_severidade'],
                    'descricao_alerta': data['descricao_alerta']
                })

                # Commit
                conn.commit()
                cursor.close()
                conn.close()

                logger.info(f"🚨 Alerta inserido no Oracle FIAP com sucesso")

                return jsonify({
                    'success': True,
                    'message': 'Alerta inserido no Oracle FIAP com sucesso',
                    'storage': 'oracle_fiap',
                    'timestamp': datetime.now().isoformat()
                }), 201

            except Exception as db_error:
                logger.error(f"❌ Erro no Oracle para alerta: {db_error}")
                if conn:
                    conn.close()

        # Fallback para JSON
        fallback_data = load_fallback_data()

        alerta = {
            'id_alerta': len(fallback_data['alertas']) + 1,
            'id_produtor': int(data['id_produtor']),
            'codigo_severidade': data['codigo_severidade'],
            'descricao_alerta': data['descricao_alerta'],
            'timestamp': datetime.now().isoformat()
        }

        fallback_data['alertas'].append(alerta)

        if save_fallback_data(fallback_data):
            logger.info(f"🚨 Alerta salvo em JSON (fallback)")

            return jsonify({
                'success': True,
                'message': 'Alerta salvo em JSON (fallback)',
                'storage': 'json_fallback',
                'alerta_id': alerta['id_alerta'],
                'timestamp': datetime.now().isoformat()
            }), 201
        else:
            return jsonify({'error': 'Erro ao salvar alerta em fallback'}), 500

    except Exception as e:
        logger.error(f"❌ Erro geral no alerta: {e}")
        return jsonify({
            'error': 'Erro interno do servidor',
            'details': str(e)
        }), 500


@app.route('/api/leituras', methods=['GET'])
def listar_leituras():
    """Listar últimas leituras do Oracle FIAP ou JSON"""
    try:
        limit = request.args.get('limit', 10, type=int)

        # Tentar Oracle FIAP primeiro
        conn = get_db_connection()
        if conn:
            try:
                cursor = conn.cursor()

                sql = """
                SELECT 
                    ID_LEITURA,
                    TIMESTAMP_LEITURA,
                    UMIDADE_SOLO,
                    TEMPERATURA_AR,
                    PRECIPITACAO_MM
                FROM GS_WW_LEITURA_SENSOR 
                ORDER BY TIMESTAMP_LEITURA DESC 
                FETCH FIRST :limit ROWS ONLY
                """

                cursor.execute(sql, {'limit': limit})
                results = cursor.fetchall()

                leituras = []
                for row in results:
                    leituras.append({
                        'id_leitura': row[0],
                        'timestamp': row[1].isoformat() if row[1] else None,
                        'umidade_solo': float(row[2]) if row[2] else 0,
                        'temperatura_ar': float(row[3]) if row[3] else 0,
                        'precipitacao_mm': float(row[4]) if row[4] else 0
                    })

                cursor.close()
                conn.close()

                logger.info(f"📊 Listadas {len(leituras)} leituras do Oracle FIAP")

                return jsonify({
                    'success': True,
                    'storage': 'oracle_fiap',
                    'count': len(leituras),
                    'leituras': leituras
                })

            except Exception as db_error:
                logger.error(f"❌ Erro ao buscar no Oracle: {db_error}")
                if conn:
                    conn.close()

        # Fallback - ler do arquivo JSON
        fallback_data = load_fallback_data()
        leituras = fallback_data['leituras'][-limit:] if fallback_data['leituras'] else []
        leituras.reverse()  # Mais recentes primeiro

        logger.info(f"📊 Listadas {len(leituras)} leituras do arquivo JSON")

        return jsonify({
            'success': True,
            'storage': 'json_fallback',
            'count': len(leituras),
            'leituras': leituras
        })

    except Exception as e:
        logger.error(f"❌ Erro ao listar leituras: {e}")
        return jsonify({
            'error': 'Erro ao buscar leituras',
            'details': str(e)
        }), 500


@app.route('/api/debug', methods=['GET'])
def debug_info():
    """Informações completas de debug"""
    try:
        # Testar conexão
        db_status = test_db_connection()

        # Informações da conexão Oracle
        debug_data = {
            'oracle_module_available': ORACLE_AVAILABLE,
            'oracle_connection_test': db_status,
            'oracle_config': {
                'host': ORACLE_CONFIG['host'],
                'port': ORACLE_CONFIG['port'],
                'service': ORACLE_CONFIG['service'],
                'user': ORACLE_CONFIG['user'],
                'password_length': len(ORACLE_CONFIG['password'])
            },
            'dsn_variations': create_dsn_variations(),
            'fallback_info': {
                'file': FALLBACK_DATA_FILE,
                'exists': os.path.exists(FALLBACK_DATA_FILE),
                'data': load_fallback_data() if os.path.exists(FALLBACK_DATA_FILE) else None
            },
            'system_info': {
                'current_directory': os.getcwd(),
                'python_version': __import__('sys').version,
                'oracle_client_version': None
            },
            'timestamp': datetime.now().isoformat()
        }

        # Tentar obter versão do Oracle Client
        if ORACLE_AVAILABLE:
            try:
                debug_data['system_info']['oracle_client_version'] = cx_Oracle.clientversion()
            except:
                debug_data['system_info']['oracle_client_version'] = 'Não disponível'

        return jsonify(debug_data)

    except Exception as e:
        return jsonify({
            'error': 'Erro ao gerar debug info',
            'details': str(e)
        }), 500


# =============================================================================
# CONFIGURAÇÃO E INICIALIZAÇÃO
# =============================================================================

@app.errorhandler(404)
def not_found(error):
    return jsonify({'error': 'Endpoint não encontrado', 'timestamp': datetime.now().isoformat()}), 404


@app.errorhandler(500)
def internal_error(error):
    return jsonify({'error': 'Erro interno do servidor', 'timestamp': datetime.now().isoformat()}), 500


if __name__ == '__main__':
    print("🌊 Iniciando WaterWise API para Oracle FIAP...")
    print("=" * 60)

    # Informações de configuração
    print(f"🏢 Servidor Oracle: {ORACLE_CONFIG['host']}:{ORACLE_CONFIG['port']}")
    print(f"🗄️  Service: {ORACLE_CONFIG['service']}")
    print(f"👤 Usuário: {ORACLE_CONFIG['user']}")
    print(f"📍 Diretório: {os.getcwd()}")
    print(f"🔧 cx_Oracle: {'Disponível' if ORACLE_AVAILABLE else 'NÃO INSTALADO'}")

    # Testar conexão Oracle FIAP
    db_connected = test_db_connection()

    if db_connected:
        print("✅ Conexão com Oracle FIAP estabelecida!")
        print("💾 Dados serão salvos no Oracle FIAP")
    else:
        print("⚠️  Oracle FIAP não acessível - usando modo fallback")
        print(f"💾 Dados serão salvos em: {FALLBACK_DATA_FILE}")
        if not ORACLE_AVAILABLE:
            print("🔧 Instale cx_Oracle: pip install cx_Oracle")

    print("\n📡 API disponível em:")
    print("   - http://localhost:5000 (informações gerais)")
    print("   - http://localhost:5000/health (health check)")
    print("   - http://localhost:5000/api/leituras (POST/GET)")
    print("   - http://localhost:5000/api/alertas (POST)")
    print("   - http://localhost:5000/api/debug (debug completo)")
    print("=" * 60)

    # Iniciar servidor Flask
    app.run(
        host='0.0.0.0',  # Permitir conexões externas (ESP32)
        port=5000,
        debug=True
    )