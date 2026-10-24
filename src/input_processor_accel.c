/*
 * Velocity based pointer acceleration for the trackball.
 *
 * The trackball sits on the peripheral, so its events reach the central
 * over the split link, and several of them can arrive within the same
 * millisecond. Estimating speed from the gap between single events would
 * then see very high speeds and jerk the cursor. This processor estimates
 * speed from the movement within a short window instead, which stays
 * smooth under such bursts.
 */

#define DT_DRV_COMPAT zmk_input_processor_tomkey_accel

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <drivers/input_processor.h>

#include "accel_curve.h"

struct accel_config {
    struct tomkey_accel_curve curve;
    uint32_t window_ms;
};

struct accel_data {
    int64_t last_ms;
    uint32_t speed;
    int32_t remainders[2];
};

static int accel_handle_event(const struct device *dev, struct input_event *event,
                              uint32_t param1, uint32_t param2,
                              struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const struct accel_config *cfg = dev->config;
    struct accel_data *data = dev->data;
    int64_t now = k_uptime_get();
    uint32_t elapsed = (uint32_t)MIN(now - data->last_ms, (int64_t)cfg->window_ms);

    data->last_ms = now;
    data->speed =
        tomkey_accel_update_speed(data->speed, elapsed, (uint32_t)abs(event->value), cfg->window_ms);

    if (event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    uint32_t factor = tomkey_accel_factor(&cfg->curve, data->speed);
    int32_t *remainder = &data->remainders[event->code == INPUT_REL_X ? 0 : 1];

    event->value = tomkey_accel_apply(event->value, factor, remainder);
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api accel_driver_api = {
    .handle_event = accel_handle_event,
};

#define ACCEL_INST(n)                                                                              \
    BUILD_ASSERT(DT_INST_PROP(n, speed_window_ms) > 0, "speed-window-ms must be positive");        \
    BUILD_ASSERT(DT_INST_PROP(n, acceleration_exponent) >= 1 &&                                    \
                     DT_INST_PROP(n, acceleration_exponent) <= 4,                                  \
                 "acceleration-exponent must be 1 to 4");                                          \
    static const struct accel_config accel_config_##n = {                                          \
        .curve =                                                                                   \
            {                                                                                      \
                .min_factor = DT_INST_PROP(n, min_factor),                                         \
                .max_factor = DT_INST_PROP(n, max_factor),                                         \
                .speed_threshold = DT_INST_PROP(n, speed_threshold),                               \
                .speed_max = DT_INST_PROP(n, speed_max),                                           \
                .exponent = DT_INST_PROP(n, acceleration_exponent),                                \
            },                                                                                     \
        .window_ms = DT_INST_PROP(n, speed_window_ms),                                             \
    };                                                                                             \
    static struct accel_data accel_data_##n;                                                       \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &accel_data_##n, &accel_config_##n, POST_KERNEL,          \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &accel_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ACCEL_INST)
