/**
 * @file ui_widgets.c
 * @brief Simple UI widgets implementation
 */

#include "ui_widgets.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/*============================================================================
 * Button Widget
 *============================================================================*/

void widget_button_init(widget_button_t* btn, int x, int y, int w, int h, const char* label) {
    if (!btn) return;
    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;
    btn->label = label;
    btn->enabled = true;
    btn->hovered = false;
    btn->pressed = false;
}

bool widget_button_update(widget_button_t* btn, const mouse_state_t* mouse) {
    if (!btn || !mouse || !btn->enabled) {
        if (btn) btn->hovered = false;
        return false;
    }

    btn->hovered = ui_point_in_rect(mouse->x, mouse->y, btn->x, btn->y, btn->w, btn->h);

    if (btn->hovered && mouse->left_clicked) {
        btn->pressed = true;
    }
    
    if (btn->pressed && mouse->left_released) {
        btn->pressed = false;
        if (btn->hovered) {
            return true;  /* Clicked */
        }
    }
    
    if (!mouse->left_down) {
        btn->pressed = false;
    }

    return false;
}

void widget_button_draw(widget_button_t* btn, ui_core_t* ui) {
    if (!btn || !ui) return;

    uint32_t bg_color;
    if (!btn->enabled) {
        bg_color = COLOR_BG_WIDGET;
    } else if (btn->pressed) {
        bg_color = COLOR_BUTTON_ACTIVE;
    } else if (btn->hovered) {
        bg_color = COLOR_BUTTON_HOVER;
    } else {
        bg_color = COLOR_BUTTON;
    }

    ui_draw_rect(ui, btn->x, btn->y, btn->w, btn->h, bg_color);
    ui_draw_rect_outline(ui, btn->x, btn->y, btn->w, btn->h, 
                         btn->enabled ? COLOR_ACCENT_DIM : COLOR_TEXT_DIM);

    if (btn->label) {
        uint32_t text_color = btn->enabled ? COLOR_TEXT : COLOR_TEXT_DIM;
        ui_draw_text_centered(ui, ui->font_normal, btn->label,
                              btn->x, btn->y + (btn->h - 14) / 2, btn->w, text_color);
    }
}

/*============================================================================
 * Text Input Widget
 *============================================================================*/

void widget_input_init(widget_input_t* input, int x, int y, int w, int h,
                       const char* label, int max_len, bool numeric) {
    if (!input) return;
    memset(input, 0, sizeof(*input));
    input->x = x;
    input->y = y;
    input->w = w;
    input->h = h;
    input->label = label;
    input->max_len = (max_len < 255) ? max_len : 255;
    input->numeric_only = numeric;
    input->cursor = 0;
}

void widget_input_set_text(widget_input_t* input, const char* text) {
    if (!input || !text) return;
    strncpy(input->text, text, sizeof(input->text) - 1);
    input->text[sizeof(input->text) - 1] = '\0';
    input->cursor = (int)strlen(input->text);
}

bool widget_input_update(widget_input_t* input, const mouse_state_t* mouse, SDL_Event* event) {
    if (!input || !mouse) return false;

    bool in_bounds = ui_point_in_rect(mouse->x, mouse->y, input->x, input->y, input->w, input->h);

    /* Focus on click */
    if (mouse->left_clicked) {
        input->focused = in_bounds;
    }

    /* Handle keyboard input when focused */
    if (input->focused && event && event->type == SDL_TEXTINPUT) {
        char c = event->text.text[0];
        if (input->numeric_only && !isdigit(c) && c != '.') {
            return false;
        }
        int len = (int)strlen(input->text);
        if (len < input->max_len) {
            input->text[input->cursor] = c;
            input->cursor++;
            input->text[input->cursor] = '\0';
            return true;  /* Text changed */
        }
    }

    if (input->focused && event && event->type == SDL_KEYDOWN) {
        switch (event->key.keysym.sym) {
            case SDLK_BACKSPACE:
                if (input->cursor > 0) {
                    input->cursor--;
                    memmove(&input->text[input->cursor], &input->text[input->cursor + 1],
                            strlen(&input->text[input->cursor + 1]) + 1);
                    return true;
                }
                break;
            case SDLK_DELETE:
                if (input->text[input->cursor]) {
                    memmove(&input->text[input->cursor], &input->text[input->cursor + 1],
                            strlen(&input->text[input->cursor + 1]) + 1);
                    return true;
                }
                break;
            case SDLK_LEFT:
                if (input->cursor > 0) input->cursor--;
                break;
            case SDLK_RIGHT:
                if (input->text[input->cursor]) input->cursor++;
                break;
            case SDLK_HOME:
                input->cursor = 0;
                break;
            case SDLK_END:
                input->cursor = (int)strlen(input->text);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                input->focused = false;
                return true;
            case SDLK_ESCAPE:
                input->focused = false;
                break;
        }
    }

    return false;
}

void widget_input_draw(widget_input_t* input, ui_core_t* ui) {
    if (!input || !ui) return;

    /* Draw label */
    if (input->label) {
        ui_draw_text(ui, ui->font_small, input->label, input->x, input->y - 16, COLOR_TEXT_DIM);
    }

    /* Draw background */
    ui_draw_rect(ui, input->x, input->y, input->w, input->h, COLOR_INPUT_BG);
    ui_draw_rect_outline(ui, input->x, input->y, input->w, input->h,
                         input->focused ? COLOR_INPUT_FOCUS : COLOR_INPUT_BORDER);

    /* Draw text */
    int text_x = input->x + 4;
    int text_y = input->y + (input->h - 14) / 2;
    ui_draw_text(ui, ui->font_normal, input->text, text_x, text_y, COLOR_TEXT);

    /* Draw cursor when focused */
    if (input->focused) {
        int cursor_x = text_x;
        if (input->cursor > 0) {
            char temp[256];
            strncpy(temp, input->text, input->cursor);
            temp[input->cursor] = '\0';
            int tw, th;
            ui_get_text_size(ui->font_normal, temp, &tw, &th);
            cursor_x += tw;
        }
        /* Blink cursor */
        if ((SDL_GetTicks() / 500) % 2 == 0) {
            ui_draw_rect(ui, cursor_x, text_y, 2, 14, COLOR_TEXT);
        }
    }
}

/*============================================================================
 * Slider Widget
 *============================================================================*/

void widget_slider_init(widget_slider_t* slider, int x, int y, int w, int h,
                        int min_val, int max_val, const char* label) {
    if (!slider) return;
    slider->x = x;
    slider->y = y;
    slider->w = w;
    slider->h = h;
    slider->min_val = min_val;
    slider->max_val = max_val;
    slider->value = min_val;
    slider->dragging = false;
    slider->label = label;
    slider->format = "%d";
}

bool widget_slider_update(widget_slider_t* slider, const mouse_state_t* mouse) {
    if (!slider || !mouse) return false;

    bool in_bounds = ui_point_in_rect(mouse->x, mouse->y, slider->x, slider->y, slider->w, slider->h);
    int old_value = slider->value;

    if (in_bounds && mouse->left_clicked) {
        slider->dragging = true;
    }

    if (!mouse->left_down) {
        slider->dragging = false;
    }

    if (slider->dragging) {
        float ratio = (float)(mouse->x - slider->x) / (float)slider->w;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        slider->value = slider->min_val + (int)(ratio * (slider->max_val - slider->min_val));
    }

    /* Mouse wheel */
    if (in_bounds && mouse->wheel_y != 0) {
        int step = (slider->max_val - slider->min_val) / 20;
        if (step < 1) step = 1;
        slider->value += mouse->wheel_y * step;
        if (slider->value < slider->min_val) slider->value = slider->min_val;
        if (slider->value > slider->max_val) slider->value = slider->max_val;
    }

    return slider->value != old_value;
}

void widget_slider_draw(widget_slider_t* slider, ui_core_t* ui) {
    if (!slider || !ui) return;

    /* Draw label */
    if (slider->label) {
        ui_draw_text(ui, ui->font_small, slider->label, slider->x, slider->y - 16, COLOR_TEXT_DIM);
    }

    /* Draw track */
    ui_draw_rect(ui, slider->x, slider->y, slider->w, slider->h, COLOR_SLIDER_BG);
    ui_draw_rect_outline(ui, slider->x, slider->y, slider->w, slider->h, COLOR_ACCENT_DIM);

    /* Draw fill */
    float ratio = (float)(slider->value - slider->min_val) / (float)(slider->max_val - slider->min_val);
    int fill_w = (int)(ratio * slider->w);
    if (fill_w > 0) {
        ui_draw_rect(ui, slider->x, slider->y + 2, fill_w, slider->h - 4, COLOR_SLIDER_FG);
    }

    /* Draw value */
    char value_str[32];
    snprintf(value_str, sizeof(value_str), slider->format ? slider->format : "%d", slider->value);
    ui_draw_text_centered(ui, ui->font_small, value_str,
                          slider->x, slider->y + (slider->h - 12) / 2, slider->w, COLOR_TEXT);
}

/*============================================================================
 * Label
 *============================================================================*/

void widget_draw_label(ui_core_t* ui, const char* text, int x, int y, uint32_t color) {
    if (!ui || !text) return;
    ui_draw_text(ui, ui->font_normal, text, x, y, color);
}
