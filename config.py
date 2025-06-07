"""
WaterWise - Configurações do Sistema FIAP
"""
import os
from decouple import config

# Configurações Oracle FIAP
ORACLE_CONFIG = {
    'host': config('ORACLE_HOST', default='oracle.fiap.com.br'),
    'port': config('ORACLE_PORT', default=1521, cast=int),
    'service': config('ORACLE_SERVICE', default='ORCL'),
    'user': config('ORACLE_USER', default='RM553528'),
    'password': config('ORACLE_PASSWORD', default='150592')
}

# Configurações MQTT
MQTT_CONFIG = {
    'host': config('MQTT_HOST', default='test.mosquitto.org'),
    'port': config('MQTT_PORT', default=1883, cast=int),
    'topic': config('MQTT_TOPIC', default='fiap/waterwise/sensors/data')
}

# Configurações de Log
LOG_CONFIG = {
    'level': config('LOG_LEVEL', default='INFO'),
    'file': config('LOG_FILE', default='waterwise.log')
}