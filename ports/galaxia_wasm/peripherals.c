// Peripherals of the Galaxia below the Thingz modules, for the WebAssembly build
// (declarations in idf/idf_periph.h). The Thingz drivers run unchanged on top:
//   - touch sensor: pads N/S/E/W (GPIO 8, 4, 5, 9), touched from the page;
//   - I2C0, as on a PCB 1.0.9: LIS2DH12 accelerometer (0x19), LIS2MDL magnetometer
//     (0x1E) and the settings EEPROM (0x50); the LIS2DH12 is the register model of
//     galaxia_sim's QEMU machine (hw/sensor/ in patches/);
//   - LEDC: the RGB LED (GPIO 41/40/39, common 10), shown by the page; its light
//     measurement (LED discharge timed through ADC1 channel 9) follows the light
//     level set in the page;
//   - DAC: the sound, as the waveform the firmware writes, played by the page;
// (Wi-Fi, ESP-NOW and the network: net.c)
// The page sets the inputs through the galaxia_set_* functions and the input queues
// of library.js; peripherals_poll() delivers them from idf_shim_poll().

#include <math.h>
#include <string.h>
#include <emscripten.h>

#include "py/runtime.h"
#include "idf/idf_all.h"
#include "peripherals.h"

extern int galaxia_js_touch_get(int pad);
extern int galaxia_js_touch_next_changed(void);
extern int galaxia_js_light(void);
extern void galaxia_js_led(int r, int g, int b);
extern void galaxia_js_sound_loop(int rate, const uint8_t *samples, int len);
extern void galaxia_js_sound_chunk(int rate, const uint8_t *samples, int len);
extern void galaxia_js_sound_stop(void);
extern void galaxia_js_sleep(int ms);

#define BIT(n) (1u << (n))

// ======================================================================== touch

#define TOUCH_BENCHMARK 20000           // raw count of an untouched pad
#define TOUCH_TOUCHED   40000           // touched: well over benchmark + threshold

static intr_handler_t touch_isr;
static void *touch_isr_arg;
static touch_pad_t touch_current;
static uint32_t touch_configured;       // pads configured by touch_pad_config

static bool touched(touch_pad_t pad) {
    return (touch_configured & BIT(pad)) && galaxia_js_touch_get(pad);
}

esp_err_t touch_pad_init(void) { return ESP_OK; }
esp_err_t touch_pad_deinit(void) { return ESP_OK; }
esp_err_t touch_pad_reset(void) { return ESP_OK; }
esp_err_t touch_pad_denoise_set_config(const touch_pad_denoise_t *cfg) { return ESP_OK; }
esp_err_t touch_pad_denoise_enable(void) { return ESP_OK; }
esp_err_t touch_pad_filter_set_config(const touch_filter_config_t *cfg) { return ESP_OK; }
esp_err_t touch_pad_filter_enable(void) { return ESP_OK; }
esp_err_t touch_pad_timeout_set(bool enable, uint32_t threshold) { return ESP_OK; }
esp_err_t touch_pad_intr_enable(touch_pad_intr_mask_t mask) { return ESP_OK; }
esp_err_t touch_pad_set_fsm_mode(touch_fsm_mode_t mode) { return ESP_OK; }
esp_err_t touch_pad_fsm_start(void) { return ESP_OK; }
esp_err_t touch_pad_fsm_stop(void) { return ESP_OK; }
esp_err_t touch_pad_set_thresh(touch_pad_t pad, uint32_t threshold) { return ESP_OK; }

esp_err_t touch_pad_config(touch_pad_t pad) {
    if (pad < 0 || pad > 14) {
        return ESP_ERR_INVALID_ARG;
    }
    touch_configured |= BIT(pad);
    return ESP_OK;
}

esp_err_t touch_pad_isr_register(intr_handler_t fn, void *arg, touch_pad_intr_mask_t mask) {
    touch_isr = fn;
    touch_isr_arg = arg;
    return ESP_OK;
}

esp_err_t touch_pad_read_benchmark(touch_pad_t pad, uint32_t *value) {
    *value = TOUCH_BENCHMARK;
    return ESP_OK;
}

esp_err_t touch_pad_filter_read_smooth(touch_pad_t pad, uint32_t *value) {
    *value = touched(pad) ? TOUCH_TOUCHED : TOUCH_BENCHMARK;
    return ESP_OK;
}

esp_err_t touch_pad_read_raw_data(touch_pad_t pad, uint32_t *value) {
    return touch_pad_filter_read_smooth(pad, value);
}

uint32_t touch_pad_get_status(void) {
    uint32_t status = 0;
    for (touch_pad_t pad = 0; pad < 15; pad++) {
        if (touched(pad)) {
            status |= BIT(pad);
        }
    }
    return status;
}

touch_pad_t touch_pad_get_current_meas_channel(void) {
    return touch_current;
}

// A pad changed: the sensor raises its active/inactive interrupt for that channel
static void touch_poll(void) {
    int pad;
    while ((pad = galaxia_js_touch_next_changed()) >= 0) {
        if (touch_isr && (touch_configured & BIT(pad))) {
            touch_current = pad;
            touch_isr(touch_isr_arg);
        }
    }
}

// ========================================================================== I2C

// --- LIS2DH12 accelerometer (galaxia_sim hw/sensor/lis2dh12.c) ---
#define LIS_WHO_AM_I    0x0F
#define LIS_CTRL_REG1   0x20
#define LIS_CTRL_REG4   0x23
#define LIS_CTRL_REG5   0x24
#define LIS_STATUS      0x27
#define LIS_OUT_X_L     0x28
#define LIS_OUT_Z_H     0x2D
#define LIS_AUTOINC     0x80

static struct {
    uint8_t regs[0x40];
    uint8_t pointer;
    bool pointer_set;
    int32_t mg[3];          // acceleration, milli-g, chip axes
} lis = { .regs = { [LIS_CTRL_REG1] = 0x07 }, .mg = { 0, 0, 1000 } };

static int16_t lis_output(int axis) {
    static const int hr[] = { 1, 2, 4, 12 }, nm[] = { 4, 8, 16, 48 }, lp[] = { 16, 32, 64, 192 };
    const int fs = (lis.regs[LIS_CTRL_REG4] >> 4) & 3;
    int sens, shift;
    if (lis.regs[LIS_CTRL_REG1] & BIT(3)) {
        sens = lp[fs];
        shift = 8;
    } else if (lis.regs[LIS_CTRL_REG4] & BIT(3)) {
        sens = hr[fs];
        shift = 4;
    } else {
        sens = nm[fs];
        shift = 6;
    }
    const int limit = (1 << (15 - shift)) - 1;
    int digits = lis.mg[axis] / sens;
    digits = digits < -limit - 1 ? -limit - 1 : digits > limit ? limit : digits;
    return (int16_t)(digits * (1 << shift));
}

static uint8_t lis_read_reg(uint8_t reg) {
    if (reg == LIS_WHO_AM_I) {
        return 0x33;
    }
    if (reg == LIS_STATUS) {
        return 0x0f;        // ZYXDA and X/Y/Z data available
    }
    if (reg >= LIS_OUT_X_L && reg <= LIS_OUT_Z_H) {
        const int16_t v = lis_output((reg - LIS_OUT_X_L) / 2);
        return (reg & 1) ? (uint8_t)(v >> 8) : (uint8_t)v;
    }
    return reg < sizeof(lis.regs) ? lis.regs[reg] : 0;
}

static void lis_start(void) {
    lis.pointer_set = false;
}

static uint8_t lis_rx(void) {
    const uint8_t reg = lis.pointer & ~LIS_AUTOINC;
    const uint8_t v = lis_read_reg(reg);
    if (lis.pointer & LIS_AUTOINC) {
        lis.pointer = LIS_AUTOINC | ((reg + 1) & 0x7f);
    }
    return v;
}

static void lis_tx(uint8_t data) {
    if (!lis.pointer_set) {
        lis.pointer = data;
        lis.pointer_set = true;
        return;
    }
    const uint8_t reg = lis.pointer & ~LIS_AUTOINC;
    if (reg < sizeof(lis.regs)) {
        if (reg == LIS_CTRL_REG5) {
            data &= ~BIT(7);    // BOOT: done at once
        }
        lis.regs[reg] = data;
    }
    if (lis.pointer & LIS_AUTOINC) {
        lis.pointer = LIS_AUTOINC | ((reg + 1) & 0x7f);
    }
}

// --- LIS2MDL magnetometer (0x1E), the compass of PCB 1.0.9 and 1.1.0 ---
// Registers of the LIS2MDL datasheet: fixed 1.5 mG/LSB, the hard-iron offset
// registers subtracted from the output, address always auto-incremented,
// continuous / single / idle modes (CFG_REG_A MD), soft reset.
#define MDL_OFFSET_X_L  0x45
#define MDL_OFFSET_Z_H  0x4A
#define MDL_WHO_AM_I    0x4F
#define MDL_CFG_REG_A   0x60
#define MDL_INT_CTRL    0x63
#define MDL_STATUS      0x67
#define MDL_OUTX_L      0x68
#define MDL_OUTZ_H      0x6D
#define MDL_TEMP_OUT_L  0x6E
#define MDL_TEMP_OUT_H  0x6F
#define MDL_SOFT_RST    BIT(5)
#define MDL_REBOOT      BIT(6)
#define MDL_MD_MASK     0x03
#define MDL_MD_CONTINUOUS 0x00
#define MDL_MD_SINGLE   0x01
#define MDL_MD_IDLE     0x03

static struct {
    uint8_t regs[0x70];
    uint8_t pointer;
    bool pointer_set;
    int16_t out[3];         // last measurement, LSB
    bool measured;          // a measurement since the reset (else the output is 0)
    int32_t mgauss[3];      // field, milligauss, chip axes
} mdl = { .regs = { [MDL_CFG_REG_A] = MDL_MD_IDLE, [MDL_INT_CTRL] = 0xE0 } };   // power-on values

static void mdl_reset(void) {
    memset(mdl.regs, 0, sizeof(mdl.regs));
    mdl.regs[MDL_CFG_REG_A] = MDL_MD_IDLE;
    mdl.regs[MDL_INT_CTRL] = 0xE0;
    memset(mdl.out, 0, sizeof(mdl.out));
    mdl.measured = false;
}

static void mdl_measure(void) {
    for (int axis = 0; axis < 3; axis++) {
        const int16_t offset = (int16_t)(mdl.regs[MDL_OFFSET_X_L + 2 * axis] | (mdl.regs[MDL_OFFSET_X_L + 2 * axis + 1] << 8));
        long v = lround(mdl.mgauss[axis] / 1.5) - offset;
        mdl.out[axis] = v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : (int16_t)v;
    }
    mdl.measured = true;
}

static uint8_t mdl_read_reg(uint8_t reg) {
    const uint8_t md = mdl.regs[MDL_CFG_REG_A] & MDL_MD_MASK;
    if (reg == MDL_WHO_AM_I) {
        return 0x40;
    }
    if (reg == MDL_STATUS) {
        return md == MDL_MD_CONTINUOUS || mdl.measured ? 0x0f : 0x00;    // ZYXDA, X/Y/Z DA
    }
    if (reg >= MDL_OUTX_L && reg <= MDL_OUTZ_H) {
        if (reg == MDL_OUTX_L && md == MDL_MD_CONTINUOUS) {
            mdl_measure();          // a burst read from OUTX_L gets one measurement
        }
        const int16_t v = mdl.out[(reg - MDL_OUTX_L) / 2];
        return (reg - MDL_OUTX_L) & 1 ? (uint8_t)(v >> 8) : (uint8_t)v;
    }
    if (reg == MDL_TEMP_OUT_L || reg == MDL_TEMP_OUT_H) {
        return 0;                   // 25 degrees C
    }
    return reg < sizeof(mdl.regs) ? mdl.regs[reg] : 0;
}

static void mdl_start(void) {
    mdl.pointer_set = false;
}

static uint8_t mdl_rx(void) {
    const uint8_t v = mdl_read_reg(mdl.pointer);
    mdl.pointer = (mdl.pointer + 1) & 0x7f;
    return v;
}

static void mdl_tx(uint8_t data) {
    if (!mdl.pointer_set) {
        mdl.pointer = data & 0x7f;
        mdl.pointer_set = true;
        return;
    }
    const uint8_t reg = mdl.pointer;
    if (reg == MDL_CFG_REG_A && (data & MDL_SOFT_RST)) {
        mdl_reset();
    } else if (reg == MDL_CFG_REG_A) {
        data &= ~MDL_REBOOT;        // done at once
        if ((data & MDL_MD_MASK) == MDL_MD_SINGLE) {
            mdl_measure();          // one measurement, then idle
            data = (data & ~MDL_MD_MASK) | MDL_MD_IDLE;
        }
        mdl.regs[reg] = data;
    } else if (reg < sizeof(mdl.regs) && reg != MDL_WHO_AM_I && reg != MDL_STATUS && !(reg >= MDL_OUTX_L && reg <= MDL_TEMP_OUT_H)) {
        mdl.regs[reg] = data;
    }
    mdl.pointer = (mdl.pointer + 1) & 0x7f;
}

// --- EEPROM (0x50): the board's settings (hal/eeprom.c), 24C02-like ---
// 256 bytes, 8-bit word address, 8-byte write pages (the address wraps inside the
// page), sequential reads over the whole array. The write cycle time is not modelled
// (the firmware waits 10 ms after each page). Its content survives the reboots
// (galaxia_eeprom_image, saved by the page with the files).
#define EEPROM_SIZE 256
#define EEPROM_PAGE 8

// The factory content of a PCB 1.0.9: protocol 1, version 3 (THINGZ_VERSION_1_0_9),
// display 0, no compass calibration (0xFF: the firmware uses its defaults)
// TODO replace with the dump of a real 1.0.9 board
static uint8_t eeprom[EEPROM_SIZE] = {
    [0] = 1, [1] = 3, [2] = 0,
    [3 ... EEPROM_SIZE - 1] = 0xff,
};

static struct {
    uint8_t address;
    bool address_set;
} eep;

static void eeprom_start(void) {
    eep.address_set = false;
}

static uint8_t eeprom_rx(void) {
    return eeprom[eep.address++];
}

static void eeprom_tx(uint8_t data) {
    if (!eep.address_set) {
        eep.address = data;
        eep.address_set = true;
        return;
    }
    eeprom[eep.address] = data;
    eep.address = (eep.address & ~(EEPROM_PAGE - 1)) | ((eep.address + 1) & (EEPROM_PAGE - 1));
}

EMSCRIPTEN_KEEPALIVE
uint8_t *galaxia_eeprom_image(void) {
    return eeprom;
}

EMSCRIPTEN_KEEPALIVE
int galaxia_eeprom_size(void) {
    return EEPROM_SIZE;
}

static const struct i2c_device {

    uint8_t addr;
    void (*start_send)(void);
    uint8_t (*rx)(void);
    void (*tx)(uint8_t);
} i2c_devices[] = {
    { 0x19, lis_start, lis_rx, lis_tx },
    { 0x1E, mdl_start, mdl_rx, mdl_tx },
    { 0x50, eeprom_start, eeprom_rx, eeprom_tx },
};

// --- command links: recorded, then run by i2c_master_cmd_begin ---
enum { OP_START, OP_STOP, OP_WRITE, OP_READ };

struct galaxia_i2c_cmd {
    int count, size;
    struct { uint8_t op, byte; uint8_t *dest; } *ops;
};

static esp_err_t cmd_add(i2c_cmd_handle_t cmd, uint8_t op, uint8_t byte, uint8_t *dest) {
    if (cmd->count == cmd->size) {
        cmd->size = cmd->size ? cmd->size * 2 : 16;
        cmd->ops = realloc(cmd->ops, cmd->size * sizeof(*cmd->ops));
    }
    cmd->ops[cmd->count].op = op;
    cmd->ops[cmd->count].byte = byte;
    cmd->ops[cmd->count].dest = dest;
    cmd->count++;
    return ESP_OK;
}

esp_err_t i2c_param_config(i2c_port_t port, const i2c_config_t *conf) { return ESP_OK; }
esp_err_t i2c_driver_install(i2c_port_t port, i2c_mode_t mode, size_t rx, size_t tx, int flags) { return ESP_OK; }
esp_err_t i2c_driver_delete(i2c_port_t port) { return ESP_OK; }
esp_err_t i2c_set_timeout(i2c_port_t port, int timeout) { return ESP_OK; }

i2c_cmd_handle_t i2c_cmd_link_create(void) {
    return calloc(1, sizeof(struct galaxia_i2c_cmd));
}

void i2c_cmd_link_delete(i2c_cmd_handle_t cmd) {
    if (cmd) {
        free(cmd->ops);
        free(cmd);
    }
}

esp_err_t i2c_master_start(i2c_cmd_handle_t cmd) { return cmd_add(cmd, OP_START, 0, NULL); }
esp_err_t i2c_master_stop(i2c_cmd_handle_t cmd) { return cmd_add(cmd, OP_STOP, 0, NULL); }

esp_err_t i2c_master_write_byte(i2c_cmd_handle_t cmd, uint8_t data, bool ack_en) {
    return cmd_add(cmd, OP_WRITE, data, NULL);
}

esp_err_t i2c_master_write(i2c_cmd_handle_t cmd, const uint8_t *data, size_t len, bool ack_en) {
    for (size_t i = 0; i < len; i++) {
        cmd_add(cmd, OP_WRITE, data[i], NULL);
    }
    return ESP_OK;
}

esp_err_t i2c_master_read_byte(i2c_cmd_handle_t cmd, uint8_t *data, i2c_ack_type_t ack) {
    return cmd_add(cmd, OP_READ, 0, data);
}

esp_err_t i2c_master_read(i2c_cmd_handle_t cmd, uint8_t *data, size_t len, i2c_ack_type_t ack) {
    for (size_t i = 0; i < len; i++) {
        cmd_add(cmd, OP_READ, 0, data + i);
    }
    return ESP_OK;
}

esp_err_t i2c_master_cmd_begin(i2c_port_t port, i2c_cmd_handle_t cmd, TickType_t ticks) {
    const struct i2c_device *dev = NULL;
    bool addressing = false;
    for (int i = 0; i < cmd->count; i++) {
        switch (cmd->ops[i].op) {
            case OP_START:
                addressing = true;
                break;
            case OP_STOP:
                dev = NULL;
                break;
            case OP_WRITE:
                if (addressing) {
                    // Address byte: no device answers -> NACK, as on the bus
                    addressing = false;
                    dev = NULL;
                    for (size_t d = 0; d < MP_ARRAY_SIZE(i2c_devices); d++) {
                        if (i2c_devices[d].addr == cmd->ops[i].byte >> 1) {
                            dev = &i2c_devices[d];
                        }
                    }
                    if (!dev) {
                        return ESP_FAIL;
                    }
                    if (!(cmd->ops[i].byte & 1)) {
                        dev->start_send();
                    }
                } else if (dev) {
                    dev->tx(cmd->ops[i].byte);
                }
                break;
            case OP_READ:
                *cmd->ops[i].dest = dev ? dev->rx() : 0xff;
                break;
        }
    }
    return ESP_OK;
}

// Accelerometer, board axes as read by thingz.accelerometer (milli-g). The firmware
// reads board = (chip_y, -chip_x, -chip_z) (hal/accel_lis2dh12.c).
EMSCRIPTEN_KEEPALIVE
void galaxia_set_accel(int x, int y, int z) {
    lis.mg[0] = -y;
    lis.mg[1] = x;
    lis.mg[2] = -z;
}

// Magnetic field, magnetometer axes (milligauss)
EMSCRIPTEN_KEEPALIVE
void galaxia_set_magnetic(int x, int y, int z) {
    mdl.mgauss[0] = x;
    mdl.mgauss[1] = y;
    mdl.mgauss[2] = z;
}

// A calibration value of the EEPROM, as hal/magneto.c takes it: a float at a setting
// of the v1 map (shifted by one in the v0 map), the default if erased (0xFFFFFFFF) or NaN
static double eeprom_calibration(int setting, double fallback) {
    const int addr = eeprom[0] == 1 ? setting : setting - 1;
    float f;
    memcpy(&f, eeprom + addr, sizeof(f));
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits == 0xFFFFFFFF || isnan(f) ? fallback : (double)f;
}

// A compass heading as thingz.compass.heading() will report it: the field whose
// reading, once corrected by the calibration the firmware loads (hal/magneto.c,
// _load_calibration_data: the EEPROM's, else the default one, which is the QMC6310U's
// on a PCB 1.0.9), points to `heading` degrees, with a horizontal component `h` and a
// vertical one `v` in calibrated units. calibrated = (d - hardiron) x softiron (a row
// vector times the matrix), d = the LIS2MDL reading in milligauss with y and z negated.
EMSCRIPTEN_KEEPALIVE
void galaxia_set_compass(double heading, double h, double v) {
    static const double def_soft[3][3] = { { 1.601022, 0, 0 }, { -0.336627, 1.392834, 0 }, { 0, 0, 6.549427 } };
    static const double def_hard[3] = { -161.899996, -329.866672, -84.533339 };
    enum { HARD_X = 3, HARD_Y = 7, HARD_Z = 11, SOFT_M = 15 };      // hal/eeprom.c, v1 map
    double hard[3], s[3][3] = { { 0 } };
    hard[0] = eeprom_calibration(HARD_X, def_hard[0]);
    hard[1] = eeprom_calibration(HARD_Y, def_hard[1]);
    hard[2] = eeprom_calibration(HARD_Z, def_hard[2]);
    if (fabs(hard[0]) <= 0.0) {
        hard[0] = def_hard[0];
    }
    for (int i = 1; i < 3; i++) {
        if (fabs(hard[i]) <= 0.1) {
            hard[i] = def_hard[i];
        }
    }
    // the matrix is stored as its diagonal and [1][0], the other terms are 0
    for (int i = 0; i < 3; i++) {
        s[i][i] = eeprom_calibration(SOFT_M + 4 * i, def_soft[i][i]);
    }
    s[1][0] = eeprom_calibration(SOFT_M + 12, def_soft[1][0]);
    const double rad = heading * M_PI / 180.0;
    const double c[3] = { h * sin(rad), h * cos(rad), v };
    // c = m x s, s lower triangular: c2 = m2 s22, c1 = m1 s11, c0 = m0 s00 + m1 s10
    const double m2 = c[2] / s[2][2];
    const double m1 = c[1] / s[1][1];
    const double m0 = (c[0] - m1 * s[1][0]) / s[0][0];
    if (!isfinite(m0) || !isfinite(m1) || !isfinite(m2)) {
        return;     // a calibration the firmware cannot invert either: keep the field
    }
    const double d[3] = { m0 + hard[0], m1 + hard[1], m2 + hard[2] };
    galaxia_set_magnetic((int)lround(d[0]), (int)lround(-d[1]), (int)lround(-d[2]));
}

// ===================================================================== LED, light

#define LED_R 41
#define LED_G 40
#define LED_B 39
#define LED_COMMON 10
#define LEDC_CHANNELS 8
#define LEDC_FULL_DUTY 8192     // 13-bit duty

static struct {
    bool on;
    int gpio;
    uint32_t duty;
} ledc[LEDC_CHANNELS];

static int64_t light_t0;        // start of a light measurement (LED common set as input)

esp_err_t ledc_timer_config(const ledc_timer_config_t *cfg) { return ESP_OK; }
uint32_t ledc_get_freq(ledc_mode_t mode, ledc_timer_t timer) { return 2000; }

esp_err_t ledc_channel_config(const ledc_channel_config_t *cfg) {
    if (cfg->channel < 0 || cfg->channel >= LEDC_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }
    ledc[cfg->channel].on = true;
    ledc[cfg->channel].gpio = cfg->gpio_num;
    ledc[cfg->channel].duty = cfg->duty;
    return ESP_OK;
}

esp_err_t ledc_stop(ledc_mode_t mode, int channel, uint32_t idle_level) {
    if (channel >= 0 && channel < LEDC_CHANNELS) {
        ledc[channel].on = false;
    }
    return ESP_OK;
}

esp_err_t gpio_set_drive_capability(gpio_num_t pin, gpio_drive_cap_t strength) { return ESP_OK; }

static int led_level(int gpio) {
    for (int ch = 0; ch < LEDC_CHANNELS; ch++) {
        if (ledc[ch].on && ledc[ch].gpio == gpio) {
            uint32_t d = ledc[ch].duty > LEDC_FULL_DUTY ? LEDC_FULL_DUTY : ledc[ch].duty;
            return (d * 255 + LEDC_FULL_DUTY / 2) / LEDC_FULL_DUTY;
        }
    }
    return 0;
}

static void led_poll(void) {
    static int shown[3] = { -1, -1, -1 };
    const int rgb[3] = { led_level(LED_R), led_level(LED_G), led_level(LED_B) };
    if (memcmp(rgb, shown, sizeof(rgb))) {
        memcpy(shown, rgb, sizeof(rgb));
        galaxia_js_led(rgb[0], rgb[1], rgb[2]);
    }
}

void peripherals_gpio_direction(gpio_num_t pin, gpio_mode_t mode) {
    if (pin == LED_COMMON && mode == GPIO_MODE_INPUT) {
        light_t0 = esp_timer_get_time();
    }
}

esp_err_t adc1_config_width(adc_bits_width_t width) { return ESP_OK; }
esp_err_t adc1_config_channel_atten(adc1_channel_t channel, adc_atten_t atten) { return ESP_OK; }

// The LED, reverse charged, discharges through the photocurrent: faster in light.
// thingz_led maps a discharge of 500..8400 us to a light level of 255..0.
int adc1_get_raw(adc1_channel_t channel) {
    if (channel != ADC1_CHANNEL_9) {
        return 0;
    }
    const int light = galaxia_js_light();
    const int64_t discharge_us = 500 + (int64_t)(255 - light) * (8400 - 500) / 255;
    return esp_timer_get_time() - light_t0 < discharge_us ? 100 : 0;
}

// =========================================================================== DAC

struct galaxia_dac_continuous {
    uint32_t freq_hz;
    bool enabled;
};

esp_err_t dac_continuous_new_channels(const dac_continuous_config_t *cfg, dac_continuous_handle_t *out) {
    *out = calloc(1, sizeof(**out));
    (*out)->freq_hz = cfg->freq_hz;
    return ESP_OK;
}

esp_err_t dac_continuous_del_channels(dac_continuous_handle_t h) {
    if (h && h->enabled) {
        galaxia_js_sound_stop();
    }
    free(h);
    return ESP_OK;
}

esp_err_t dac_continuous_enable(dac_continuous_handle_t h) {
    h->enabled = true;
    return ESP_OK;
}

esp_err_t dac_continuous_disable(dac_continuous_handle_t h) {
    if (h->enabled) {
        galaxia_js_sound_stop();
    }
    h->enabled = false;
    return ESP_OK;
}

// A tone: the page loops the waveform at the DAC rate
esp_err_t dac_continuous_write_cyclically(dac_continuous_handle_t h, uint8_t *buf, size_t size, size_t *loaded) {
    galaxia_js_sound_loop(h->freq_hz, buf, size);
    if (loaded) {
        *loaded = size;
    }
    return ESP_OK;
}

// Samples (WAV): queued in the page, paced as the DAC would drain them
esp_err_t dac_continuous_write(dac_continuous_handle_t h, uint8_t *buf, size_t size, size_t *loaded, int timeout_ms) {
    galaxia_js_sound_chunk(h->freq_hz, buf, size);
    const int64_t until = esp_timer_get_time() + (int64_t)size * 1000000 / (h->freq_hz ? h->freq_hz : 8000);
    for (int64_t now; (now = esp_timer_get_time()) < until;) {
        galaxia_js_sleep((int)((until - now) / 1000) + 1);
    }
    if (loaded) {
        *loaded = size;
    }
    return ESP_OK;
}

// ===================================================================== others

QueueHandle_t xQueueCreateStatic(UBaseType_t len, UBaseType_t item_size, uint8_t *storage, StaticQueue_t *buf) {
    return xQueueCreate(len, item_size);
}

SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *buf) {
    return xSemaphoreCreateBinary();
}

// The log's SPIFFS partition: its files are in the Emscripten file system (/spiffs)
esp_err_t esp_spiffs_format(const char *partition_label) {
    EM_ASM({
        try {
            for (const name of FS.readdir("/spiffs")) {
                if (name !== "." && name !== "..") {
                    FS.unlink("/spiffs/" + name);
                }
            }
        } catch (e) { }
    });
    return ESP_OK;
}

// machine.Pin / machine.PWM: thingz_led creates them to set the LED pins up; here
// they are placeholders (the LED is driven through ledc_channel_config)
typedef struct {
    mp_obj_base_t base;
    mp_int_t value;
} placeholder_obj_t;

static mp_obj_t placeholder_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    placeholder_obj_t *self = mp_obj_malloc(placeholder_obj_t, type);
    self->value = n_args ? mp_obj_get_int(args[0]) : 0;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t placeholder_pin_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    return placeholder_make_new(type, n_args, n_kw, args);
}

static mp_obj_t placeholder_pwm_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    placeholder_obj_t *self = mp_obj_malloc(placeholder_obj_t, type);
    self->value = 0;
    return MP_OBJ_FROM_PTR(self);
}

MP_DEFINE_CONST_OBJ_TYPE(machine_pin_type, MP_QSTR_Pin, MP_TYPE_FLAG_NONE, make_new, placeholder_pin_make_new);
MP_DEFINE_CONST_OBJ_TYPE(machine_pwm_type, MP_QSTR_PWM, MP_TYPE_FLAG_NONE, make_new, placeholder_pwm_make_new);

// ===================================================================== polling

void peripherals_poll(void) {
    touch_poll();
    led_poll();
}
