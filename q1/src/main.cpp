/*
 * WaterWise - Sistema Inteligente de Prevenção a Enchentes
 * Global Solution 2025 - FIAP
 * Disciplina: DISRUPTIVE ARCHITECTURES: IOT, IOB & GENERATIVE IA
 * 
 * SISTEMA IOT COMPLETO COM 5 DISPOSITIVOS:
 * 1. DHT22 - Sensor Temperatura/Umidade
 * 2. Sensor Umidade do Solo
 * 3. Sensor de Precipitação
 * 4. Buzzer - Atuador de Alerta Sonoro
 * 5. LED Status - Atuador Visual
 * 
 * AUTORES: [INSERIR NOMES E RMs DO SEU GRUPO AQUI]
 * DATA: Junho 2025
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

//----------------------------------------------------------
// 🔧 DEFINIÇÕES DE PINOS - 5 DISPOSITIVOS IOT

#define DHT_PIN 12             // Dispositivo 1: DHT22 (Sensor)
#define DHT_TYPE DHT22
#define SOIL_MOISTURE_PIN 34   // Dispositivo 2: Sensor Umidade Solo (Sensor)
#define RAIN_SENSOR_PIN 35     // Dispositivo 3: Sensor Chuva (Sensor)
#define BUZZER_PIN 25          // Dispositivo 4: Buzzer Alerta (Atuador)
#define STATUS_LED_PIN 26      // Dispositivo 5: LED Status (Atuador)
#define LED_BUILTIN 2          // LED interno ESP32

//----------------------------------------------------------
// 🌐 CONFIGURAÇÕES DE REDE (ALTERE AQUI!)

const char* SSID = "Wokwi-GUEST";        // Para Wokwi
const char* PASSWORD = "";               // Para Wokwi
// Para WiFi real, use:
// const char* SSID = "SUA_REDE_WIFI";
// const char* PASSWORD = "SUA_SENHA_WIFI";

//----------------------------------------------------------
// 📡 CONFIGURAÇÕES MQTT FIAP (NÃO ALTERAR)

const char* MQTT_BROKER = "172.208.54.189";
const int MQTT_PORT = 1883;
const char* MQTT_USER = "gs2025";
const char* MQTT_PASSWORD = "q1w2e3r4";

// Tópicos MQTT WaterWise
const char* TOPIC_SENSORS = "waterwise/sensors/data";
const char* TOPIC_ALERTS = "waterwise/alerts/flood";
const char* TOPIC_STATUS = "waterwise/status/system";

//----------------------------------------------------------
// 🏷️ IDENTIFICADORES WATERWISE (ALTERE AQUI!)

const char* FARM_ID = "FARM_WaterWise_2025";
const char* TEAM_NAME = "GRUPO_WATERWISE";    // ALTERAR PARA SEU GRUPO
const char* LOCATION = "SP_Zona_Rural";
const char* PROJECT_VERSION = "WaterWise-v2.0-IoT5D";

//----------------------------------------------------------
// 📊 OBJETOS GLOBAIS

WiFiClient espClient;
PubSubClient mqtt(espClient);
DHT dht(DHT_PIN, DHT_TYPE);
JsonDocument sensorData;
JsonDocument alertData;
char jsonBuffer[1024];  // Buffer maior para mais dados

//----------------------------------------------------------
// 📊 ESTRUTURAS DE DADOS

struct WaterWiseSensors {
    // Dispositivo 1: DHT22
    float temperature;
    float airHumidity;
    bool dhtStatus;
    
    // Dispositivo 2: Solo
    int soilMoistureRaw;
    float soilMoisturePercent;
    String soilStatus;
    
    // Dispositivo 3: Chuva
    int rainLevelRaw;
    float rainIntensity;
    String rainStatus;
    
    // Dispositivo 4: Buzzer (Atuador)
    bool buzzerActive;
    int alertLevel;
    
    // Dispositivo 5: LED Status (Atuador)
    bool ledState;
    String ledColor;
    int blinkPattern;
    
    // Timestamp
    unsigned long timestamp;
};

struct FloodRiskAnalysis {
    int riskLevel;              // 0-10
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
const int CRITICAL_RISK_LEVEL = 7;

//----------------------------------------------------------
// 🧮 ALGORITMO ANÁLISE DE RISCO WATERWISE (DECLARAÇÃO ANTECIPADA)

FloodRiskAnalysis analyzeFloodRisk() {
    FloodRiskAnalysis analysis;
    analysis.riskLevel = 0;
    analysis.floodAlert = false;
    analysis.droughtAlert = false;
    analysis.extremeWeatherAlert = false;
    
    // FATOR SOLO (40% do peso)
    if (sensors.soilMoisturePercent < 15) {
        analysis.riskLevel += 4;
        analysis.droughtAlert = true;
    } else if (sensors.soilMoisturePercent < 30) {
        analysis.riskLevel += 3;
    } else if (sensors.soilMoisturePercent < 50) {
        analysis.riskLevel += 1;
    }
    
    // FATOR CHUVA (50% do peso)
    if (sensors.rainIntensity > 80) {
        analysis.riskLevel += 5;
    } else if (sensors.rainIntensity > 60) {
        analysis.riskLevel += 4;
    } else if (sensors.rainIntensity > 40) {
        analysis.riskLevel += 2;
    } else if (sensors.rainIntensity > 20) {
        analysis.riskLevel += 1;
    }
    
    // FATOR TEMPERATURA (10% do peso)
    if (sensors.temperature > TEMP_EXTREME_THRESHOLD) {
        analysis.riskLevel += 1;
        analysis.extremeWeatherAlert = true;
    }
    
    // COMBINAÇÃO CRÍTICA: Solo seco + Chuva intensa
    if (sensors.soilMoisturePercent < SOIL_DRY_THRESHOLD && 
        sensors.rainIntensity > RAIN_HEAVY_THRESHOLD) {
        analysis.riskLevel += 2;  // Bônus crítico
    }
    
    // Limitar risco máximo
    analysis.riskLevel = min(analysis.riskLevel, 10);
    
    // Calcular métricas adicionais
    analysis.absorptionCapacity = 100 - sensors.soilMoisturePercent;
    analysis.runoffRisk = max(0.0f, sensors.rainIntensity - (sensors.soilMoisturePercent * 0.8f));
    
    // Definir descrições
    if (analysis.riskLevel <= 2) {
        analysis.riskDescription = "Baixo - Condições normais";
        analysis.recommendation = "Monitoramento rotineiro";
    } else if (analysis.riskLevel <= 4) {
        analysis.riskDescription = "Moderado - Atenção";
        analysis.recommendation = "Intensificar monitoramento";
    } else if (analysis.riskLevel <= 6) {
        analysis.riskDescription = "Alto - Preparação";
        analysis.recommendation = "Preparar drenagem";
    } else if (analysis.riskLevel <= 8) {
        analysis.riskDescription = "Muito Alto - Ação";
        analysis.recommendation = "Alertar autoridades";
    } else {
        analysis.riskDescription = "CRÍTICO - EMERGÊNCIA";
        analysis.recommendation = "EVACUAR ÁREAS DE RISCO";
    }
    
    analysis.floodAlert = (analysis.riskLevel >= 7);
    
    return analysis;
}

//----------------------------------------------------------
// 💡 CONTROLE DO LED STATUS (DISPOSITIVO 5)

void updateStatusLED(FloodRiskAnalysis risk) {
    if (risk.riskLevel >= 8) {
        // CRÍTICO - Vermelho piscando rápido
        sensors.ledColor = "red";
        sensors.blinkPattern = 1; // Rápido
        sensors.ledState = true;
        for (int i = 0; i < 6; i++) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(150);
            digitalWrite(STATUS_LED_PIN, LOW);
            delay(150);
        }
    } else if (risk.riskLevel >= 6) {
        // ALTO - Amarelo piscando médio
        sensors.ledColor = "yellow";
        sensors.blinkPattern = 2; // Médio
        sensors.ledState = true;
        for (int i = 0; i < 3; i++) {
            digitalWrite(STATUS_LED_PIN, HIGH);
            delay(400);
            digitalWrite(STATUS_LED_PIN, LOW);
            delay(400);
        }
    } else if (risk.riskLevel >= 3) {
        // MODERADO - Verde piscando lento
        sensors.ledColor = "green";
        sensors.blinkPattern = 3; // Lento
        sensors.ledState = true;
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(800);
        digitalWrite(STATUS_LED_PIN, LOW);
        delay(200);
    } else {
        // NORMAL - Verde fixo
        sensors.ledColor = "green";
        sensors.blinkPattern = 0; // Fixo
        sensors.ledState = true;
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(100);
        digitalWrite(STATUS_LED_PIN, LOW);
    }
}

//----------------------------------------------------------
// 🌐 INICIALIZAÇÃO WIFI

void initWiFi() {
    Serial.println("\n==================================================");
    Serial.println("🌊 WATERWISE - SISTEMA IOT 5 DISPOSITIVOS");
    Serial.println("Global Solution 2025 - FIAP");
    Serial.println("==================================================");
    Serial.printf("Farm ID: %s\n", FARM_ID);
    Serial.printf("Equipe: %s\n", TEAM_NAME);
    Serial.printf("Versão: %s\n", PROJECT_VERSION);
    Serial.println("==================================================");
    
    WiFi.begin(SSID, PASSWORD);
    Serial.printf("Conectando ao WiFi: %s", SSID);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    
    Serial.println("\n✅ WiFi Conectado!");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
}

//----------------------------------------------------------
// 📡 INICIALIZAÇÃO MQTT

void initMQTT() {
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    
    while (!mqtt.connected()) {
        Serial.println("Conectando MQTT Broker FIAP...");
        
        if (mqtt.connect(FARM_ID, MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("✅ MQTT Conectado!");
            
            // Publicar status online
            JsonDocument statusDoc;
            statusDoc["system"] = "WaterWise";
            statusDoc["farmId"] = FARM_ID;
            statusDoc["status"] = "online";
            statusDoc["devices"] = 5;
            statusDoc["timestamp"] = millis();
            
            char statusBuffer[256];
            serializeJson(statusDoc, statusBuffer);
            mqtt.publish(TOPIC_STATUS, statusBuffer);
            
        } else {
            Serial.printf("❌ Falha MQTT: %d\n", mqtt.state());
            delay(2000);
        }
    }
}

//----------------------------------------------------------
// 📊 LEITURA DOS 5 DISPOSITIVOS IOT

void readAllDevices() {
    sensors.timestamp = millis();
    
    // 🌡️ DISPOSITIVO 1: DHT22 (Sensor)
    sensors.temperature = dht.readTemperature();
    sensors.airHumidity = dht.readHumidity();
    sensors.dhtStatus = !isnan(sensors.temperature) && !isnan(sensors.airHumidity);
    
    if (!sensors.dhtStatus) {
        sensors.temperature = 25.0;  // Valor padrão
        sensors.airHumidity = 60.0;  // Valor padrão
        Serial.println("⚠️  DHT22 com erro - usando valores padrão");
    }
    
    // 🌱 DISPOSITIVO 2: Sensor Umidade Solo (Sensor)
    sensors.soilMoistureRaw = analogRead(SOIL_MOISTURE_PIN);
    sensors.soilMoisturePercent = map(sensors.soilMoistureRaw, 0, 4095, 0, 100);
    
    if (sensors.soilMoisturePercent < 20) {
        sensors.soilStatus = "Crítico";
    } else if (sensors.soilMoisturePercent < 40) {
        sensors.soilStatus = "Seco";
    } else if (sensors.soilMoisturePercent < 70) {
        sensors.soilStatus = "Normal";
    } else {
        sensors.soilStatus = "Saturado";
    }
    
    // 🌧️ DISPOSITIVO 3: Sensor de Chuva (Sensor)
    sensors.rainLevelRaw = analogRead(RAIN_SENSOR_PIN);
    sensors.rainIntensity = map(sensors.rainLevelRaw, 0, 4095, 0, 100);
    
    if (sensors.rainIntensity < 10) {
        sensors.rainStatus = "Sem chuva";
    } else if (sensors.rainIntensity < 30) {
        sensors.rainStatus = "Leve";
    } else if (sensors.rainIntensity < 70) {
        sensors.rainStatus = "Moderada";
    } else {
        sensors.rainStatus = "Intensa";
    }
    
    // 🔊 DISPOSITIVO 4: Buzzer (Atuador) - Controle baseado em risco
    FloodRiskAnalysis risk = analyzeFloodRisk();
    sensors.buzzerActive = (risk.riskLevel >= CRITICAL_RISK_LEVEL);
    sensors.alertLevel = risk.riskLevel;
    
    // Ativar buzzer se necessário
    if (sensors.buzzerActive) {
        // Padrão de bip baseado no nível de risco
        int beepCount = min(risk.riskLevel, 5);
        for (int i = 0; i < beepCount; i++) {
            digitalWrite(BUZZER_PIN, HIGH);
            delay(200);
            digitalWrite(BUZZER_PIN, LOW);
            delay(200);
        }
    }
    
    // 💡 DISPOSITIVO 5: LED Status (Atuador) - Visual do estado do sistema
    updateStatusLED(risk);
    
    // 🖥️ Debug das leituras
    Serial.println("\n📊 === LEITURA 5 DISPOSITIVOS WATERWISE ===");
    Serial.printf("🌡️  DHT22: %.1f°C, %.1f%% - %s\n", 
                  sensors.temperature, sensors.airHumidity, 
                  sensors.dhtStatus ? "OK" : "ERRO");
    Serial.printf("🌱 Solo: %d raw (%.1f%%) - %s\n", 
                  sensors.soilMoistureRaw, sensors.soilMoisturePercent, 
                  sensors.soilStatus.c_str());
    Serial.printf("🌧️  Chuva: %d raw (%.1f%%) - %s\n", 
                  sensors.rainLevelRaw, sensors.rainIntensity, 
                  sensors.rainStatus.c_str());
    Serial.printf("🔊 Buzzer: %s (Nível %d)\n", 
                  sensors.buzzerActive ? "ATIVO" : "Inativo", sensors.alertLevel);
    Serial.printf("💡 LED: %s (%s)\n", 
                  sensors.ledState ? "ON" : "OFF", sensors.ledColor.c_str());
}

//----------------------------------------------------------
// 📡 PUBLICAÇÃO MQTT - DADOS DOS 5 DISPOSITIVOS

void publishSensorData() {
    sensorData.clear();
    
    // IDENTIFICAÇÃO WATERWISE
    sensorData["system"] = PROJECT_VERSION;
    sensorData["farmId"] = FARM_ID;
    sensorData["teamName"] = TEAM_NAME;
    sensorData["location"] = LOCATION;
    sensorData["timestamp"] = sensors.timestamp;
    sensorData["ip"] = WiFi.localIP().toString();
    sensorData["mac"] = WiFi.macAddress();
    
    // DISPOSITIVOS IOT (5 dispositivos conforme exigido)
    JsonObject devices = sensorData["devices"].to<JsonObject>();
    
    // DISPOSITIVO 1: DHT22 (Sensor)
    JsonObject dht22 = devices["dht22"].to<JsonObject>();
    dht22["type"] = "sensor";
    dht22["description"] = "Temperatura e Umidade Ambiente";
    dht22["temperature"] = sensors.temperature;
    dht22["humidity"] = sensors.airHumidity;
    dht22["status"] = sensors.dhtStatus ? "active" : "error";
    dht22["pin"] = DHT_PIN;
    
    // DISPOSITIVO 2: Sensor Umidade Solo (Sensor)
    JsonObject soilSensor = devices["soilSensor"].to<JsonObject>();
    soilSensor["type"] = "sensor";
    soilSensor["description"] = "Umidade do Solo";
    soilSensor["moisture_raw"] = sensors.soilMoistureRaw;
    soilSensor["moisture_percent"] = sensors.soilMoisturePercent;
    soilSensor["status"] = sensors.soilStatus;
    soilSensor["pin"] = SOIL_MOISTURE_PIN;
    
    // DISPOSITIVO 3: Sensor Chuva (Sensor)
    JsonObject rainSensor = devices["rainSensor"].to<JsonObject>();
    rainSensor["type"] = "sensor";
    rainSensor["description"] = "Intensidade de Precipitação";
    rainSensor["rain_raw"] = sensors.rainLevelRaw;
    rainSensor["intensity_percent"] = sensors.rainIntensity;
    rainSensor["status"] = sensors.rainStatus;
    rainSensor["pin"] = RAIN_SENSOR_PIN;
    
    // DISPOSITIVO 4: Buzzer (Atuador)
    JsonObject buzzer = devices["buzzer"].to<JsonObject>();
    buzzer["type"] = "actuator";
    buzzer["description"] = "Alerta Sonoro de Emergência";
    buzzer["active"] = sensors.buzzerActive;
    buzzer["alert_level"] = sensors.alertLevel;
    buzzer["trigger_threshold"] = CRITICAL_RISK_LEVEL;
    buzzer["pin"] = BUZZER_PIN;
    
    // DISPOSITIVO 5: LED Status (Atuador)
    JsonObject statusLED = devices["statusLED"].to<JsonObject>();
    statusLED["type"] = "actuator";
    statusLED["description"] = "Indicador Visual de Status";
    statusLED["state"] = sensors.ledState;
    statusLED["color"] = sensors.ledColor;
    statusLED["blink_pattern"] = sensors.blinkPattern;
    statusLED["pin"] = STATUS_LED_PIN;
    
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
    analysis["absorption_capacity"] = risk.absorptionCapacity;
    analysis["runoff_risk"] = risk.runoffRisk;
    
    // PROTOCOLO DE COMUNICAÇÃO
    JsonObject protocol = sensorData["protocol"].to<JsonObject>();
    protocol["mqtt_topic"] = TOPIC_SENSORS;
    protocol["format"] = "JSON";
    protocol["broker"] = MQTT_BROKER;
    protocol["qos"] = 0;
    
    // SERIALIZAR E PUBLICAR
    serializeJson(sensorData, jsonBuffer);
    
    if (mqtt.publish(TOPIC_SENSORS, jsonBuffer)) {
        Serial.println("📡 ✅ Dados 5 dispositivos publicados no MQTT");
    } else {
        Serial.println("📡 ❌ Falha na publicação MQTT");
    }
    
    // Mostrar JSON compacto
    Serial.println("📄 JSON Enviado (5 Dispositivos IoT):");
    Serial.println(jsonBuffer);
}

//----------------------------------------------------------
// 🚨 PUBLICAÇÃO DE ALERTAS

void publishAlerts() {
    FloodRiskAnalysis risk = analyzeFloodRisk();
    
    if (!risk.floodAlert && !risk.droughtAlert && !risk.extremeWeatherAlert) {
        return; // Só publica se houver algum alerta
    }
    
    alertData.clear();
    
    alertData["alertSystem"] = "WaterWise-Alert-v2.0";
    alertData["farmId"] = FARM_ID;
    alertData["location"] = LOCATION;
    alertData["timestamp"] = millis();
    alertData["severity"] = risk.riskLevel >= 8 ? "CRITICAL" : "HIGH";
    
    // Tipos de alerta
    JsonArray alerts = alertData["active_alerts"].to<JsonArray>();
    if (risk.floodAlert) {
        JsonObject floodAlert = alerts.add<JsonObject>();
        floodAlert["type"] = "FLOOD_WARNING";
        floodAlert["message"] = "Risco de enchente detectado";
        floodAlert["trigger"] = "Solo seco + Chuva intensa";
    }
    if (risk.droughtAlert) {
        JsonObject droughtAlert = alerts.add<JsonObject>();
        droughtAlert["type"] = "DROUGHT_WARNING";
        droughtAlert["message"] = "Solo criticamente seco";
        droughtAlert["trigger"] = "Umidade solo < 15%";
    }
    if (risk.extremeWeatherAlert) {
        JsonObject weatherAlert = alerts.add<JsonObject>();
        weatherAlert["type"] = "EXTREME_WEATHER";
        weatherAlert["message"] = "Temperatura extrema detectada";
        weatherAlert["trigger"] = "Temperatura > 35°C";
    }
    
    // Dados críticos dos sensores
    JsonObject criticalData = alertData["sensor_data"].to<JsonObject>();
    criticalData["soil_moisture"] = sensors.soilMoisturePercent;
    criticalData["rain_intensity"] = sensors.rainIntensity;
    criticalData["temperature"] = sensors.temperature;
    criticalData["risk_level"] = risk.riskLevel;
    
    // Ações recomendadas
    alertData["recommendation"] = risk.recommendation;
    alertData["emergency_contact"] = "Defesa Civil: 199";
    
    serializeJson(alertData, jsonBuffer);
    
    if (mqtt.publish(TOPIC_ALERTS, jsonBuffer)) {
        Serial.println("🚨 ✅ ALERTA ENVIADO!");
    } else {
        Serial.println("🚨 ❌ Falha no envio do alerta");
    }
}

//----------------------------------------------------------
// 💡 FEEDBACK VISUAL LED INTERNO

void waterWiseStatusFeedback() {
    FloodRiskAnalysis risk = analyzeFloodRisk();
    
    // LED interno como feedback adicional
    if (risk.riskLevel >= 8) {
        // CRÍTICO - Piscadas rápidas
        for (int i = 0; i < 8; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
    } else if (risk.riskLevel >= 6) {
        // ALTO - Piscadas médias
        for (int i = 0; i < 4; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(300);
            digitalWrite(LED_BUILTIN, LOW);
            delay(300);
        }
    } else {
        // NORMAL - Piscada única
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
    }
}

//----------------------------------------------------------
// 🔄 CONFIGURAÇÃO INICIAL

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // Configurar pinos dos 5 dispositivos
    pinMode(LED_BUILTIN, OUTPUT);        // LED interno ESP32
    pinMode(BUZZER_PIN, OUTPUT);         // Dispositivo 4: Buzzer
    pinMode(STATUS_LED_PIN, OUTPUT);     // Dispositivo 5: LED Status
    pinMode(SOIL_MOISTURE_PIN, INPUT);   // Dispositivo 2: Solo
    pinMode(RAIN_SENSOR_PIN, INPUT);     // Dispositivo 3: Chuva
    
    // Inicializar DHT22 (Dispositivo 1)
    dht.begin();
    
    // Conectividade
    initWiFi();
    initMQTT();
    
    // Primeira leitura
    Serial.println("📡 Primeira leitura dos 5 dispositivos...");
    readAllDevices();
    
    Serial.println("\n🚀 === WATERWISE SISTEMA IOT ONLINE ===");
    Serial.println("5 Dispositivos: DHT22 + Solo + Chuva + Buzzer + LED");
    Serial.println("Protocolos: MQTT + JSON + HTTP");
    Serial.println("Intervalo: 15 segundos");
    Serial.println("==================================================\n");
}

//----------------------------------------------------------
// 🔄 LOOP PRINCIPAL

void loop() {
    // Manter conexões
    if (!mqtt.connected()) {
        initMQTT();
    }
    mqtt.loop();
    
    // Leitura completa dos 5 dispositivos
    readAllDevices();
    
    // Publicar dados via MQTT
    publishSensorData();
    
    // Verificar e enviar alertas
    publishAlerts();
    
    // Feedback visual adicional
    waterWiseStatusFeedback();
    
    // Status resumido
    FloodRiskAnalysis risk = analyzeFloodRisk();
    Serial.printf("\n⏱️  WaterWise | 5 Dispositivos | Risco: %d/10 | Próxima: 15s\n", 
                  risk.riskLevel);
    Serial.printf("📊 Solo: %.1f%% | Chuva: %.1f%% | Temp: %.1f°C\n",
                  sensors.soilMoisturePercent, sensors.rainIntensity, sensors.temperature);
    Serial.printf("🔊 Buzzer: %s | 💡 LED: %s (%s)\n",
                  sensors.buzzerActive ? "ON" : "OFF",
                  sensors.ledState ? "ON" : "OFF",
                  sensors.ledColor.c_str());
    Serial.println("--------------------------------------------------");
    
    // Aguardar próxima leitura (15 segundos)
    delay(15000);
}