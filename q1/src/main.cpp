/*
 * WaterWise - Sistema Inteligente de Prevenção a Enchentes
 * Global Solution 2025 - FIAP
 * Disciplina: DISRUPTIVE ARCHITECTURES: IOT, IOB & GENERATIVE IA
 *
 * SISTEMA IOT COM 3 SENSORES:
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
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

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
// 📡 CONFIGURAÇÕES MQTT (MQTT PÚBLICO PARA DEMONSTRAÇÃO)

const char *MQTT_BROKER = "localhost"; // MQTT público
const int MQTT_PORT = 1883;
const char *MQTT_USER = "";     // Sem usuário
const char *MQTT_PASSWORD = ""; // Sem senha

// Tópicos MQTT WaterWise
const char *TOPIC_SENSORS = "fiap/waterwise/sensors/data";
const char *TOPIC_ALERTS = "fiap/waterwise/alerts/flood";
const char *TOPIC_STATUS = "fiap/waterwise/status/system";

//----------------------------------------------------------
// 🏷️ IDENTIFICADORES WATERWISE

const char *FARM_ID = "FARM_WaterWise_2025";
const char *TEAM_NAME = "GRUPO_WATERWISE";
const char *LOCATION = "SP_Zona_Rural";
const char *PROJECT_VERSION = "WaterWise-v2.0-3Sensors";

//----------------------------------------------------------
// 🎮 CONFIGURAÇÕES DE SIMULAÇÃO

bool SIMULATION_MODE = true;            // Ativar dados simulados
unsigned long lastSimulationUpdate = 0; // Controle de simulação
int simulationCycle = 0;                // Ciclo de simulação

//----------------------------------------------------------
// 📊 OBJETOS GLOBAIS

WiFiClient espClient;
PubSubClient mqtt(espClient);
DHT dht(DHT_PIN, DHT_TYPE);
JsonDocument sensorData;
JsonDocument alertData;
char jsonBuffer[1024];

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
    if (now - lastSimulationUpdate < 20000)
        return; // Atualizar a cada 30s

    lastSimulationUpdate = now;
    simulationCycle++;

    // Ciclo de simulação: Normal → Seca → Chuva → Crítico → Normal
    switch (simulationCycle % 5)
    {
    case 0: // Condições Normais
        sensors.soilMoisturePercent = random(50, 80);
        sensors.rainIntensity = random(0, 20);
        Serial.println("🎭 [SIMULAÇÃO] Condições normais");
        break;

    case 1: // Solo Seco
        sensors.soilMoisturePercent = random(10, 30);
        sensors.rainIntensity = random(0, 15);
        Serial.println("🎭 [SIMULAÇÃO] Solo ficando seco");
        break;

    case 2: // Chuva Moderada
        sensors.soilMoisturePercent = random(40, 60);
        sensors.rainIntensity = random(30, 60);
        Serial.println("🎭 [SIMULAÇÃO] Chuva moderada");
        break;

    case 3:                                          // SITUAÇÃO CRÍTICA!
        sensors.soilMoisturePercent = random(5, 20); // Solo muito seco
        sensors.rainIntensity = random(70, 95);      // Chuva intensa
        Serial.println("🎭 [SIMULAÇÃO] ⚠️ SITUAÇÃO CRÍTICA - Solo seco + Chuva intensa!");
        break;

    case 4: // Recuperação
        sensors.soilMoisturePercent = random(60, 90);
        sensors.rainIntensity = random(5, 25);
        Serial.println("🎭 [SIMULAÇÃO] Situação se normalizando");
        break;
    }

    // Converter para valores raw (simulados)
    sensors.soilMoistureRaw = map(sensors.soilMoisturePercent, 0, 100, 0, 4095);
    sensors.rainLevelRaw = map(sensors.rainIntensity, 0, 100, 0, 4095);

    // Adicionar pequena variação aleatória
    sensors.soilMoisturePercent += random(-3, 3);
    sensors.rainIntensity += random(-5, 5);

    // Garantir limites
    sensors.soilMoisturePercent = constrain(sensors.soilMoisturePercent, 0, 100);
    sensors.rainIntensity = constrain(sensors.rainIntensity, 0, 100);
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

    // Definir descrições
    if (analysis.riskLevel <= 2)
    {
        analysis.riskDescription = "Baixo - Condições normais";
        analysis.recommendation = "Monitoramento rotineiro";
    }
    else if (analysis.riskLevel <= 4)
    {
        analysis.riskDescription = "Moderado - Atenção";
        analysis.recommendation = "Intensificar monitoramento";
    }
    else if (analysis.riskLevel <= 6)
    {
        analysis.riskDescription = "Alto - Preparação";
        analysis.recommendation = "Preparar sistemas de drenagem";
    }
    else if (analysis.riskLevel <= 8)
    {
        analysis.riskDescription = "Muito Alto - Ação imediata";
        analysis.recommendation = "Alertar autoridades";
    }
    else
    {
        analysis.riskDescription = "CRÍTICO - EMERGÊNCIA";
        analysis.recommendation = "EVACUAR ÁREAS DE RISCO";
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
// 📡 INICIALIZAÇÃO MQTT

void initMQTT()
{
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);

    Serial.printf("Conectando MQTT: %s:%d\n", MQTT_BROKER, MQTT_PORT);

    String clientId = "WaterWise-" + String(random(0xffff), HEX);

    if (mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD))
    {
        Serial.println("✅ MQTT Conectado!");

        // Publicar status online
        String statusMsg = "{\"system\":\"WaterWise\",\"status\":\"online\",\"sensors\":3}";
        mqtt.publish(TOPIC_STATUS, statusMsg.c_str());
    }
    else
    {
        Serial.printf("⚠️ MQTT não conectou (código: %d) - Modo offline\n", mqtt.state());
    }
}

//----------------------------------------------------------
// 📊 LEITURA DOS 3 SENSORES

void readAllSensors()
{
    sensors.timestamp = millis();

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
    int soilRaw = analogRead(SOIL_MOISTURE_PIN);

    if (SIMULATION_MODE)
    {
        // Usar dados simulados se em modo simulação
        simulateRealisticData();
    }
    else
    {
        // Usar dados reais dos potenciômetros
        sensors.soilMoistureRaw = soilRaw;
        sensors.soilMoisturePercent = map(soilRaw, 0, 4095, 0, 100);
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

    // 🌧️ SENSOR 3: Intensidade de Chuva
    int rainRaw = analogRead(RAIN_SENSOR_PIN);

    if (!SIMULATION_MODE)
    {
        // Usar dados reais dos potenciômetros
        sensors.rainLevelRaw = rainRaw;
        sensors.rainIntensity = map(rainRaw, 0, 4095, 0, 100);
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
// 📡 PUBLICAÇÃO MQTT - DADOS DOS 3 SENSORES

void publishSensorData()
{
    sensorData.clear();

    // IDENTIFICAÇÃO WATERWISE
    sensorData["system"] = PROJECT_VERSION;
    sensorData["farmId"] = FARM_ID;
    sensorData["teamName"] = TEAM_NAME;
    sensorData["location"] = LOCATION;
    sensorData["timestamp"] = sensors.timestamp;
    sensorData["ip"] = WiFi.localIP().toString();
    sensorData["mac"] = WiFi.macAddress();
    sensorData["simulation"] = SIMULATION_MODE;

    // SENSORES IOT (3 sensores)
    JsonObject sensorsObj = sensorData["sensors"].to<JsonObject>();

    // SENSOR 1: DHT22
    JsonObject dht22 = sensorsObj["dht22"].to<JsonObject>();
    dht22["type"] = "sensor";
    dht22["description"] = "Temperatura e Umidade Ambiente";
    dht22["temperature"] = round(sensors.temperature * 10) / 10.0;
    dht22["humidity"] = round(sensors.airHumidity * 10) / 10.0;
    dht22["status"] = sensors.dhtStatus ? "active" : "simulated";
    dht22["pin"] = DHT_PIN;

    // SENSOR 2: Solo
    JsonObject soilSensor = sensorsObj["soilSensor"].to<JsonObject>();
    soilSensor["type"] = "sensor";
    soilSensor["description"] = "Umidade do Solo";
    soilSensor["moisture_raw"] = sensors.soilMoistureRaw;
    soilSensor["moisture_percent"] = round(sensors.soilMoisturePercent * 10) / 10.0;
    soilSensor["status"] = sensors.soilStatus;
    soilSensor["pin"] = SOIL_MOISTURE_PIN;

    // SENSOR 3: Chuva
    JsonObject rainSensor = sensorsObj["rainSensor"].to<JsonObject>();
    rainSensor["type"] = "sensor";
    rainSensor["description"] = "Intensidade de Precipitação";
    rainSensor["rain_raw"] = sensors.rainLevelRaw;
    rainSensor["intensity_percent"] = round(sensors.rainIntensity * 10) / 10.0;
    rainSensor["status"] = sensors.rainStatus;
    rainSensor["pin"] = RAIN_SENSOR_PIN;

    // ANÁLISE DE RISCO WATERWISE
    FloodRiskAnalysis risk = analyzeFloodRisk();
    JsonObject analysis = sensorData["analysis"].to<JsonObject>();
    analysis["algorithm"] = "WaterWise-v2.0";
    analysis["risk_level"] = risk.riskLevel;
    analysis["risk_description"] = risk.riskDescription;
    analysis["recommendation"] = risk.recommendation;
    analysis["flood_alert"] = risk.floodAlert;
    analysis["drought_alert"] = risk.droughtAlert;
    analysis["extreme_weather_alert"] = risk.extremeWeatherAlert;
    analysis["absorption_capacity"] = round(risk.absorptionCapacity * 10) / 10.0;
    analysis["runoff_risk"] = round(risk.runoffRisk * 10) / 10.0;

    // SERIALIZAR E PUBLICAR
    serializeJson(sensorData, jsonBuffer);

    // Tentar publicar MQTT
    bool mqttSuccess = false;
    if (mqtt.connected())
    {
        mqttSuccess = mqtt.publish(TOPIC_SENSORS, jsonBuffer);
    }

    if (mqttSuccess)
    {
        Serial.println("📡 ✅ Dados dos 3 sensores publicados no MQTT");
    }
    else
    {
        Serial.println("📡 ⚠️ MQTT offline - Dados disponíveis localmente");
    }

    // Mostrar JSON sempre (para demonstração)
    Serial.println("📄 JSON Gerado (3 Sensores):");
    Serial.println(jsonBuffer);
}

//----------------------------------------------------------
// 🚨 PUBLICAÇÃO DE ALERTAS

void publishAlerts()
{
    FloodRiskAnalysis risk = analyzeFloodRisk();

    if (!risk.floodAlert && !risk.droughtAlert && !risk.extremeWeatherAlert)
    {
        return; // Só publica se houver alerta
    }

    String alertMsg = "{\"alert\":\"" + risk.riskDescription + "\",\"level\":" + String(risk.riskLevel) + "}";

    if (mqtt.connected())
    {
        if (mqtt.publish(TOPIC_ALERTS, alertMsg.c_str()))
        {
            Serial.println("🚨 ✅ ALERTA ENVIADO VIA MQTT!");
        }
    }
    else
    {
        Serial.println("🚨 ⚠️ ALERTA GERADO (MQTT offline): " + risk.riskDescription);
    }
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
    initMQTT();

    // Primeira leitura
    Serial.println("📡 Primeira leitura dos 3 sensores...");
    readAllSensors();

    Serial.println("\n🚀 === WATERWISE SISTEMA IOT ONLINE ===");
    Serial.println("3 Sensores: DHT22 + Solo + Chuva");
    Serial.println("Simulação: Dados realistas ativada");
    Serial.println("Protocolos: MQTT + JSON + HTTP");
    Serial.println("Intervalo: 15 segundos");
    Serial.println("==================================================\n");
}

//----------------------------------------------------------
// 🔄 LOOP PRINCIPAL

void loop()
{
    // Manter conexão MQTT (sem bloquear)
    if (mqtt.connected())
    {
        mqtt.loop();
    }

    // Leitura dos sensores
    readAllSensors();

    // Publicar dados
    publishSensorData();

    // Verificar alertas
    publishAlerts();

    // Feedback LED
    waterWiseStatusFeedback();

    // Status resumido
    FloodRiskAnalysis risk = analyzeFloodRisk();
    Serial.printf("\n⏱️  WaterWise | 3 Sensores | Risco: %d/10 | Ciclo: %d | Próxima: 15s\n",
                  risk.riskLevel, simulationCycle % 5);
    Serial.printf("📊 Solo: %.1f%% | Chuva: %.1f%% | Temp: %.1f°C\n",
                  sensors.soilMoisturePercent, sensors.rainIntensity, sensors.temperature);

    if (SIMULATION_MODE)
    {
        Serial.println("🎭 Modo Simulação: Dados variando automaticamente");
    }

    Serial.println("--------------------------------------------------");

    // Aguardar próxima leitura
    delay(15000);
}