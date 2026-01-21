// ENS160 sensor with I2C interface from ScioSense
//
// Datasheet: https://www.sciosense.com/wp-content/uploads/documents/SC-001224-DS-7-ENS160-Datasheet.pdf
//
// Implementation based on:
//   https://github.com/sciosense/ENS160_driver

#include "ens160_base.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ens160_base {

static const char *const TAG = "ens160";

static const uint8_t ENS160_BOOTING = 10;

static const uint16_t ENS160_PART_ID = 0x0160;

static const uint8_t ENS160_REG_PART_ID = 0x00;
static const uint8_t ENS160_REG_OPMODE = 0x10;
static const uint8_t ENS160_REG_CONFIG = 0x11;
static const uint8_t ENS160_REG_COMMAND = 0x12;
static const uint8_t ENS160_REG_TEMP_IN = 0x13;
static const uint8_t ENS160_REG_DATA_STATUS = 0x20;
static const uint8_t ENS160_REG_DATA_AQI = 0x21;
static const uint8_t ENS160_REG_DATA_TVOC = 0x22;
static const uint8_t ENS160_REG_DATA_ECO2 = 0x24;

static const uint8_t ENS160_REG_GPR_READ_0 = 0x48;
static const uint8_t ENS160_REG_GPR_READ_4 = ENS160_REG_GPR_READ_0 + 4;

static const uint8_t ENS160_COMMAND_NOP = 0x00;
static const uint8_t ENS160_COMMAND_CLRGPR = 0xCC;
static const uint8_t ENS160_COMMAND_GET_APPVER = 0x0E;

// helpers to mask reserved bits
static const uint8_t ENS160_DATA_STATUS_STATAS = 0x80;
static const uint8_t ENS160_DATA_STATUS_STATER = 0x40;
static const uint8_t ENS160_DATA_STATUS_VALIDITY = 0x0C;
static const uint8_t ENS160_DATA_STATUS_NEWDAT = 0x02;
static const uint8_t ENS160_DATA_STATUS_NEWGPR = 0x01;
static const uint8_t ENS160_DATA_AQI = 0x07;

void ENS160Component::publish_operation_mode_select_(OperationMode mode) {
  if (this->operation_mode_select_ == nullptr || this->operation_mode_select_busy_)
    return;

  const char *opt = nullptr;
  switch (mode) {
    case OPMODE_DEEP_SLEEP:
      opt = "Deep Sleep";
      break;
    case OPMODE_IDLE:
      opt = "Idle";
      break;
    case OPMODE_STD:
      opt = "Measure";
      break;
    default:
      break;
  }

  if (opt != nullptr) {
    this->operation_mode_select_busy_ = true;
    this->operation_mode_select_->publish_state(opt);
    this->operation_mode_select_busy_ = false;
  }
}

bool ENS160Component::set_operation_mode_from_select(OperationMode mode) {
  if (this->operation_mode_select_busy_)
    return false;

  this->operation_mode_select_busy_ = true;
  bool ok = this->set_operation_mode(mode);
  this->operation_mode_select_busy_ = false;
  return ok;
}

bool ENS160Component::set_operation_mode(OperationMode mode) {
  if (!this->write_byte(ENS160_REG_OPMODE, static_cast<uint8_t>(mode))) {
    this->error_code_ = WRITE_FAILED;
    this->mark_failed();
    return false;
  }
  if (mode == OPMODE_STD) {
    delay(ENS160_BOOTING);
    uint8_t op_mode;
    if (!this->read_byte(ENS160_REG_OPMODE, &op_mode)) {
      this->error_code_ = READ_FAILED;
      this->mark_failed();
      return false;
    }
    if (op_mode != OPMODE_STD) {
      this->error_code_ = STD_OPMODE_FAILED;
      this->mark_failed();
      return false;
    }
  }

  this->publish_operation_mode_select_(mode);
  return true;
}

void ENS160Component::setup() {
  // check part_id
  uint16_t part_id;
  if (!this->read_bytes(ENS160_REG_PART_ID, reinterpret_cast<uint8_t *>(&part_id), 2)) {
    this->error_code_ = COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }
  if (part_id != ENS160_PART_ID) {
    this->error_code_ = INVALID_ID;
    this->mark_failed();
    return;
  }

  // reset -> idle -> standard
  if (!this->set_operation_mode(OPMODE_RESET)) return;
  delay(ENS160_BOOTING);

  uint8_t status_value;
  if (!this->read_byte(ENS160_REG_DATA_STATUS, &status_value)) {
    this->error_code_ = READ_FAILED;
    this->mark_failed();
    return;
  }
  this->validity_flag_ = static_cast<ValidityFlag>((ENS160_DATA_STATUS_VALIDITY & status_value) >> 2);

  if (this->validity_flag_ == INVALID_OUTPUT) {
    this->error_code_ = VALIDITY_INVALID;
    this->mark_failed();
    return;
  }

  if (!this->set_operation_mode(OPMODE_IDLE)) return;

  // clear command
  if (!this->write_byte(ENS160_REG_COMMAND, ENS160_COMMAND_NOP)) {
    this->error_code_ = WRITE_FAILED;
    this->mark_failed();
    return;
  }
  if (!this->write_byte(ENS160_REG_COMMAND, ENS160_COMMAND_CLRGPR)) {
    this->error_code_ = WRITE_FAILED;
    this->mark_failed();
    return;
  }

  // read firmware version
  if (!this->write_byte(ENS160_REG_COMMAND, ENS160_COMMAND_GET_APPVER)) {
    this->error_code_ = WRITE_FAILED;
    this->mark_failed();
    return;
  }
  uint8_t version_data[3];
  if (!this->read_bytes(ENS160_REG_GPR_READ_4, version_data, 3)) {
    this->error_code_ = READ_FAILED;
    this->mark_failed();
    return;
  }
  this->firmware_ver_major_ = version_data[0];
  this->firmware_ver_minor_ = version_data[1];
  this->firmware_ver_build_ = version_data[2];

  if (!this->set_operation_mode(OPMODE_STD)) return;

  // Jednorazovo publikuj aktuálny režim do selectu podľa reálneho OPMODE registra
  if (this->operation_mode_select_ != nullptr) {
    uint8_t op_mode_read;
    if (this->read_byte(ENS160_REG_OPMODE, &op_mode_read)) {
      this->publish_operation_mode_select_(static_cast<OperationMode>(op_mode_read));
    }
  }
}

void ENS160Component::publish_state_text_() {
  if (this->state_text_ == nullptr)
    return;

  const char *state_str = nullptr;
  switch (this->validity_flag_) {
    case NORMAL_OPERATION:
      state_str = "NORMAL_OPERATION";
      break;
    case WARMING_UP:
      state_str = "WARMING_UP";
      break;
    case INITIAL_STARTUP:
      state_str = "INITIAL_STARTUP";
      break;
    case INVALID_OUTPUT:
      state_str = "INVALID_OUTPUT";
      break;
  }
  if (state_str != nullptr)
    this->state_text_->publish_state(state_str);
}

void ENS160Component::update() {
  uint8_t status_value, data_ready;

  if (!this->read_byte(ENS160_REG_DATA_STATUS, &status_value)) {
    ESP_LOGW(TAG, "Error reading status register");
    this->status_set_warning();
    return;
  }

  // verbose status logging
  ESP_LOGV(TAG,
           "Status: ENS160 STATAS bit    0x%x\n"
           "Status: ENS160 STATER bit    0x%x\n"
           "Status: ENS160 VALIDITY FLAG 0x%02x\n"
           "Status: ENS160 NEWDAT bit    0x%x\n"
           "Status: ENS160 NEWGPR bit    0x%x",
           (ENS160_DATA_STATUS_STATAS & (status_value)) == ENS160_DATA_STATUS_STATAS,
           (ENS160_DATA_STATUS_STATER & (status_value)) == ENS160_DATA_STATUS_STATER,
           (ENS160_DATA_STATUS_VALIDITY & status_value) >> 2,
           (ENS160_DATA_STATUS_NEWDAT & (status_value)) == ENS160_DATA_STATUS_NEWDAT,
           (ENS160_DATA_STATUS_NEWGPR & (status_value)) == ENS160_DATA_STATUS_NEWGPR);

  data_ready = ENS160_DATA_STATUS_NEWDAT & status_value;
  this->validity_flag_ = static_cast<ValidityFlag>((ENS160_DATA_STATUS_VALIDITY & status_value) >> 2);
  this->publish_state_text_();

  switch (validity_flag_) {
    case NORMAL_OPERATION:
      if (data_ready != ENS160_DATA_STATUS_NEWDAT) {
        ESP_LOGD(TAG, "ENS160 readings unavailable - Normal Operation but readings not ready");
        return;
      }
      break;

    case INITIAL_STARTUP:
      if (this->fast_warmup_debug_) {
        ESP_LOGW(TAG, "FAST WARM-UP ENABLED: bypassing INITIAL_STARTUP guard");
        break;
      }
      if (!this->initial_startup_) {
        this->initial_startup_ = true;
        ESP_LOGI(TAG, "ENS160 readings unavailable - 1 hour startup required after first power on");
      }
      return;

    case WARMING_UP:
      if (this->fast_warmup_debug_) {
        ESP_LOGW(TAG, "FAST WARM-UP ENABLED: bypassing WARMING_UP guard");
        break;
      }
      if (!this->warming_up_) {
        this->warming_up_ = true;
        ESP_LOGI(TAG, "ENS160 readings not available yet - Warming up requires 3 minutes");
        this->send_env_data_();
      }
      return;

    case INVALID_OUTPUT:
      ESP_LOGE(TAG, "ENS160 Invalid Status - No valid output");
      this->status_set_warning();
      return;
  }

  // read new data
  uint16_t data_eco2;
  if (!this->read_bytes(ENS160_REG_DATA_ECO2, reinterpret_cast<uint8_t *>(&data_eco2), 2)) {
    ESP_LOGW(TAG, "Error reading eCO2 data register");
    this->status_set_warning();
    return;
  }
  if (this->co2_ != nullptr) {
    this->co2_->publish_state(data_eco2);
  }

  uint16_t data_tvoc;
  if (!this->read_bytes(ENS160_REG_DATA_TVOC, reinterpret_cast<uint8_t *>(&data_tvoc), 2)) {
    ESP_LOGW(TAG, "Error reading TVOC data register");
    this->status_set_warning();
    return;
  }
  if (this->tvoc_ != nullptr) {
    this->tvoc_->publish_state(data_tvoc);
  }

  uint8_t data_aqi;
  if (!this->read_byte(ENS160_REG_DATA_AQI, &data_aqi)) {
    ESP_LOGW(TAG, "Error reading AQI data register");
    this->status_set_warning();
    return;
  }
  if (this->aqi_ != nullptr) {
    data_aqi = ENS160_DATA_AQI & data_aqi;  // mask reserved bits
    this->aqi_->publish_state(data_aqi);
  }

  this->status_clear_warning();

  // set temperature and humidity compensation data
  this->send_env_data_();
}

void ENS160Component::loop() {
  if (this->pending_mode_set_) {
    auto mode = this->pending_mode_;
    this->pending_mode_set_ = false;
    this->set_operation_mode_from_select(mode);
  }
}

void ENS160Component::send_env_data_() {
  if (this->temperature_ == nullptr && this->humidity_ == nullptr)
    return;

  float temperature = NAN;
  if (this->temperature_ != nullptr)
    temperature = this->temperature_->state;

  if (std::isnan(temperature) || temperature < -40.0f || temperature > 85.0f) {
    ESP_LOGW(TAG, "Invalid external temperature - compensation values not updated");
    return;
  } else {
    ESP_LOGV(TAG, "External temperature compensation: %.1f°C", temperature);
  }

  float humidity = NAN;
  if (this->humidity_ != nullptr)
    humidity = this->humidity_->state;

  if (std::isnan(humidity) || humidity < 0.0f || humidity > 100.0f) {
    ESP_LOGW(TAG, "Invalid external humidity - compensation values not updated");
    return;
  } else {
    ESP_LOGV(TAG, "External humidity compensation:    %.1f%%", humidity);
  }

  uint16_t t = (uint16_t) ((temperature + 273.15f) * 64.0f);
  uint16_t h = (uint16_t) (humidity * 512.0f);

  uint8_t data[4];
  data[0] = t & 0xff;
  data[1] = (t >> 8) & 0xff;
  data[2] = h & 0xff;
  data[3] = (h >> 8) & 0xff;

  if (!this->write_bytes(ENS160_REG_TEMP_IN, data, 4)) {
    ESP_LOGE(TAG, "Error writing compensation values");
    this->status_set_warning();
    return;
  }
}

void ENS160Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ENS160:");

  switch (this->error_code_) {
    case COMMUNICATION_FAILED:
      ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
      break;
    case READ_FAILED:
      ESP_LOGE(TAG, "Error reading from register");
      break;
    case WRITE_FAILED:
      ESP_LOGE(TAG, "Error writing to register");
      break;
    case INVALID_ID:
      ESP_LOGE(TAG, "Sensor reported an invalid ID. Is this a ENS160?");
      break;
    case VALIDITY_INVALID:
      ESP_LOGE(TAG, "Invalid Device Status - No valid output");
      break;
    case STD_OPMODE_FAILED:
      ESP_LOGE(TAG, "Device failed to achieve Standard Operating Mode");
      break;
    case NONE:
      ESP_LOGD(TAG, "Setup successful");
      break;
  }
  ESP_LOGI(TAG, "Firmware Version: %d.%d.%d", this->firmware_ver_major_, this->firmware_ver_minor_,
           this->firmware_ver_build_);

  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "CO2 Sensor:", this->co2_);
  LOG_SENSOR("  ", "TVOC Sensor:", this->tvoc_);
  LOG_SENSOR("  ", "AQI Sensor:", this->aqi_);

  if (this->temperature_ != nullptr && this->humidity_ != nullptr) {
    LOG_SENSOR("  ", "  Temperature Compensation:", this->temperature_);
    LOG_SENSOR("  ", "  Humidity Compensation:", this->humidity_);
  } else {
    ESP_LOGCONFIG(TAG, "  Compensation: Not configured");
  }

  ESP_LOGCONFIG(TAG, "Fast warm-up (debug): %s", this->fast_warmup_debug_ ? "ENABLED" : "disabled");
}

}  // namespace ens160_base
}  // namespace esphome