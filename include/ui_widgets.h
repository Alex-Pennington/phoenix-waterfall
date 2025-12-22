/**
 * @file ui_widgets.h
 * @brief Simple UI widgets for waterfall settings panel
 */

#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include "ui_core.h"

/* Button widget */
typedef struct {
    int x, y, w, h;
    const char* label;
    bool enabled;
    bool hovered;
    bool pressed;
} widget_button_t;

/* Text input widget */
typedef struct {
    int x, y, w, h;
    char text[256];
    int cursor;
    int max_len;
    bool focused;
    bool numeric_only;
    const char* label;
} widget_input_t;

/* Slider widget */
typedef struct {
    int x, y, w, h;
    int min_val, max_val;
    int value;
    bool dragging;
    const char* label;
    const char* format;
} widget_slider_t;

/* Button functions */
void widget_button_init(widget_button_t* btn, int x, int y, int w, int h, const char* label);
bool widget_button_update(widget_button_t* btn, const mouse_state_t* mouse);
void widget_button_draw(widget_button_t* btn, ui_core_t* ui);

/* Text input functions */
void widget_input_init(widget_input_t* input, int x, int y, int w, int h, 
                       const char* label, int max_len, bool numeric);
void widget_input_set_text(widget_input_t* input, const char* text);
bool widget_input_update(widget_input_t* input, const mouse_state_t* mouse, SDL_Event* event);
void widget_input_draw(widget_input_t* input, ui_core_t* ui);

/* Slider functions */
void widget_slider_init(widget_slider_t* slider, int x, int y, int w, int h,
                        int min_val, int max_val, const char* label);
bool widget_slider_update(widget_slider_t* slider, const mouse_state_t* mouse);
void widget_slider_draw(widget_slider_t* slider, ui_core_t* ui);

/* Label drawing */
void widget_draw_label(ui_core_t* ui, const char* text, int x, int y, uint32_t color);

#endif /* UI_WIDGETS_H */
