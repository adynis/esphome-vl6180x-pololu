import esphome.codegen as cg
import esphome.config_validation as cv

# Definim namespace-ul care va fi folosit în C++ și sensor.py
vl6180x_pololu_ns = cg.esphome_ns.namespace("vl6180x_pololu")

CONFIG_SCHEMA = cv.Schema({})