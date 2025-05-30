#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ========== CONFIGURAÇÕES DE REDE ==========
const char *ssid = "SUA_REDE_WIFI";
const char *password = "SUA_SENHA_WIFI";
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// ========== CONFIGURAÇÕES DOS SENSORES ==========
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define SOIL_MOISTURE_PIN A0
#define WATER_LEVEL_PIN A1

DHT dht(DHT_PIN, DHT_TYPE);

// ========== CONFIGURAÇÕES MQTT ==========
WiFiClient espClient;
PubSubClient client(espClient);

// ========== TÓPICOS MQTT ==========
const char *topic_umidade = "waterwise/sensor1/umidade";
const char *topic_temperatura = "waterwise/sensor1/temperatura";
const char *topic_solo = "waterwise/sensor1/solo";
const char *topic_status = "waterwise/sensor1/status";

// ========== VARIÁVEIS GLOBAIS ==========
unsigned long lastMsg = 0;
const long interval = 30000; // Envio a cada 30 segundos

void setup()
{
    Serial.begin(115200);

    // Inicializar sensores
    dht.begin();
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(WATER_LEVEL_PIN, INPUT);

    // Conectar WiFi
    setup_wifi();

    // Configurar MQTT
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

void setup_wifi()
{
    delay(10);
    Serial.println();
    Serial.print("Conectando a rede WiFi: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi conectado!");
    Serial.println("Endereço IP: ");
    Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Mensagem recebida [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

void reconnect()
{
    while (!client.connected())
    {
        Serial.print("Tentando conexão MQTT...");

        String clientId = "WaterWise_Sensor1_";
        clientId += String(random(0xffff), HEX);

        if (client.connect(clientId.c_str()))
        {
            Serial.println("Conectado!");

            // Publicar status de conexão
            StaticJsonDocument<200> statusDoc;
            statusDoc["sensor_id"] = "SENSOR_001";
            statusDoc["status"] = "online";
            statusDoc["timestamp"] = millis();
            statusDoc["location"] = "Propriedade Rural Mairiporã";

            char statusBuffer[256];
            serializeJson(statusDoc, statusBuffer);
            client.publish(topic_status, statusBuffer);
        }
        else
        {
            Serial.print("Falha, rc=");
            Serial.print(client.state());
            Serial.println(" tentando novamente em 5 segundos");
            delay(5000);
        }
    }
}

void loop()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    unsigned long now = millis();
    if (now - lastMsg > interval)
    {
        lastMsg = now;

        // ========== LEITURA DOS SENSORES ==========
        float humidity = dht.readHumidity();
        float temperature = dht.readTemperature();
        int soilMoisture = analogRead(SOIL_MOISTURE_PIN);
        int waterLevel = analogRead(WATER_LEVEL_PIN);

        // Converter valores para percentuais
        float soilMoisturePercent = map(soilMoisture, 0, 4095, 0, 100);
        float waterLevelPercent = map(waterLevel, 0, 4095, 0, 100);

        // ========== VERIFICAÇÃO DE DADOS VÁLIDOS ==========
        if (isnan(humidity) || isnan(temperature))
        {
            Serial.println("Falha na leitura do sensor DHT!");
            return;
        }

        // ========== CRIAÇÃO DO JSON PRINCIPAL ==========
        StaticJsonDocument<500> doc;
        doc["sensor_id"] = "SENSOR_001";
        doc["timestamp"] = now;
        doc["location"]["latitude"] = -23.3149;
        doc["location"]["longitude"] = -46.5873;
        doc["location"]["nome"] = "Fazenda São João - Mairiporã";

        // Dados ambientais
        doc["dados"]["temperatura"] = temperature;
        doc["dados"]["umidade_ar"] = humidity;
        doc["dados"]["umidade_solo"] = soilMoisturePercent;
        doc["dados"]["nivel_agua"] = waterLevelPercent;

        // Status do solo para prevenção de enchentes
        String statusSolo;
        if (soilMoisturePercent < 30)
        {
            statusSolo = "CRITICO_BAIXO"; // Solo muito seco - risco de enchente
        }
        else if (soilMoisturePercent < 60)
        {
            statusSolo = "MODERADO";
        }
        else if (soilMoisturePercent < 85)
        {
            statusSolo = "IDEAL";
        }
        else
        {
            statusSolo = "SATURADO"; // Solo saturado - risco iminente
        }

        doc["alertas"]["status_solo"] = statusSolo;
        doc["alertas"]["risco_enchente"] = (soilMoisturePercent < 30 || soilMoisturePercent > 85);

        // ========== PUBLICAÇÃO MQTT ==========
        char buffer[800];
        serializeJson(doc, buffer);

        // Publicar dados completos
        client.publish("waterwise/sensor1/dados_completos", buffer);

        // Publicar dados individuais para dashboards específicos
        client.publish(topic_temperatura, String(temperature).c_str());
        client.publish(topic_umidade, String(humidity).c_str());
        client.publish(topic_solo, String(soilMoisturePercent).c_str());

        // ========== LOG SERIAL ==========
        Serial.println("=== WATERWISE SENSOR 001 ===");
        Serial.println("Temperatura: " + String(temperature) + "°C");
        Serial.println("Umidade Ar: " + String(humidity) + "%");
        Serial.println("Umidade Solo: " + String(soilMoisturePercent) + "%");
        Serial.println("Nível Água: " + String(waterLevelPercent) + "%");
        Serial.println("Status Solo: " + statusSolo);
        Serial.println("Dados enviados via MQTT!");
        Serial.println("============================");

        // ========== ALERTA DE EMERGÊNCIA ==========
        if (statusSolo == "CRITICO_BAIXO" || statusSolo == "SATURADO")
        {
            StaticJsonDocument<300> alertDoc;
            alertDoc["sensor_id"] = "SENSOR_001";
            alertDoc["tipo_alerta"] = "RISCO_ENCHENTE";
            alertDoc["nivel"] = "ALTO";
            alertDoc["timestamp"] = now;
            alertDoc["mensagem"] = "Solo em condição crítica para absorção de água";
            alertDoc["umidade_solo"] = soilMoisturePercent;

            char alertBuffer[400];
            serializeJson(alertDoc, alertBuffer);
            client.publish("waterwise/alertas/emergencia", alertBuffer);

            Serial.println("⚠️  ALERTA DE EMERGÊNCIA ENVIADO!");
        }
    }
}