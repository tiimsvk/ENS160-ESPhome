#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace ens160_base {

class ENS160Component : public PollingComponent, public sensor::Sensor {
 public:
  enum OperationMode : uint8_t {
    OPMODE_DEEP_SLEEP = 0x00,
    OPMODE_IDLE = 0x01,
    OPMODE_STD = 0x02,
    OPMODE_RESET = 0xF0,
  };

  void set_co2(sensor::Sensor *co2) { co2_ = co2; }
  void set_tvoc(sensor::Sensor *tvoc) { tvoc_ = tvoc; }
  void set_aqi(sensor::Sensor *aqi) { aqi_ = aqi; }

  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }

  void set_operation_mode_select(select::Select *select) { operation_mode_select_ = select; }
  void set_state_text_sensor(text_sensor::TextSensor *ts) { state_text_ = ts; }
  void publish_operation_mode_select_(OperationMode mode);
  bool set_operation_mode_from_select(OperationMode mode);

  bool set_operation_mode(OperationMode mode);

  // NOVÉ: len uloží požiadavku, reálne prepnutie sa spraví v loop()
  void request_operation_mode(OperationMode mode) {
    pending_mode_ = mode;
    pending_mode_set_ = true;
  }

  void set_fast_warmup_debug(bool enable) { fast_warmup_debug_ = enable; }

  void setup() override;
  void update() override;
  void dump_config() override;
  void loop() override;  // NOVÉ

 protected:
  void send_env_data_();
  void publish_state_text_();

  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    INVALID_ID,
    VALIDITY_INVALID,
    READ_FAILED,
    WRITE_FAILED,
    STD_OPMODE_FAILED,
  } error_code_{NONE};

  enum ValidityFlag {
    NORMAL_OPERATION = 0,
    WARMING_UP,
    INITIAL_STARTUP,
    INVALID_OUTPUT,
  } validity_flag_;

  bool warming_up_{false};
  bool initial_startup_{false};
  bool fast_warmup_debug_{false};

  virtual bool read_byte(uint8_t a_register, uint8_t *data) = 0;
  virtual bool write_byte(uint8_t a_register, uint8_t data) = 0;
  virtual bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) = 0;
  virtual bool write_bytes(uint8_t a_register, uint8_t *data, size_t len) = 0;

  uint8_t firmware_ver_major_{0};
  uint8_t firmware_ver_minor_{0};
  uint8_t firmware_ver_build_{0};

  sensor::Sensor *co2_{nullptr};
  sensor::Sensor *tvoc_{nullptr};
  sensor::Sensor *aqi_{nullptr};

  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *temperature_{nullptr};

  select::Select *operation_mode_select_{nullptr};
  text_sensor::TextSensor *state_text_{nullptr};
  bool operation_mode_select_busy_{false};

  // NOVÉ
  bool pending_mode_set_{false};
  OperationMode pending_mode_{OPMODE_STD};
};

}  // namespace ens160_base
}  // namespace esphome