#ENS 160 for ESPHome Custom Component
https://dfimg.dfrobot.com/store/data/SEN0515/SEN0515.jpg
Added Features:

- Sleep / Idle / Measure (for managing current consumption: 10µA / 2.5mA / 30mA)
  selectable via a select template*

- Status function: initialize / warm up / normal

*Note: I added this function in two ways. One is via a template select, which works without issues. The other can be added directly to the ENS160 component, but it causes a restart. I haven’t been able to fix it, but I’m leaving it in the code in case someone is interested in completing it.

YAML:
```
#-------------------------------------------
# EXTERNAL COMPONENT
#-------------------------------------------
external_components:
  - source: github://tiimsvk/ENS160-ESPhome@main
    components: [ens160_base, ens160_i2c]
    
#-------------------------------------------
# SENSORS
#-------------------------------------------
sensor:
  - platform: ens160_i2c
    id:       ens160_1
    address:  0x53
    update_interval: 10s
    eco2:
      name: "eCO2"
    tvoc:
      name: "TVOC"
    aqi:
      name: "AQI"
    compensation:
      temperature: temp_sensor
      humidity: humi_sensor
    # add funcion
    operation_mode:
      name: Operation mode # reset after selected fix code!
    state:
      name: Status
    fast_warmup_debug: true
    
#-------------------------------------------
# SELECT
#-------------------------------------------
select:
  - platform: template
    name: "Mode"
    optimistic: true
    options:
      - Deep Sleep
      - Idle
      - Measure
    set_action:
      - lambda: |-
          using Mode = esphome::ens160_base::ENS160Component::OperationMode;
          if (x == "Deep Sleep") {
            id(ens160_1).set_operation_mode(Mode::OPMODE_DEEP_SLEEP);
          } else if (x == "Idle") {
            id(ens160_1).set_operation_mode(Mode::OPMODE_IDLE);
          } else if (x == "Measure") {
            id(ens160_1).set_operation_mode(Mode::OPMODE_STD);
          }
```
