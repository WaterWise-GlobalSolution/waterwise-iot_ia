/********************************************************************
 * WaterWise - Sistema Inteligente de Prevenção a Enchentes
 * Global Solution 2025 - FIAP
 * 
 * Descrição: Sistema IoT que monitora propriedades rurais em tempo 
 * real para prevenir enchentes urbanas através de dados de:
 * - Umidade do solo
 * - Temperatura ambiente  
 * - Precipitação pluviométrica
 ********************************************************************/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>

//----------------------------------------------------------
// Definições WaterWise

#define LED_BUILTIN 2
#define DHT_PIN 12
#define DHT_TYPE DHT22
#define SOIL_MOISTURE_PIN 34    // Sensor umidade do solo
#define RAIN_SENSOR_PIN 35      // Sensor de chuva

// Identificação WaterWise
const char* FARM_ID = "FARM_001_WaterWise";
const char* LOCATION = "Zona_Rural_SP";

// Configurações de rede
const char* SSID = "Wokwi-GUEST";
const char* PASSWORD = "";

// MQTT Broker
const char* BROKER_MQTT = "172.208.54.189";
const int BROKER_PORT = 1883;
const char* MQTT_USER = "gs2025";
const char* MQTT_PASSWORD = "q1w2e3r4";

// Tópicos MQTT WaterWise
#define TOPIC_SENSORS "waterwise/sensors/data"
#define TOPIC_ALERTS "waterwise/alerts/flood"
#define TOPIC_STATUS "waterwise/farm/status"

//----------------------------------------------------------
// Objetos globais

WiFiClient espClient;
PubSubClient mqtt(espClient);
DHT dht(DHT_PIN, DHT_TYPE);
JsonDocument sensorData;
JsonDocument alertData;
char jsonBuffer[512];

//----------------------------------------------------------
// Variáveis de sensores

struct SensorReadings {
    float temperature;
    float humidity;
    int soilMoisture;      // 0-4095 (0=seco, 4095=úmido)
    int rainLevel;         // 0-4095 (0=sem chuva, 4095=chuva intensa)
    float soilMoisturePercent;
    float rainIntensity;
    unsigned long timestamp;
};

SensorReadings readings;

//----------------------------------------------------------
// Configurações de alerta

const int SOIL_DRY_THRESHOLD = 1000;     // Solo seco < 1000
const int RAIN_ALERT_THRESHOLD = 2000;   // Chuva forte > 2000
const float TEMP_ALERT_MAX = 35.0;       // Temperatura crítica

//----------------------------------------------------------
// Funções de conectividade

void initWiFi() {
    WiFi.begin(SSID, PASSWORD);
    Serial.print("Conectando WiFi WaterWise");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    
    Serial.println("\n🌊 WaterWise WiFi Conectado!");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Farm ID: %s\n", FARM_ID);
}

void initMQTT() {
    mqtt.setServer(BROKER_MQTT, BROKER_PORT);
    
    while (!mqtt.connected()) {
        Serial.println("Conectando MQTT WaterWise...");
        
        if (mqtt.connect(FARM_ID, MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("✅ MQTT WaterWise Conectado!");
        } else {
            Serial.printf("❌ Falha MQTT: %d\n", mqtt.state());
            delay(2000);
        }
    }
}

//----------------------------------------------------------
// Leitura de sensores

void readSensors() {
    // Timestamp
    readings.timestamp = millis();
    
    // Temperatura e umidade do ar
    readings.temperature = dht.readTemperature();
    readings.humidity = dht.readHumidity();
    
    // Umidade do solo (0-4095)
    readings.soilMoisture = analogRead(SOIL_MOISTURE_PIN);
    readings.soilMoisturePercent = map(readings.soilMoisture, 0, 4095, 0, 100);
    
    // Sensor de chuva (0-4095)
    readings.rainLevel = analogRead(RAIN_SENSOR_PIN);
    readings.rainIntensity = map(readings.rainLevel, 0, 4095, 0, 100);
    
    // Debug no Serial
    Serial.println("\n📊 === LEITURA WATERWISE ===");
    Serial.printf("🌡️  Temperatura: %.1f°C\n", readings.temperature);
    Serial.printf("💨 Umidade Ar: %.1f%%\n", readings.humidity);
    Serial.printf("🌱 Umidade Solo: %d (%.1f%%)\n", readings.soilMoisture, readings.soilMoisturePercent);
    Serial.printf("🌧️  Chuva: %d (%.1f%%)\n", readings.rainLevel, readings.rainIntensity);
}

//----------------------------------------------------------
// Análise de riscos WaterWise

struct RiskAnalysis {
    bool floodRisk;
    bool droughtRisk;
    bool extremeWeather;
    int riskLevel;        // 0-10
    String riskDescription;
};

RiskAnalysis analyzeRisk() {
    RiskAnalysis risk;
    risk.floodRisk = false;
    risk.droughtRisk = false;
    risk.extremeWeather = false;
    risk.riskLevel = 0;
    
    // Análise de seca
    if (readings.soilMoisturePercent < 20) {
        risk.droughtRisk = true;
        risk.riskLevel += 3;
    }
    
    // Análise de enchente
    if (readings.rainIntensity > 70 && readings.soilMoisturePercent < 30) {
        risk.floodRisk = true;
        risk.riskLevel += 5;
        risk.riskDescription = "ALTO RISCO: Solo seco + chuva intensa = enchente!";
    }
    
    // Temperatura extrema
    if (readings.temperature > TEMP_ALERT_MAX) {
        risk.extremeWeather = true;
        risk.riskLevel += 2;
    }
    
    // Descrições de risco
    if (risk.riskLevel == 0) {
        risk.riskDescription = "Condições normais";
    } else if (risk.riskLevel <= 3) {
        risk.riskDescription = "Risco baixo - monitoramento";
    } else if (risk.riskLevel <= 6) {
        risk.riskDescription = "Risco médio - atenção";
    } else {
        risk.riskDescription = "Risco alto - ação necessária";
    }
    
    return risk;
}

//----------------------------------------------------------
// Publicação MQTT

void publishSensorData() {
    sensorData.clear();
    
    // Identificação
    sensorData["farmId"] = FARM_ID;
    sensorData["location"] = LOCATION;
    sensorData["timestamp"] = readings.timestamp;
    sensorData["ip"] = WiFi.localIP().toString();
    
    // Dados dos sensores
    sensorData["temperature"] = readings.temperature;
    sensorData["airHumidity"] = readings.humidity;
    sensorData["soilMoisture"] = readings.soilMoisture;
    sensorData["soilMoisturePercent"] = readings.soilMoisturePercent;
    sensorData["rainLevel"] = readings.rainLevel;
    sensorData["rainIntensity"] = readings.rainIntensity;
    
    // Análise de risco
    RiskAnalysis risk = analyzeRisk();
    sensorData["riskLevel"] = risk.riskLevel;
    sensorData["floodRisk"] = risk.floodRisk;
    sensorData["droughtRisk"] = risk.droughtRisk;
    sensorData["riskDescription"] = risk.riskDescription;
    
    // Serializar e publicar
    serializeJson(sensorData, jsonBuffer);
    mqtt.publish(TOPIC_SENSORS, jsonBuffer);
    
    // Log
    Serial.println("📡 Dados enviados via MQTT");
    Serial.println(jsonBuffer);
}

void publishAlert() {
    RiskAnalysis risk = analyzeRisk();
    
    if (risk.riskLevel >= 5) {
        alertData.clear();
        
        alertData["alertType"] = "FLOOD_RISK";
        alertData["farmId"] = FARM_ID;
        alertData["location"] = LOCATION;
        alertData["severity"] = risk.riskLevel;
        alertData["message"] = risk.riskDescription;
        alertData["timestamp"] = millis();
        
        // Dados críticos
        alertData["criticalData"]["soilMoisture"] = readings.soilMoisturePercent;
        alertData["criticalData"]["rainIntensity"] = readings.rainIntensity;
        alertData["criticalData"]["temperature"] = readings.temperature;
        
        serializeJson(alertData, jsonBuffer);
        mqtt.publish(TOPIC_ALERTS, jsonBuffer);
        
        Serial.println("🚨 ALERTA DE ENCHENTE ENVIADO!");
        Serial.println(jsonBuffer);
    }
}

//----------------------------------------------------------
// Feedback visual

void statusLED() {
    RiskAnalysis risk = analyzeRisk();
    
    if (risk.riskLevel >= 7) {
        // Piscada rápida - risco alto
        for (int i = 0; i < 6; i++) {
            digitalWrite(LED_BUILTIN, HIGH);
            delay(100);
            digitalWrite(LED_BUILTIN, LOW);
            delay(100);
        }
    } else if (risk.riskLevel >= 4) {
        // Piscada média - risco médio
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
    } else {
        // Piscada lenta - normal
        digitalWrite(LED_BUILTIN, HIGH);
        delay(200);
        digitalWrite(LED_BUILTIN, LOW);
    }
}

//----------------------------------------------------------
// Setup e Loop

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    
    Serial.println("🌊 === WATERWISE INICIANDO ===");
    Serial.println("Sistema Inteligente de Prevenção a Enchentes");
    
    dht.begin();
    initWiFi();
    initMQTT();
    
    Serial.println("✅ WaterWise Sistema Online!");
}

void loop() {
    // Manter conexões
    if (!mqtt.connected()) {
        initMQTT();
    }
    mqtt.loop();
    
    // Leitura dos sensores
    readSensors();
    
    // Publicar dados
    publishSensorData();
    
    // Verificar alertas
    publishAlert();
    
    // Feedback visual
    statusLED();
    
    // Intervalo de 15 segundos
    delay(15000);
}