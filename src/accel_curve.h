/*
 * Math for the tomkey pointer acceleration. Kept free of Zephyr
 * dependencies so that it can be tested on a host.
 */
#pragma once

#include <stdint.h>

/* Factors are in thousandths (1000 = 1.0x), speeds in counts per second. */
struct tomkey_accel_curve {
    uint32_t min_factor;
    uint32_t max_factor;
    uint32_t speed_threshold;
    uint32_t speed_max;
    uint8_t exponent;
};

/*
 * Leaky integrator over window_ms. A steady movement of r counts per second
 * converges to r, and events that arrive together in a burst only add their
 * distance instead of being divided by a near-zero interval.
 */
static inline uint32_t tomkey_accel_update_speed(uint32_t speed, uint32_t elapsed_ms,
                                                 uint32_t distance, uint32_t window_ms) {
    if (elapsed_ms >= window_ms) {
        speed = 0;
    } else {
        speed = (uint32_t)((uint64_t)speed * (window_ms - elapsed_ms) / window_ms);
    }
    return speed + (uint32_t)((uint64_t)distance * 1000 / window_ms);
}

static inline uint32_t tomkey_accel_factor(const struct tomkey_accel_curve *c, uint32_t speed) {
    if (speed <= c->speed_threshold || c->max_factor <= c->min_factor) {
        return c->min_factor;
    }
    if (speed >= c->speed_max) {
        return c->max_factor;
    }

    uint64_t t = (uint64_t)(speed - c->speed_threshold) * 1000 /
                 (c->speed_max - c->speed_threshold);
    uint64_t shaped = t;
    for (uint8_t i = 1; i < c->exponent; i++) {
        shaped = shaped * t / 1000;
    }

    return c->min_factor + (uint32_t)((uint64_t)(c->max_factor - c->min_factor) * shaped / 1000);
}

/* Scales value by factor / 1000 and carries the fraction in *remainder. */
static inline int32_t tomkey_accel_apply(int32_t value, uint32_t factor, int32_t *remainder) {
    int64_t total = (int64_t)value * factor + *remainder;
    int32_t out = (int32_t)(total / 1000);

    *remainder = (int32_t)(total - (int64_t)out * 1000);
    return out;
}
