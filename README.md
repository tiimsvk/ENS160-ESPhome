#ENS160 for ESPHome Custom Component
![ens160 modul](https://dfimg.dfrobot.com/store/data/SEN0515/SEN0515.jpg)

*Added Features:

- Sleep / Idle / Measure (for managing current consumption: 10 µA / 2.5 mA / 30 mA)
Selection via select template

- Status Function: initialize / warm-up / normal
- Debug feature that bypasses the 3-minute warm-up time to start sending data immediately. Recommended only for testing the sensor at startup.

* Note: I added this feature in two ways. One is via the template select, which works without issues. The other can be added directly to the ENS160 component, but it causes restarts. I haven’t been able to fix this, but I left it in the code in case someone wants to finish it.

Important Note: This sensor is sensitive to undervoltage. If you don’t provide a proper 3.3 V supply with sufficient current, the sensor won’t start and will report normal start bad not reading. Therefore, I recommend either adding a capacitor between the 1.8 V LDO output and GND or ensuring stable power supply (47uF).

*YAML:
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
