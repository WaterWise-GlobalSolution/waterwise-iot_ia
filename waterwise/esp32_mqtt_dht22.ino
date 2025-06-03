/*
 * WaterWise - Sistema Inteligente de Prevenção a Enchentes
 * Global Solution 2025 - FIAP
 * Disciplina: DISRUPTIVE ARCHITECTURES: IOT, IOB & GENERATIVE IA
 * 
 * Versão para Arduino IDE - Uso com placas físicas ESP32
 * 
 * DESCRIÇÃO:
 * Sistema IoT que monitora propriedades rurais para prevenir enchentes urbanas
 * através de sensores de umidade do solo, temperatura e precipitação.
 * 
 * ALGORITMO WATERWISE:
 * Solo seco + Chuva intensa = Alto risco de enchente
 * 
 * AUTORES: [INSERIR NOMES E RMs DO SEU GRUPO]
 * DATA: Junho 2025
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

//----------------------------------------------------------
// 🔧 DEFINIÇÕES DE PINOS WATERWISE

#define DHTPIN 12              // Sensor DHT22 - Temperatura/Umidade ar
#define DHTTYPE DHT22          // DHT22 (AM2302)
#define SOIL_MOISTURE_PIN 34   // Sensor umidade do solo (ADC)
#define RAIN_SENSOR_PIN 35     // Sensor de chuva (ADC)  
#define LED_BUILTIN 2          // LED onboard para feedback

//----------------------------------------------------------
// 🌐 CONFIGURAÇÕES DE REDE (ALTERE AQUI!)

const char* ssid = "Rapha";           // Nome da sua rede WiFi
const char* password = "13310866";      // Senha da sua rede WiFi

//----------------------------------------------------------
// 📡 CONFIGURAÇÕES MQTT (NÃO ALTERAR)

const char* mqtt_server = "172.208.54.189";
const int mqtt_port = 1883;
const char* mqtt_user = "gs2025";
const char* mqtt_password = "q1w2e3r4";

// Tópicos MQTT WaterWise
const char* topic_sensors = "waterwise/sensors";
const char* topic_alerts = "waterwise/alerts";
const char* topic_status = "waterwise/status";

//----------------------------------------------------------
// 🏷️ IDENTIFICADORES WATERWISE (ALTERE AQUI!)

const char* FARM_ID = "FARM_WaterWise_2025";        // ID único da fazenda
const char* TEAM_NAME = "GRUPO_WateWise";       // Nome do seu grupo
const char* LOCATION = "SP_Zona_Rural";             // Localização

//----------------------------------------------------------
// 📊 OBJETOS E VARIÁVEIS GLOBAIS

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
JsonDocument sensorData;
JsonDocument alertData;
char jsonBuffer[512];

// Estrutura dos dados dos sensores
struct WaterWiseSensors {
    float temperature;          // Temperatura em °C
    float airHumidity;         // Umidade do ar em %
    int soilMoistureRaw;       // Umidade solo raw (0-4095)
    int rainLevelRaw;          // Chuva raw (0-4095)
    float soilMoisturePercent; // Umidade solo em %
    float rainIntensity;       // Intensidade chuva em %
    String soilStatus;         // Status do solo
    String rainStatus;         // Status da chuva
    unsigned long timestamp;   // Timestamp da leitura
};

WaterWiseSensors sensors;

//----------------------------------------------------------
// ⚠️ CONFIGURAÇÕES DE ALERTAS

const float SOIL_DRY_THRESHOLD = 25.0;      // Solo crítico < 25%
const float RAIN_HEAVY_THRESHOLD = 70.0;    // Chuva intensa > 70%
const float TEMP_EXTREME_THRESHOLD = 35.0;  // Temperatura extrema > 35°C

//----------------------------------------------------------
// 🌐 FUNÇÃO DE CONEXÃO WIFI

void setup_wifi() {
    delay(10);
    
    // Banner WaterWise
    Serial.println("\n==================================================");
    Serial.println("🌊 WATERWISE - PREVENÇÃO DE ENCHENTES");
    Serial.println("Global Solution 2025 - FIAP");
    Serial.println("==================================================");
    Serial.print("Farm ID: ");
    Serial.println(FARM_ID);
    Serial.print("Equipe: ");
    Serial.println(TEAM_NAME);
    Serial.println("==================================================");
    
    Serial.print("Conectando ao WiFi: ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    
    Serial.println("\n✅ WiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC: ");
    Serial.println(WiFi.macAddress());
}

//----------------------------------------------------------
// 📡 FUNÇÃO DE CONEXÃO MQTT

void reconnect() {
    while (!client.connected()) {
        Serial.print("Conectando ao MQTT Broker...");
        
        if (client.connect(FARM_ID, mqtt_user, mqtt_password)) {
            Serial.println(" ✅ conectado!");
            
            // Publicar status online
            client.publish(topic_status, "{\"status\":\"online\",\"system\":\"WaterWise\"}");
            
        } else {
            Serial.print(" ❌ falhou, rc=");
            Serial.print(client.state());
            Serial.println(" tentando novamente em 5 segundos");
            delay(5000);
        }
    }
}

//----------------------------------------------------------
// 📊 FUNÇÃO DE LEITURA DOS SENSORES

void readAllSensors() {
    sensors.timestamp = millis();
    
    // 🌡️ Leitura DHT22 (temperatura e umidade do ar)
    sensors.temperature = dht.readTemperature();
    sensors.airHumidity = dht.readHumidity();
    
    // 🌱 Leitura sensor umidade do solo
    sensors.soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN);
    sensors.soilMoisturePercent = map(sensors.soilMoistureRaw, 0, 4095, 0, 100);
    
    // 🌧️ Leitura sensor de chuva
    sensors.rainLevelRaw = analogRead(RAIN_SENSOR_PIN);
    sensors.rainIntensity = map(sensors.rainLevelRaw, 0, 4095, 0, 100);
    
    // 📊 Classificação do status do solo
    if (sensors.soilMoisturePercent < 20) {
        sensors.soilStatus = "Crítico - Muito Seco";
    } else if (sensors.soilMoisturePercent < 40) {
        sensors.soilStatus = "Seco";
    } else if (sensors.soilMoisturePercent < 70) {
        sensors.soilStatus = "Normal";
    } else {
        sensors.soilStatus = "Saturado";
    }
    
    // 🌧️ Classificação da intensidade da chuva
    if (sensors.rainIntensity < 10) {
        sensors.rainStatus = "Sem chuva";
    } else if (sensors.rainIntensity < 30) {
        sensors.rainStatus = "Chuva leve";
    } else if (sensors.rainIntensity < 70) {
        sensors.rainStatus = "Chuva moderada";
    } else {
        sensors.rainStatus = "Chuva intensa";
    }
    
    // 🖥️ Debug das leituras
    Serial.println("\n📊 === LEITURA SENSORES WATERWISE ===");
    Serial.printf("🌡️  Temperatura: %.1f°C\n", sensors.temperature);
    Serial.printf("💨 Umidade Ar: %.1f%%\n", sensors.airHumidity);
    Serial.printf("🌱 Solo: %d raw (%.1f%%) - %s\n", 
                  sensors.soilMoistureRaw, sensors.soilMoisturePercent, sensors.soilStatus.c_str());
    Serial.printf("🌧️  Chuva: %d raw (%.1f%%) - %s\n", 
                  sensors.rainLevelRaw, sensors.rainIntensity, sensors.rainStatus.c_str());
}

//----------------------------------------------------------
// 🧮 ALGORITMO DE ANÁLISE DE RISCO WATERWISE

struct FloodRiskAnalysis {
    int riskLevel;              // 0-10
    bool floodAlert;            // true se risco >= 7
    bool droughtAlert;          // true se solo muito seco
    bool extremeWeatherAlert;   // true se condições extremas
    String riskDescription;     // Descrição do risco
    String recommendation;      // Recomendação
    float timeToFlood;         // Tempo estimado (minutos)
};

FloodRiskAnalysis analyzeFloodRisk() {
    FloodRiskAnalysis analysis;
    analysis.riskLevel = 0;
    analysis.floodAlert = false;
    analysis.droughtAlert = false;
    analysis.extremeWeatherAlert = false;
    
    // 🌱 FATOR SOLO (40% do peso)
    if (sensors.soilMoisturePercent < 15) {
        analysis.riskLevel += 4;  // Crítico
        analysis.droughtAlert = true;
    } else if (sensors.soilMoisturePercent < 30) {
        analysis.riskLevel += 3;  // Seco
    } else if (sensors.soilMoisturePercent < 50) {
        analysis.riskLevel += 1;  // Baixo
    }
    
    // 🌧️ FATOR CHUVA (50% do peso)
    if (sensors.rainIntensity > 80) {
        analysis.riskLevel += 5;  // Extrema
    } else if (sensors.rainIntensity > 60) {
        analysis.riskLevel += 4;  // Intensa
    } else if (sensors.rainIntensity > 40) {
        analysis.riskLevel += 2;  // Moderada
    } else if (sensors.rainIntensity > 20) {
        analysis.riskLevel += 1;  // Leve
    }
    
    // 🌡️ FATOR TEMPERATURA (10% do peso)
    if (sensors.temperature > TEMP_EXTREME_THRESHOLD) {
        analysis.riskLevel += 1;
        analysis.extremeWeatherAlert = true;
    }
    
    // 🚨 COMBINAÇÃO CRÍTICA: Solo seco + Chuva intensa
    if (sensors.soilMoisturePercent < SOIL_DRY_THRESHOLD && 
        sensors.rainIntensity > RAIN_HEAVY_THRESHOLD) {
        analysis.riskLevel += 2;  // Bônus crítico
    }
    
    // Limitar risco máximo
    analysis.riskLevel = min(analysis.riskLevel, 10);
    
    // 📝 Definir descrições e ações
    if (analysis.riskLevel <= 2) {
        analysis.riskDescription = "Risco Baixo - Condições normais";
        analysis.recommendation = "Monitoramento de rotina";
        analysis.timeToFlood = 999;
    } else if (analysis.riskLevel <= 4) {
        analysis.riskDescription = "Risco Moderado - Atenção";
        analysis.recommendation = "Intensificar monitoramento";
        analysis.timeToFlood = 180;
    } else if (analysis.riskLevel <= 6) {
        analysis.riskDescription = "Risco Alto - Preparação";
        analysis.recommendation = "Preparar sistemas de drenagem";
        analysis.timeToFlood = 90;
    } else if (analysis.riskLevel <= 8) {
        analysis.riskDescription = "Risco Muito Alto - Ação imediata";
        analysis.recommendation = "Alertar autoridades";
        analysis.timeToFlood = 45;
    } else {
        analysis.riskDescription = "RISCO CRÍTICO - EMERGÊNCIA";
        analysis.recommendation = "EVACUAR ÁREAS DE RISCO";
        analysis.timeToFlood = 20;
    }
    
    analysis.floodAlert = (analysis.riskLevel >= 7);
    
    // Debug da análise
    Serial.printf("🧮 Risco: %d/10 - %s\n", analysis.riskLevel, analysis.riskDescription.c_str());
    if (analysis.floodAlert) {
        Serial.println("🚨 ALERTA DE ENCHENTE ATIVO!");
    }
    
    return analysis;
}

//----------------------------------------------------------
// 📡 PUBLICAÇÃO DOS DADOS VIA MQTT

void publishSensorData() {
    // Limpar JSON
    sensorData.clear();
    
    // 🏷️ IDENTIFICAÇÃO WATERWISE
    sensorData["system"] = "WaterWise";
    sensorData["farmId"] = FARM_ID;
    sensorData["teamName"] = TEAM_NAME;
    sensorData["location"] = LOCATION;
    sensorData["timestamp"] = sensors.timestamp;
    sensorData["ip"] = WiFi.localIP().toString();
    sensorData["mac"] = WiFi.macAddress();
    
    // 📊 DADOS DOS SENSORES
    JsonObject sensorObj = sensorData["sensors"].to<JsonObject>();
    sensorObj["temperature"] = sensors.temperature;
    sensorObj["airHumidity"] = sensors.airHumidity;
    
    // Solo
    JsonObject soilObj = sensorObj["soil"].to<JsonObject>();
    soilObj["raw"] = sensors.soilMoistureRaw;
    soilObj["percent"] = sensors.soilMoisturePercent;
    soilObj["status"] = sensors.soilStatus;
    
    // Chuva
    JsonObject rainObj = sensorObj["rain"].to<JsonObject>();
    rainObj["raw"] = sensors.rainLevelRaw;
    rainObj["intensity"] = sensors.rainIntensity;
    rainObj["status"] = sensors.rainStatus;
    
    // 🧮 ANÁLISE DE RISCO
    FloodRiskAnalysis risk = analyzeFloodRisk();
    JsonObject analysisObj = sensorData["analysis"].to<JsonObject>();
    analysisObj["riskLevel"] = risk.riskLevel;
    analysisObj["riskDescription"] = risk.riskDescription;
    analysisObj["recommendation"] = risk.recommendation;
    analysisObj["timeToFlood"] = risk.timeToFlood;
    
    // 🚨 ALERTAS
    JsonObject alertsObj = sensorData["alerts"].to<JsonObject>();
    alertsObj["floodAlert"] = risk.floodAlert;
    alertsObj["droughtAlert"] = risk.droughtAlert;
    alertsObj["extremeWeatherAlert"] = risk.extremeWeatherAlert;
    
    // 📊 MÉTRICAS CALCULADAS
    JsonObject metricsObj = sensorData["metrics"].to<JsonObject>();
    metricsObj["absorptionCapacity"] = 100 - sensors.soilMoisturePercent;
    metricsObj["runoffRisk"] = max(0.0f, sensors.rainIntensity - (sensors.soilMoisturePercent * 0.8f));
    
    // Serializar e publicar
    serializeJson(sensorData, jsonBuffer);
    
    if (client.publish(topic_sensors, jsonBuffer)) {
        Serial.println("📡 ✅ Dados publicados no MQTT");
    } else {
        Serial.println("📡 ❌ Falha na publicação MQTT");
    }
    
    // Mostrar JSON no Serial
    Serial.println("📄 JSON Enviado:");
    Serial.println(jsonBuffer);
}

//----------------------------------------------------------
// 🚨 PUBLICAÇÃO DE ALERTAS DE ENCHENTE

void publishFloodAlert() {
    FloodRiskAnalysis risk = analyzeFloodRisk();
    
    if (!risk.floodAlert) return; // Só publica se houver alerta
    
    alertData.clear();
    
    alertData["alertType"] = "FLOOD_WARNING";
    alertData["severity"] = "HIGH";
    alertData["farmId"] = FARM_ID;
    alertData["location"] = LOCATION;
    alertData["timestamp"] = millis();
    alertData["riskLevel"] = risk.riskLevel;
    alertData["message"] = "🚨 RISCO DE ENCHENTE - " + risk.riskDescription;
    alertData["recommendation"] = risk.recommendation;
    alertData["estimatedFloodTime"] = risk.timeToFlood;
    
    // Dados críticos
    JsonObject criticalData = alertData["criticalData"].to<JsonObject>();
    criticalData["soilMoisture"] = sensors.soilMoisturePercent;
    criticalData["rainIntensity"] = sensors.rainIntensity;
    criticalData["temperature"] = sensors.temperature;
    
    serializeJson(alertData, jsonBuffer);
    
    if (client.publish(topic_alerts, jsonBuffer)) {
        Serial.println("🚨 ✅ ALERTA DE ENCHENTE ENVIADO!");
    } else {
        Serial.println("🚨 ❌ Falha no envio do alerta");
    }
}

//----------------------------------------------------------
// 💡 FEEDBACK VISUAL LED

void waterWiseStatusLED() {
    FloodRiskAnalysis risk = analyzeFloodRisk();
    
    if (risk.riskLevel >= 8) {
        // CRÍTICO - Piscadas rápidas
        for (int i = 0; i < 6; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(150);
            digitalWrite(LED_BUILTIN, LOW);
            delay(150);
        }
    } else if (risk.riskLevel >= 6) {
        // ALTO - Piscadas médias
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(400);
            digitalWrite(LED_BUILTIN, LOW);
            delay(400);
        }
    } else if (risk.riskLevel >= 4) {
        // MODERADO - Piscada lenta
        digitalWrite(LED_BUILTIN, HIGH);
        delay(800);
        digitalWrite(LED_BUILTIN, LOW);
        delay(200);
    } else {
        // NORMAL - Piscada rápida única
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
    }
}

//----------------------------------------------------------
// 🔄 CONFIGURAÇÃO INICIAL

void setup() {
    // Configurar Serial
    Serial.begin(115200);
    delay(2000);
    
    // Configurar pinos
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(RAIN_SENSOR_PIN, INPUT);
    
    // Inicializar DHT22
    dht.begin();
    
    // Conectar WiFi
    setup_wifi();
    
    // Configurar MQTT
    client.setServer(mqtt_server, mqtt_port);
    
    // Primeira leitura
    Serial.println("📡 Realizando primeira leitura dos sensores...");
    readAllSensors();
    
    Serial.println("\n🚀 === WATERWISE SISTEMA ONLINE ===");
    Serial.println("Monitoramento ativo - Intervalo: 15 segundos");
    Serial.println("==================================================\n");
}

//----------------------------------------------------------
// 🔄 LOOP PRINCIPAL

void loop() {
    // Manter conexão MQTT
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    
    // Leitura completa dos sensores
    readAllSensors();
    
    // Publicar dados via MQTT
    publishSensorData();
    
    // Verificar e enviar alertas
    publishFloodAlert();
    
    // Feedback visual LED
    waterWiseStatusLED();
    
    // Status resumido
    FloodRiskAnalysis risk = analyzeFloodRisk();
    Serial.printf("\n⏱️  Sistema WaterWise | Risco: %d/10 | Próxima leitura: 15s\n", risk.riskLevel);
    Serial.println("--------------------------------------------------");
    
    // Aguardar próxima leitura
    delay(15000);
}

//----------------------------------------------------------
// 📋 INSTRUÇÕES DE USO
/*
🌊 WATERWISE - INSTRUÇÕES DE CONFIGURAÇÃO

1. BIBLIOTECAS NECESSÁRIAS (Arduino IDE):
   - WiFi (nativa ESP32)
   - PubSubClient
   - ArduinoJson
   - DHT sensor library
   - Adafruit Unified Sensor

2. CONFIGURAÇÕES A ALTERAR:
   - Linha 25-26: ssid e password do seu WiFi
   - Linha 40-42: FARM_ID, TEAM_NAME, LOCATION

3. CONEXÕES FÍSICAS ESP32:
   - DHT22:    VCC→3.3V, GND→GND, DATA→GPIO12
   - Solo:     VCC→3.3V, GND→GND, SIG→GPIO34
   - Chuva:    VCC→3.3V, GND→GND, SIG→GPIO35
   - LED:      GPIO2 (onboard)

4. MONITORAMENTO:
   - Serial Monitor: 115200 baud
   - Node-RED: 172.208.54.189:1880
   - Tópicos MQTT: waterwise/sensors, waterwise/alerts

5. TESTE DO SISTEMA:
   - Solo seco (< 25%) + Chuva intensa (> 70%) = Alerta!
   - LED pisca conforme nível de risco
   - JSON enviado via MQTT a cada 15 segundos

🎯 READY FOR GLOBAL SOLUTION 2025! 🌊
*/