#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ========== CONFIGURAÇÕES DE REDE ==========
const char* ssid = "SUA_REDE_WIFI";
const char* password = "SUA_SENHA_WIFI";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

// ========== CONFIGURAÇÕES DOS SENSORES ==========
#define RAIN_SENSOR_PIN 2     // Sensor de chuva digital
#define RAIN_ANALOG_PIN A0    // Sensor de chuva analógico
#define TIPPING_BUCKET_PIN 3  // Pluviômetro tipo báscula
#define LED_STATUS_PIN 13     // LED indicador

// ========== CONFIGURAÇÕES MQTT ==========
WiFiClient espClient;
PubSubClient client(espClient);

// ========== VARIÁVEIS GLOBAIS ==========
volatile int tipCount = 0;        // Contador de báscula
float rainVolume = 0.0;           // Volume de chuva acumulado
unsigned long lastTipTime = 0;    // Última báscula registrada
unsigned long lastMsg = 0;
const long interval = 10000;      // Envio a cada 10 segundos
const float mmPerTip = 0.2794;    // mm de chuva por báscula

// ========== INTERRUPÇÃO PARA PLUVIÔMETRO ==========
void IRAM_ATTR rainInterrupt() {
  unsigned long currentTime = millis();
  
  // Debounce - evitar múltiplas leituras
  if (currentTime - lastTipTime > 100) {
    tipCount++;
    lastTipTime = currentTime;
    digitalWrite(LED_STATUS_PIN, !digitalRead(LED_STATUS_PIN)); // Piscar LED
  }
}

void setup() {
  Serial.begin(115200);
  
  // Configurar pinos
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(RAIN_ANALOG_PIN, INPUT);
  pinMode(TIPPING_BUCKET_PIN, INPUT_PULLUP);
  pinMode(LED_STATUS_PIN, OUTPUT);
  
  // Configurar interrupção para pluviômetro
  attachInterrupt(digitalPinToInterrupt(TIPPING_BUCKET_PIN), rainInterrupt, FALLING);
  
  // Conectar WiFi
  setup_wifi();
  
  // Configurar MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  Serial.println("=== WATERWISE SENSOR PLUVIOMÉTRICO ===");
  Serial.println("Sensor inicializado com sucesso!");
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a rede WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado!");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Comando recebido [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println(message);
  
  // Processar comandos remotos
  if (String(topic) == "waterwise/sensor2/comando") {
    if (message == "reset_contador") {
      tipCount = 0;
      rainVolume = 0.0;
      Serial.println("Contador de chuva resetado!");
    } else if (message == "status") {
      enviarStatus();
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    
    String clientId = "WaterWise_Sensor2_";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado!");
      
      // Subscrever tópicos de comando
      client.subscribe("waterwise/sensor2/comando");
      
      // Publicar status de conexão
      enviarStatus();
      
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

void enviarStatus() {
  StaticJsonDocument<300> statusDoc;
  statusDoc["sensor_id"] = "SENSOR_002";
  statusDoc["tipo"] = "PLUVIOMETRICO";
  statusDoc["status"] = "online";
  statusDoc["timestamp"] = millis();
  statusDoc["location"] = "Propriedade Rural Mairiporã - Ponto 2";
  statusDoc["calibracao"]["mm_por_bascula"] = mmPerTip;
  statusDoc["uptime"] = millis() / 1000;
  
  char statusBuffer[400];
  serializeJson(statusDoc, statusBuffer);
  client.publish("waterwise/sensor2/status", statusBuffer);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;
    
    // ========== LEITURA DOS SENSORES ==========
    bool rainDetected = !digitalRead(RAIN_SENSOR_PIN); // Sensor invertido
    int rainIntensity = analogRead(RAIN_ANALOG_PIN);
    
    // Calcular volume acumulado
    rainVolume = tipCount * mmPerTip;
    
    // Calcular intensidade da chuva (últimos 10 segundos)
    static int lastTipCount = 0;
    float recentRainfall = (tipCount - lastTipCount) * mmPerTip;
    float intensityMmPerHour = (recentRainfall * 360); // Conversão para mm/h
    lastTipCount = tipCount;
    
    // ========== CLASSIFICAÇÃO DA CHUVA ==========
    String intensidadeClass;
    String alertLevel = "NORMAL";
    
    if (intensityMmPerHour == 0) {
      intensidadeClass = "SEM_CHUVA";
    } else if (intensityMmPerHour < 2.5) {
      intensidadeClass = "FRACA";
    } else if (intensityMmPerHour < 10) {
      intensidadeClass = "MODERADA";
    } else if (intensityMmPerHour < 50) {
      intensidadeClass = "FORTE";
      alertLevel = "ATENCAO";
    } else {
      intensidadeClass = "MUITO_FORTE";
      alertLevel = "EMERGENCIA";
    }
    
    // ========== CRIAÇÃO DO JSON PRINCIPAL ==========
    StaticJsonDocument<700> doc;
    doc["sensor_id"] = "SENSOR_002";
    doc["timestamp"] = now;
    doc["location"]["latitude"] = -23.3152;
    doc["location"]["longitude"] = -46.5869;
    doc["location"]["nome"] = "Ponto Monitoramento 2 - Mairiporã";
    
    // Dados pluviométricos
    doc["dados"]["chuva_detectada"] = rainDetected;
    doc["dados"]["intensidade_analogica"] = rainIntensity;
    doc["dados"]["volume_acumulado_mm"] = rainVolume;
    doc["dados"]["intensidade_mm_por_hora"] = intensityMmPerHour;
    doc["dados"]["classificacao"] = intensidadeClass;
    doc["dados"]["total_basculas"] = tipCount;
    
    // Dados para prevenção de enchentes
    doc["previsao"]["risco_enchente"] = (intensityMmPerHour > 25);
    doc["previsao"]["tempo_saturacao_estimado"] = calcularTempoSaturacao(intensityMmPerHour);
    doc["previsao"]["capacidade_absorcao_restante"] = calcularCapacidadeRestante();
    
    // Alertas
    doc["alertas"]["nivel"] = alertLevel;
    doc["alertas"]["requer_acao"] = (alertLevel != "NORMAL");
    
    // ========== PUBLICAÇÃO MQTT ==========
    char buffer[900];
    serializeJson(doc, buffer);
    
    // Publicar dados completos
    client.publish("waterwise/sensor2/dados_completos", buffer);
    
    // Publicar dados específicos para dashboards
    client.publish("waterwise/sensor2/intensidade", String(intensityMmPerHour).c_str());
    client.publish("waterwise/sensor2/volume_acumulado", String(rainVolume).c_str());
    client.publish("waterwise/sensor2/classificacao", intensidadeClass.c_str());
    
    // ========== LOG SERIAL ==========
    Serial.println("=== WATERWISE SENSOR 002 ===");
    Serial.println("Chuva Detectada: " + String(rainDetected ? "SIM" : "NÃO"));
    Serial.println("Intensidade: " + String(intensityMmPerHour) + " mm/h");
    Serial.println("Volume Acumulado: " + String(rainVolume) + " mm");
    Serial.println("Classificação: " + intensidadeClass);
    Serial.println("Total Básculas: " + String(tipCount));
    Serial.println("Nível Alerta: " + alertLevel);
    Serial.println("============================");
    
    // ========== ALERTAS AUTOMÁTICOS ==========
    if (alertLevel == "EMERGENCIA") {
      enviarAlertaEmergencia(intensityMmPerHour, rainVolume);
    }
    
    // Reset diário do contador (00:00h simulado a cada 24h de uptime)
    if ((millis() / 1000) % 86400 == 0 && tipCount > 0) {
      Serial.println("Reset diário automático do pluviômetro");
      tipCount = 0;
      rainVolume = 0.0;
    }
  }
}

float calcularTempoSaturacao(float intensidadeMmH) {
  // Estimativa baseada na capacidade de absorção do solo
  // Solo típico pode absorver ~25mm/h antes da saturação
  float capacidadeMaxima = 25.0;
  
  if (intensidadeMmH <= capacidadeMaxima) {
    return -1; // Sem risco de saturação
  }
  
  float excessoIntensidade = intensidadeMmH - capacidadeMaxima;
  float tempoEstimadoMinutos = (50.0 / excessoIntensidade) * 60; // 50mm capacidade residual
  
  return tempoEstimadoMinutos;
}

float calcularCapacidadeRestante() {
  // Simulação de capacidade baseada na chuva acumulada
  float capacidadeTotal = 75.0; // mm
  float restante = capacidadeTotal - rainVolume;
  
  return max(0.0f, restante);
}

void enviarAlertaEmergencia(float intensidade, float volume) {
  StaticJsonDocument<400> alertDoc;
  alertDoc["sensor_id"] = "SENSOR_002";
  alertDoc["tipo_alerta"] = "CHUVA_INTENSA";
  alertDoc["nivel"] = "EMERGENCIA";
  alertDoc["timestamp"] = millis();
  alertDoc["dados"]["intensidade_mm_h"] = intensidade;
  alertDoc["dados"]["volume_acumulado"] = volume;
  alertDoc["mensagem"] = "Chuva muito intensa detectada - Risco de enchente iminente";
  alertDoc["acao_recomendada"] = "Ativar sistema de drenagem e alertar populacao urbana";
  
  char alertBuffer[500];
  serializeJson(alertDoc, alertBuffer);
  client.publish("waterwise/alertas/emergencia", alertBuffer);
  
  Serial.println("🚨 ALERTA DE EMERGÊNCIA - CHUVA INTENSA!");
}