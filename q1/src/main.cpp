/*
 * WaterWise - Sistema Inteligente de Prevenção a Enchentes
 * Global Solution 2025 - FIAP
 * Disciplina: DISRUPTIVE ARCHITECTURES: IOT, IOB & GENERATIVE IA
 *
 * SISTEMA IOT COM 3 SENSORES INTEGRADO AO BANCO ORACLE:
 * 1. DHT22 - Sensor Temperatura/Umidade
 * 2. Sensor Umidade do Solo (simulado com potenciômetro)
 * 3. Sensor de Precipitação (simulado com potenciômetro)
 *
 * ALGORITMO WATERWISE: Solo seco + Chuva intensa = Alto risco de enchente
 *
 * AUTORES:
 * Felipe Amador 553528
 * Leonardo de Oliveira	554024
 * Sara Sousa	552656
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

//----------------------------------------------------------
// 🔧 DEFINIÇÕES DE PINOS - 3 SENSORES

#define DHT_PIN 12 // Sensor 1: DHT22 (Temperatura/Umidade)
#define DHT_TYPE DHT22
#define SOIL_MOISTURE_PIN 34 // Sensor 2: Umidade do Solo
#define RAIN_SENSOR_PIN 35   // Sensor 3: Intensidade de Chuva
#define LED_BUILTIN 2        // LED interno ESP32

//----------------------------------------------------------
// 🌐 CONFIGURAÇÕES DE REDE

const char *SSID = "Wokwi-GUEST"; // Para Wokwi
const char *PASSWORD = "";        // Para Wokwi

//----------------------------------------------------------
// 🔗 CONFIGURAÇÕES API BACKEND (CONFIGURE SEU IP LOCAL)

const char *API_BASE_URL = "http://192.168.0.202:5000/";
const char *API_ENDPOINT_LEITURA = "/api/leituras";
const char *API_ENDPOINT_ALERTA = "/api/alertas";
const char *API_KEY = ""; // Não precisa de API key

//----------------------------------------------------------
// 🏷️ IDENTIFICADORES WATERWISE FIAP

const int SENSOR_ID = 1;           // ID do sensor no banco Oracle FIAP
const int PRODUTOR_ID = 1;         // ID do produtor no banco
const char *FARM_ID = "FARM_WaterWise_2025";
const char *TEAM_NAME = "GRUPO_WATERWISE";
const char *LOCATION = "SP_Zona_Rural";
const char *PROJECT_VERSION = "WaterWise-v2.0-3Sensors";

//----------------------------------------------------------
// 🎮 CONFIGURAÇÕES DE SIMULAÇÃO

bool SIMULATION_MODE = true;            // Ativar dados simulados
unsigned long lastSimulationUpdate = 0; // Controle de simulação
unsigned long lastDataSend = 0;         // Controle de envio de dados
int simulationCycle = 0;                // Ciclo de simulação

//----------------------------------------------------------
// 📊 OBJETOS GLOBAIS

HTTPClient http;
DHT dht(DHT_PIN, DHT_TYPE);
JsonDocument sensorData;
JsonDocument alertData;
String jsonString;

//----------------------------------------------------------
// 📊 ESTRUTURAS DE DADOS

struct WaterWiseSensors
{
    // Sensor 1: DHT22
    float temperature;
    float airHumidity;
    bool dhtStatus;

    // Sensor 2: Solo
    int soilMoistureRaw;
    float soilMoisturePercent;
    String soilStatus;

    // Sensor 3: Chuva
    int rainLevelRaw;
    float rainIntensity;
    String rainStatus;

    // Timestamp
    unsigned long timestamp;
};

struct FloodRiskAnalysis
{
    int riskLevel;
    bool floodAlert;
    bool droughtAlert;
    bool extremeWeatherAlert;
    String riskDescription;
    String recommendation;
    float absorptionCapacity;
    float runoffRisk;
    String severityCode;
};

WaterWiseSensors sensors;

//----------------------------------------------------------
// ⚙️ CONFIGURAÇÕES DE ALERTAS

const float SOIL_DRY_THRESHOLD = 25.0;
const float RAIN_HEAVY_THRESHOLD = 70.0;
const float TEMP_EXTREME_THRESHOLD = 35.0;

//----------------------------------------------------------
// 🎮 SIMULAÇÃO DE DADOS REALISTAS

void simulateRealisticData()
{
    if (!SIMULATION_MODE)
        return;

    unsigned long now = millis();
    if (now - lastSimulationUpdate < 10000)
        return; // Atualizar a cada 10s

    lastSimulationUpdate = now;
    simulationCycle++;

    // Ciclo de simulação: Normal → Seca → Chuva → Crítico → Normal
    switch (simulationCycle % 5)
    {
    case 0: // Condições Normais
        sensors.soilMoisturePercent = random(50, 80);
        sensors.rainIntensity = random(0, 20);
        sensors.temperature = random(20, 28);
        sensors.airHumidity = random(60, 80);
        Serial.println("🎭 [SIMULAÇÃO] Condições normais");
        break;

    case 1: // Solo Seco
        sensors.soilMoisturePercent = random(10, 30);
        sensors.rainIntensity = random(0, 15);
        sensors.temperature = random(25, 32);
        sensors.airHumidity = random(40, 60);
        Serial.println("🎭 [SIMULAÇÃO] Solo ficando seco");
        break;

    case 2: // Chuva Moderada
        sensors.soilMoisturePercent = random(40, 60);
        sensors.rainIntensity = random(30, 60);
        sensors.temperature = random(18, 25);
        sensors.airHumidity = random(70, 90);
        Serial.println("🎭 [SIMULAÇÃO] Chuva moderada");
        break;

    case 3: // SITUAÇÃO CRÍTICA!
        sensors.soilMoisturePercent = random(5, 20); // Solo muito seco
        sensors.rainIntensity = random(70, 95);      // Chuva intensa
        sensors.temperature = random(30, 38);        // Temperatura alta
        sensors.airHumidity = random(80, 95);        // Umidade alta
        Serial.println("🎭 [SIMULAÇÃO] ⚠️ SITUAÇÃO CRÍTICA - Solo seco + Chuva intensa!");
        break;

    case 4: // Recuperação
        sensors.soilMoisturePercent = random(60, 90);
        sensors.rainIntensity = random(5, 25);
        sensors.temperature = random(22, 28);
        sensors.airHumidity = random(65, 75);
        Serial.println("🎭 [SIMULAÇÃO] Situação se normalizando");
        break;
    }

    // Converter para valores raw (simulados)
    sensors.soilMoistureRaw = map(sensors.soilMoisturePercent, 0, 100, 0, 4095);
    sensors.rainLevelRaw = map(sensors.rainIntensity, 0, 100, 0, 4095);

    // Adicionar pequena variação aleatória
    sensors.soilMoisturePercent += random(-3, 3);
    sensors.rainIntensity += random(-5, 5);
    sensors.temperature += random(-2, 2);
    sensors.airHumidity += random(-5, 5);

    // Garantir limites
    sensors.soilMoisturePercent = constrain(sensors.soilMoisturePercent, 0, 100);
    sensors.rainIntensity = constrain(sensors.rainIntensity, 0, 100);
    sensors.temperature = constrain(sensors.temperature, -10, 50);
    sensors.airHumidity = constrain(sensors.airHumidity, 0, 100);
    
    sensors.dhtStatus = true; // Simular DHT funcionando
}

//----------------------------------------------------------
// 🧮 ALGORITMO ANÁLISE DE RISCO WATERWISE

FloodRiskAnalysis analyzeFloodRisk()
{
    FloodRiskAnalysis analysis;
    analysis.riskLevel = 0;
    analysis.floodAlert = false;
    analysis.droughtAlert = false;
    analysis.extremeWeatherAlert = false;

    // FATOR SOLO (40% do peso)
    if (sensors.soilMoisturePercent < 15)
    {
        analysis.riskLevel += 4;
        analysis.droughtAlert = true;
    }
    else if (sensors.soilMoisturePercent < 30)
    {
        analysis.riskLevel += 3;
    }
    else if (sensors.soilMoisturePercent < 50)
    {
        analysis.riskLevel += 1;
    }

    // FATOR CHUVA (50% do peso)
    if (sensors.rainIntensity > 80)
    {
        analysis.riskLevel += 5;
    }
    else if (sensors.rainIntensity > 60)
    {
        analysis.riskLevel += 4;
    }
    else if (sensors.rainIntensity > 40)
    {
        analysis.riskLevel += 2;
    }
    else if (sensors.rainIntensity > 20)
    {
        analysis.riskLevel += 1;
    }

    // FATOR TEMPERATURA (10% do peso)
    if (sensors.temperature > TEMP_EXTREME_THRESHOLD)
    {
        analysis.riskLevel += 1;
        analysis.extremeWeatherAlert = true;
    }

    // COMBINAÇÃO CRÍTICA: Solo seco + Chuva intensa
    if (sensors.soilMoisturePercent < SOIL_DRY_THRESHOLD &&
        sensors.rainIntensity > RAIN_HEAVY_THRESHOLD)
    {
        analysis.riskLevel += 2; // Bônus crítico
    }

    // Limitar risco máximo
    analysis.riskLevel = constrain(analysis.riskLevel, 0, 10);

    // Calcular métricas
    analysis.absorptionCapacity = 100 - sensors.soilMoisturePercent;
    analysis.runoffRisk = max(0.0f, sensors.rainIntensity - (sensors.soilMoisturePercent * 0.8f));

    // Definir descrições e códigos de severidade
    if (analysis.riskLevel <= 2)
    {
        analysis.riskDescription = "Baixo - Condições normais";
        analysis.recommendation = "Monitoramento rotineiro";
        analysis.severityCode = "BAIXO";
    }
    else if (analysis.riskLevel <= 4)
    {
        analysis.riskDescription = "Moderado - Atenção";
        analysis.recommendation = "Intensificar monitoramento";
        analysis.severityCode = "MEDIO";
    }
    else if (analysis.riskLevel <= 6)
    {
        analysis.riskDescription = "Alto - Preparação";
        analysis.recommendation = "Preparar sistemas de drenagem";
        analysis.severityCode = "ALTO";
    }
    else if (analysis.riskLevel <= 8)
    {
        analysis.riskDescription = "Muito Alto - Ação imediata";
        analysis.recommendation = "Alertar autoridades";
        analysis.severityCode = "CRITICO";
    }
    else
    {
        analysis.riskDescription = "CRÍTICO - EMERGÊNCIA";
        analysis.recommendation = "EVACUAR ÁREAS DE RISCO";
        analysis.severityCode = "CRITICO";
    }

    analysis.floodAlert = (analysis.riskLevel >= 7);

    return analysis;
}

//----------------------------------------------------------
// 🌐 INICIALIZAÇÃO WIFI

void initWiFi()
{
    Serial.println("\n==================================================");
    Serial.println("🌊 WATERWISE - SISTEMA IOT 3 SENSORES v2.0");
    Serial.println("Global Solution 2025 - FIAP");
    Serial.println("==================================================");
    Serial.printf("Farm ID: %s\n", FARM_ID);
    Serial.printf("Equipe: %s\n", TEAM_NAME);
    Serial.printf("Versão: %s\n", PROJECT_VERSION);
    Serial.printf("Simulação: %s\n", SIMULATION_MODE ? "ATIVA" : "Desabilitada");
    Serial.println("==================================================");

    WiFi.begin(SSID, PASSWORD);
    Serial.printf("Conectando ao WiFi: %s", SSID);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(1000);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n✅ WiFi Conectado!");
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
    }
    else
    {
        Serial.println("\n❌ WiFi não conectou - continuando offline");
    }
}

//----------------------------------------------------------
// 📊 LEITURA DOS 3 SENSORES

void readAllSensors()
{
    sensors.timestamp = millis();

    if (SIMULATION_MODE)
    {
        // Usar dados simulados
        simulateRealisticData();
    }
    else
    {
        // 🌡️ SENSOR 1: DHT22 (Temperatura e Umidade do Ar)
        sensors.temperature = dht.readTemperature();
        sensors.airHumidity = dht.readHumidity();
        sensors.dhtStatus = !isnan(sensors.temperature) && !isnan(sensors.airHumidity);

        if (!sensors.dhtStatus)
        {
            sensors.temperature = 25.0 + random(-3, 3);   // Valor simulado
            sensors.airHumidity = 60.0 + random(-10, 10); // Valor simulado
        }

        // 🌱 SENSOR 2: Umidade do Solo
        sensors.soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN);
        sensors.soilMoisturePercent = map(sensors.soilMoistureRaw, 0, 4095, 0, 100);

        // 🌧️ SENSOR 3: Intensidade de Chuva
        sensors.rainLevelRaw = analogRead(RAIN_SENSOR_PIN);
        sensors.rainIntensity = map(sensors.rainLevelRaw, 0, 4095, 0, 100);
    }

    // Definir status do solo
    if (sensors.soilMoisturePercent < 20)
    {
        sensors.soilStatus = "Crítico";
    }
    else if (sensors.soilMoisturePercent < 40)
    {
        sensors.soilStatus = "Seco";
    }
    else if (sensors.soilMoisturePercent < 70)
    {
        sensors.soilStatus = "Normal";
    }
    else
    {
        sensors.soilStatus = "Saturado";
    }

    // Definir status da chuva
    if (sensors.rainIntensity < 10)
    {
        sensors.rainStatus = "Sem chuva";
    }
    else if (sensors.rainIntensity < 30)
    {
        sensors.rainStatus = "Leve";
    }
    else if (sensors.rainIntensity < 70)
    {
        sensors.rainStatus = "Moderada";
    }
    else
    {
        sensors.rainStatus = "Intensa";
    }

    // 🖥️ Debug das leituras
    Serial.println("\n📊 === LEITURA 3 SENSORES WATERWISE ===");
    Serial.printf("🌡️  DHT22: %.1f°C, %.1f%% - %s\n",
                  sensors.temperature, sensors.airHumidity,
                  sensors.dhtStatus ? "OK" : "SIMULADO");
    Serial.printf("🌱 Solo: %d raw (%.1f%%) - %s\n",
                  sensors.soilMoistureRaw, sensors.soilMoisturePercent,
                  sensors.soilStatus.c_str());
    Serial.printf("🌧️  Chuva: %d raw (%.1f%%) - %s\n",
                  sensors.rainLevelRaw, sensors.rainIntensity,
                  sensors.rainStatus.c_str());

    // Análise de risco
    FloodRiskAnalysis risk = analyzeFloodRisk();
    Serial.printf("🧮 Risco: %d/10 - %s\n",
                  risk.riskLevel, risk.riskDescription.c_str());

    if (risk.floodAlert)
    {
        Serial.println("🚨 ⚠️ ALERTA DE ENCHENTE ATIVO! ⚠️");
    }
    if (risk.droughtAlert)
    {
        Serial.println("🚨 ⚠️ ALERTA DE SECA ATIVO! ⚠️");
    }
}

//----------------------------------------------------------
// 📡 ENVIO DE DADOS PARA O BANCO VIA API

bool sendSensorDataToDatabase()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("❌ WiFi desconectado - não é possível enviar dados");
        return false;
    }

    // Limpar documento JSON
    sensorData.clear();
    
    // Criar JSON para envio ao banco
    sensorData["id_sensor"] = SENSOR_ID;
    sensorData["umidade_solo"] = round(sensors.soilMoisturePercent * 100) / 100.0;
    sensorData["temperatura_ar"] = round(sensors.temperature * 100) / 100.0;
    sensorData["precipitacao_mm"] = round(sensors.rainIntensity * 100) / 100.0;
    sensorData["timestamp"] = "CURRENT_TIMESTAMP"; // O banco irá usar a timestamp atual
    sensorData["farm_id"] = FARM_ID;
    sensorData["team_name"] = TEAM_NAME;

    // Serializar JSON
    serializeJson(sensorData, jsonString);
    
    // Configurar requisição HTTP
    http.begin(String(API_BASE_URL) + API_ENDPOINT_LEITURA);
    http.addHeader("Content-Type", "application/json");
    if (strlen(API_KEY) > 0)
    {
        http.addHeader("Authorization", "Bearer " + String(API_KEY));
    }

    // Enviar dados
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.printf("✅ Dados enviados ao banco! Código: %d\n", httpResponseCode);
        Serial.println("📄 Payload JSON enviado:");
        Serial.println(jsonString);
        http.end();
        return true;
    }
    else
    {
        Serial.printf("❌ Erro ao enviar dados: %d\n", httpResponseCode);
        Serial.println("📄 Payload que tentou enviar:");
        Serial.println(jsonString);
        http.end();
        return false;
    }
}

//----------------------------------------------------------
// 🚨 ENVIO DE ALERTAS PARA O BANCO

bool sendAlertToDatabase(FloodRiskAnalysis risk)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    // Só enviar se houver alerta ativo
    if (!risk.floodAlert && !risk.droughtAlert && !risk.extremeWeatherAlert)
    {
        return true; // Não é erro, apenas não há alerta
    }

    // Limpar documento JSON
    alertData.clear();
    
    // Criar JSON para alerta
    alertData["id_produtor"] = PRODUTOR_ID;
    alertData["id_leitura"] = "LAST_INSERT_ID()"; // Usar a última leitura inserida
    alertData["codigo_severidade"] = risk.severityCode;
    alertData["descricao_alerta"] = risk.riskDescription + " - " + risk.recommendation;
    alertData["timestamp"] = "CURRENT_TIMESTAMP";

    String alertPayload;
    serializeJson(alertData, alertPayload);
    
    http.begin(String(API_BASE_URL) + API_ENDPOINT_ALERTA);
    http.addHeader("Content-Type", "application/json");
    if (strlen(API_KEY) > 0)
    {
        http.addHeader("Authorization", "Bearer " + String(API_KEY));
    }

    int httpResponseCode = http.POST(alertPayload);
    
    if (httpResponseCode > 0)
    {
        Serial.printf("🚨 ✅ ALERTA ENVIADO AO BANCO! Código: %d\n", httpResponseCode);
        http.end();
        return true;
    }
    else
    {
        Serial.printf("🚨 ❌ Erro ao enviar alerta: %d\n", httpResponseCode);
        http.end();
        return false;
    }
}

//----------------------------------------------------------
// 📡 ALTERNATIVA: INSERÇÃO DIRETA NO BANCO (para teste local)

void insertDataDirectlyToOracle()
{
    // Esta função simula a inserção direta no banco Oracle
    // Em um ambiente real, você precisaria de um middleware ou API
    
    Serial.println("📡 === SIMULANDO INSERÇÃO NO ORACLE ===");
    
    // Simular INSERT na tabela GS_WW_LEITURA_SENSOR
    Serial.println("SQL que seria executado:");
    Serial.printf("INSERT INTO GS_WW_LEITURA_SENSOR (ID_SENSOR, UMIDADE_SOLO, TEMPERATURA_AR, PRECIPITACAO_MM) VALUES (%d, %.2f, %.2f, %.2f);\n", 
                  SENSOR_ID, sensors.soilMoisturePercent, sensors.temperature, sensors.rainIntensity);
    
    // Verificar se precisa inserir alerta
    FloodRiskAnalysis risk = analyzeFloodRisk();
    if (risk.floodAlert || risk.droughtAlert || risk.extremeWeatherAlert)
    {
        // Simular INSERT na tabela GS_WW_ALERTA
        Serial.printf("INSERT INTO GS_WW_ALERTA (ID_PRODUTOR, ID_LEITURA, ID_NIVEL_SEVERIDADE, DESCRICAO_ALERTA) VALUES (%d, LAST_INSERT_ID(), (SELECT ID_NIVEL_SEVERIDADE FROM GS_WW_NIVEL_SEVERIDADE WHERE CODIGO_SEVERIDADE='%s'), '%s');\n",
                      PRODUTOR_ID, risk.severityCode.c_str(), risk.riskDescription.c_str());
    }
    
    Serial.println("========================================");
}

//----------------------------------------------------------
// 💡 FEEDBACK VISUAL LED INTERNO

void waterWiseStatusFeedback()
{
    FloodRiskAnalysis risk = analyzeFloodRisk();

    // LED interno indica nível de risco
    if (risk.riskLevel >= 7)
    {
        // CRÍTICO - Piscadas rápidas
        for (int i = 0; i < 6; i++)
        {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
    }
    else if (risk.riskLevel >= 4)
    {
        // MODERADO - Piscadas médias
        for (int i = 0; i < 3; i++)
        {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(300);
            digitalWrite(LED_BUILTIN, LOW);
            delay(300);
        }
    }
    else
    {
        // NORMAL - Piscada única
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
    }
}

//----------------------------------------------------------
// 🔄 CONFIGURAÇÃO INICIAL

void setup()
{
    Serial.begin(115200);
    delay(2000);

    // Configurar pinos
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(RAIN_SENSOR_PIN, INPUT);

    // Inicializar DHT22
    dht.begin();

    // Conectividade
    initWiFi();

    // Primeira leitura
    Serial.println("📡 Primeira leitura dos 3 sensores...");
    readAllSensors();

    Serial.println("\n🚀 === WATERWISE SISTEMA IOT ONLINE ===");
    Serial.println("3 Sensores: DHT22 + Solo + Chuva");
    Serial.println("Simulação: Dados realistas ativada");
    Serial.println("Protocolos: HTTP + JSON + Oracle");
    Serial.println("Intervalo: 30 segundos");
    Serial.println("==================================================\n");
}

//----------------------------------------------------------
// 🔄 LOOP PRINCIPAL

void loop()
{
    unsigned long now = millis();
    
    // Leitura dos sensores (sempre)
    readAllSensors();

    // Enviar dados para o banco a cada 30 segundos
    if (now - lastDataSend >= 30000)
    {
        lastDataSend = now;
        
        // Tentar enviar para API/banco real
        bool apiSuccess = sendSensorDataToDatabase();
        
        if (!apiSuccess)
        {
            // Se falhar, mostrar SQL que seria executado
            insertDataDirectlyToOracle();
        }
        
        // Verificar e enviar alertas
        FloodRiskAnalysis risk = analyzeFloodRisk();
        sendAlertToDatabase(risk);
    }

    // Feedback LED
    waterWiseStatusFeedback();

    // Status resumido
    FloodRiskAnalysis risk = analyzeFloodRisk();
    Serial.printf("\n⏱️  WaterWise | 3 Sensores | Risco: %d/10 | Ciclo: %d | Próximo envio: %lus\n",
                  risk.riskLevel, simulationCycle % 5, (30000 - (now - lastDataSend)) / 1000);
    Serial.printf("📊 Solo: %.1f%% | Chuva: %.1f%% | Temp: %.1f°C\n",
                  sensors.soilMoisturePercent, sensors.rainIntensity, sensors.temperature);

    if (SIMULATION_MODE)
    {
        Serial.println("🎭 Modo Simulação: Dados variando automaticamente");
    }

    Serial.println("--------------------------------------------------");

    // Aguardar próxima leitura (verificação a cada 5 segundos)
    delay(5000);
}