# Ecossistema WaterWise

"A enchente que alaga uma avenida pode começar com uma gota que o solo seco da zona rural não absorveu. Com WaterWise, cada metro de terra volta a ser uma esponja contra desastres."

## Visão Geral do Projeto

A **WaterWise** é um ecossistema tecnológico integrado que previne enchentes urbanas através do monitoramento inteligente de propriedades rurais.

### Repositório Principal
Para uma compreensão abrangente do projeto e sua arquitetura, acesse nosso repositório de overview:

**[WaterWise - Visão Geral do Projeto](https://github.com/WaterWise-GlobalSolution/waterwise-overview)**

## Organização GitHub

Para melhor organização dos entregáveis e centralização de todas as soluções do ecossistema WaterWise, criamos uma organização dedicada no GitHub.

### Acesse Nossa Organização
**[WaterWise Organization](https://github.com/WaterWise-GlobalSolution)**

---
# 🌊 WaterWise - Sistema Inteligente de Prevenção a Enchentes

## 📚 Trabalho Acadêmico - Global Solution 2025

**Disciplina:** DISRUPTIVE ARCHITECTURES: IOT, IOB & GENERATIVE IA  
**Instituição:** FIAP - Faculdade de Informática e Administração Paulista  
**Curso:** Análise e Desenvolvimento de Sistemas 
**Semestre:** 2º Semestre 2025  

---

## 👥 Equipe de Desenvolvimento - Turma 2TDSPS

| Nome                 | RM     |
| -------------------- | ------ |
| Felipe Amador        | 553528 |
| Leonardo de Oliveira | 554024 |
| Sara Sousa           | 552656 |

---

## 🎯 Objetivo do Projeto

O **WaterWise** é um sistema IoT desenvolvido para **prevenção de enchentes urbanas** através do monitoramento inteligente de propriedades rurais. O sistema detecta condições críticas que podem levar ao escoamento superficial e inundações em áreas urbanas, fornecendo alertas precoces para autoridades e comunidades.

### 🔬 Hipótese Central

> "Solo seco + Chuva intensa = Alto risco de enchente urbana"

Quando o solo rural está ressecado, sua capacidade de absorção é drasticamente reduzida. Se uma chuva intensa ocorre nessas condições, a água escoa rapidamente para rios e córregos, causando enchentes nas cidades a jusante.

---

## 🏗️ Arquitetura do Sistema

### 📡 **Componentes IoT (3 Sensores)**

#### 1. **DHT22** - Temperatura e Umidade Ambiente
- Monitoramento de condições climáticas
- Detecção de clima extremo
- GPIO 12

#### 2. **Sensor de Umidade do Solo** (Simulado)
- Medição da capacidade de absorção do solo
- Fator crítico para prevenção de enchentes
- GPIO 34 (Potenciômetro no Wokwi)

#### 3. **Sensor de Precipitação** (Simulado)
- Intensidade de chuva em tempo real
- Correlação com risco de escoamento
- GPIO 35 (Potenciômetro no Wokwi)

### 🧮 **Algoritmo WaterWise**

```
Risco de Enchente = f(Umidade_Solo, Intensidade_Chuva, Temperatura)

Onde:
- Solo Seco (< 25%) → +3 pontos de risco
- Chuva Intensa (> 70%) → +4 pontos de risco  
- Combinação Crítica → +2 pontos bônus
- Escala: 0-10 (Baixo → Crítico)
```

### 📊 **Dashboard Node-RED**

- **4 Gauges** para visualização dos sensores
- **Análise de Risco** em tempo real (0-10)
- **Alertas Automáticos** para situações críticas
- **Gráfico Histórico** dos últimos 10 minutos
- **Status do Sistema** com saúde dos sensores

---

## 🛠️ Tecnologias Utilizadas

### **Hardware & Simulação**
- **ESP32** - Microcontrolador principal
- **DHT22** - Sensor real de temperatura/umidade
- **Wokwi** - Simulador para desenvolvimento
- **Potenciômetros** - Simulação dos sensores de solo e chuva

### **Software & Plataformas**
- **C++/Arduino** - Programação do ESP32
- **Python Streamlit** - Dashboard e visualização
- **MQTT** - Protocolo de comunicação IoT
- **JSON** - Formato de dados estruturados
- **PlatformIO** - Environment de desenvolvimento

### **Bibliotecas**
- `ArduinoJson` - Manipulação de dados JSON
- `DHT sensor library` - Interface com sensor DHT22
- `PubSubClient` - Cliente MQTT para ESP32
- `Adafruit Unified Sensor` - Base para sensores

---

## 📁 Estrutura do Projeto

```
WaterWise/
├── 📂 q1/src/
│   └── main.cpp                 # Código principal ESP32
├── 📂 nodered/
│   └── flows.json              # Fluxos Node-RED
├── 📂 waterwise/
│   ├── diagram.json            # Diagrama Wokwi
│   ├── wokwi.toml             # Configuração Wokwi
│   └── esp32_mqtt_dht22.ino   # Versão Arduino IDE
├── 📂 docs/
│   └── README_WaterWise.md    # Documentação técnica
├── platformio.ini              # Configuração PlatformIO
├── README.md                   # Este arquivo
└── LICENSE                     # Licença MIT
```

---

## 🚀 Como Executar o Projeto

### **Pré-requisitos**
- Visual Studio Code
- Extensão PlatformIO IDE
- Extensão Wokwi (com licença)
- Python - Streamlit - Interface

### **1. Configuração do Ambiente**

```bash
# Clonar repositório
git clone https://github.com/WaterWise-GlobalSolution/waterwise-iot_ia.git
cd waterwise

# Abrir no VS Code
code .
```

### **2. Configurar Identificadores**

Edite `q1/src/main.cpp`:
```cpp
const char* FARM_ID = "FARM_SEU_GRUPO_2025";
const char* TEAM_NAME = "NOME_DO_SEU_GRUPO";
const char* LOCATION = "SP_Sua_Localizacao";
```

### **3. Executar Simulação**


---

## 📊 Resultados e Demonstração

### **Cenários de Teste**

#### 🟢 **Cenário Normal**
- **Solo:** 60% | **Chuva:** 20% | **Risco:** 2/10
- **Status:** "Baixo - Condições normais"
- **Ação:** "Monitoramento rotineiro"

#### 🟡 **Cenário de Seca**
- **Solo:** 15% | **Chuva:** 5% | **Risco:** 5/10
- **Status:** "Alto - Preparação"
- **Alertas:** "🏜️ SECA"

#### 🔴 **Cenário CRÍTICO**
- **Solo:** 12% | **Chuva:** 85% | **Risco:** 10/10
- **Status:** "CRÍTICO - EMERGÊNCIA"
- **Alertas:** "🌊 ENCHENTE | 🏜️ SECA"
- **Ação:** "EVACUAR ÁREAS DE RISCO"

---

## 🧪 Testes e Validação

### **Testes Automatizados**
- ✅ Leitura dos 3 sensores
- ✅ Conectividade WiFi/MQTT
- ✅ Algoritmo de análise de risco
- ✅ Geração de JSON estruturado
- ✅ Dashboard responsivo

### **Testes de Integração**
- ✅ ESP32 ↔ MQTT ↔ API ↔ BANCO DE DADOS ORACLE
- ✅ Simulação automática de dados
- ✅ Alertas em tempo real

---


<div align="center">

---
  
## 💧 <strong>WaterWise: Cada Gota Importa, Cada Metro Conta</strong><br>

---

**Global Solution 2025 - FIAP**  
*Desenvolvido com 💙 pela equipe WaterWise*

</div>
