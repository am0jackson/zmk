/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * PATCHED for bb9900 trackpad: hold the key bound to `&mo SCROLL` (SELECT)
 * to turn trackpad motion into scrolling instead of cursor movement.
 * Drop this file in over: app/src/mouse/hid_input_listener.c
 */

#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zmk/mouse.h>
#include <zmk/mouse/input_config.h>
#include <zmk/endpoints.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>          /* ADDED: for zmk_keymap_layer_active() */

/* --- bb9900 trackpad scroll-on-layer (ADDED) --------------------------------
 * BB_SCROLL_LAYER must match the index of `scroll_layer` in bb9900.keymap.
 * BB_SCROLL_DIV  = how many trackpad units make one scroll tick.
 *                  Larger  = slower scrolling, Smaller = faster. Tune to taste.
 * -------------------------------------------------------------------------- */
#define BB_SCROLL_LAYER 3
#define BB_SCROLL_DIV   12

void handle_rel_code(struct input_event *evt) {
    /* persist remainder across events so small moves still scroll eventually */
    static int16_t scroll_acc_x = 0;
    static int16_t scroll_acc_y = 0;

    if (zmk_keymap_layer_active(BB_SCROLL_LAYER)) {
        switch (evt->code) {
        case INPUT_REL_X:
            scroll_acc_x += evt->value;
            if (scroll_acc_x / BB_SCROLL_DIV != 0) {
                int8_t t = (int8_t)(scroll_acc_x / BB_SCROLL_DIV);
                zmk_hid_mouse_scroll_update(t, 0);              /* horizontal */
                scroll_acc_x -= (int16_t)t * BB_SCROLL_DIV;
            }
            break;
        case INPUT_REL_Y:
            scroll_acc_y += evt->value;
            if (scroll_acc_y / BB_SCROLL_DIV != 0) {
                int8_t t = (int8_t)(scroll_acc_y / BB_SCROLL_DIV);
                /* negate so pushing the pad up scrolls the page up (natural).
                 * Remove the minus if you prefer the opposite direction.     */
                zmk_hid_mouse_scroll_update(0, (int8_t)(-t));   /* vertical */
                scroll_acc_y -= (int16_t)t * BB_SCROLL_DIV;
            }
            break;
        case INPUT_REL_WHEEL:
            zmk_hid_mouse_scroll_update(0, evt->value);
            break;
        case INPUT_REL_HWHEEL:
            zmk_hid_mouse_scroll_update(evt->value, 0);
            break;
        default:
            break;
        }
        return;
    }

    /* --- original behaviour: trackpad moves the cursor --- */
    switch (evt->code) {
    case INPUT_REL_X:
        zmk_hid_mouse_movement_update(evt->value, 0);
        break;
    case INPUT_REL_Y:
        zmk_hid_mouse_movement_update(0, evt->value);
        break;
    case INPUT_REL_WHEEL:
        zmk_hid_mouse_scroll_update(0, evt->value);
        break;
    case INPUT_REL_HWHEEL:
        zmk_hid_mouse_scroll_update(evt->value, 0);
        break;
    default:
        break;
    }
}

void handle_key_code(struct input_event *evt) {
    int8_t btn;

    switch (evt->code) {
    case INPUT_BTN_0:
    case INPUT_BTN_1:
    case INPUT_BTN_2:
    case INPUT_BTN_3:
    case INPUT_BTN_4:
        btn = evt->code - INPUT_BTN_0;
        if (evt->value > 0) {
            zmk_hid_mouse_button_press(btn);
        } else {
            zmk_hid_mouse_button_release(btn);
        }
        break;
    default:
        break;
    }
}

static void swap_xy(struct input_event *evt) {
    switch (evt->code) {
    case INPUT_REL_X:
        evt->code = INPUT_REL_Y;
        break;
    case INPUT_REL_Y:
        evt->code = INPUT_REL_X;
        break;
    }
}

static void filter_with_input_config(struct input_event *evt) {
    if (!evt->dev) {
        return;
    }

    const struct zmk_input_config *cfg = zmk_input_config_get_for_device(evt->dev);

    if (!cfg) {
        return;
    }

    if (cfg->xy_swap) {
        swap_xy(evt);
    }

    if ((cfg->x_invert && evt->code == INPUT_REL_X) ||
        (cfg->y_invert && evt->code == INPUT_REL_Y)) {
        evt->value = -(evt->value);
    }

    evt->value = (int16_t)((evt->value * cfg->scale_multiplier) / cfg->scale_divisor);
}

void input_handler(struct input_event *evt) {
    // First, filter to update the event data as needed.
    filter_with_input_config(evt);

    switch (evt->type) {
    case INPUT_EV_REL:
        handle_rel_code(evt);
        break;
    case INPUT_EV_KEY:
        handle_key_code(evt);
        break;
    }

    if (evt->sync) {
        zmk_endpoints_send_mouse_report();
        zmk_hid_mouse_scroll_set(0, 0);
        zmk_hid_mouse_movement_set(0, 0);
    }
}

INPUT_CALLBACK_DEFINE(NULL, input_handler);
