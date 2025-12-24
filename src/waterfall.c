/**
 * @file waterfall.c
 * @brief Waterfall display for sdr_server I/Q stream
 *
 * DISPLAY CHAIN ONLY - No detection logic here.
 * Detection (tick, marker, BCD) lives in phoenix-detector module.
 *
 * Connects to sdr_server via phoenix-discovery for raw I/Q stream.
 * Uses phoenix-dsp for decimation/filtering, phoenix-kiss-fft for FFT.
 *
 * HOT PATH (per-frame with samples):
 *   1. Receive IQDQ frames from sdr_server (PHXI/IQDQ protocol)
 *   2. Convert samples to float32 (S16/F32/U8 formats supported)
 *   3. Decimate from 2 MHz to 12 kHz display rate
 *   4. Accumulate in circular buffer
 *   5. Apply window function and compute FFT
 *   6. Calculate magnitudes and map to screen pixels
 *   7. Auto-gain tracking (attack/decay)
 *   8. Scroll waterfall and draw new row
 *   9. Render to screen
 *
 * Features:
 *   - Auto-discovery and auto-connect to sdr_server
 *   - Auto-reconnect on disconnect (5 second retry)
 *   - PHXI/IQDQ protocol with sequence tracking
 *   - Sample format conversion (S16/F32/U8)
 *   - Decimation (2 MSPS → 12 kHz)
 *   - Settings panel (Tab key)
 *   - Resizable window
 *   - Gain adjustment (+/- keys)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <SDL.h>
#include "kiss_fft.h"
#include "version.h"
#include "pn_discovery.h"
#include "pn_dsp.h"

#ifdef HAS_GUI
#include "ui_core.h"
#include "ui_widgets.h"
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define SOCKET_INVALID INVALID_SOCKET
#define socket_close closesocket
#define socket_errno WSAGetLastError()
#define EWOULDBLOCK_VAL WSAEWOULDBLOCK
#define ETIMEDOUT_VAL WSAETIMEDOUT
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
typedef int socket_t;
#define SOCKET_INVALID (-1)
#define socket_close close
#define socket_errno errno
#define EWOULDBLOCK_VAL EWOULDBLOCK
#define ETIMEDOUT_VAL ETIMEDOUT
#define Sleep(ms) usleep((ms) * 1000)
#endif

typedef enum {
    RECV_OK = 0,
    RECV_TIMEOUT,
    RECV_ERROR
} recv_result_t;

/*============================================================================
 * SDR Server Protocol (PHXI/IQDQ)
 *============================================================================*/

#define MAGIC_PHXI  0x50485849  // "PHXI" - stream header magic
#define MAGIC_IQDQ  0x49514451  // "IQDQ" - data frame magic
#define MAGIC_META  0x4D455441  // "META" - metadata frame magic

/* Sample format types */
#define SAMPLE_FORMAT_S16  1  // int16_t (2 bytes per I or Q)
#define SAMPLE_FORMAT_F32  2  // float (4 bytes per I or Q)
#define SAMPLE_FORMAT_U8   3  // uint8_t (1 byte per I or Q)

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;           // MAGIC_PHXI (0x50485849)
    uint32_t version;         // Protocol version (currently 1)
    uint32_t sample_rate;     // Samples per second
    uint32_t sample_format;   // 1=S16, 2=F32, 3=U8
    uint32_t center_freq_lo;  // Center frequency (low 32 bits)
    uint32_t center_freq_hi;  // Center frequency (high 32 bits)
    int32_t  gain_reduction;  // Gain reduction in 0.1 dB (e.g., 250 = 25.0 dB)
    uint32_t lna_state;       // LNA state (1=enabled, 0=disabled)
} phxi_stream_header_t;  // 32 bytes total

typedef struct {
    uint32_t magic;       // MAGIC_IQDQ (0x49514451)
    uint32_t sequence;    // Sequence number (wraps at UINT32_MAX)
    uint32_t num_samples; // Number of I/Q pairs in this frame
    uint32_t flags;       // Reserved for future use
} iqdq_data_frame_t;  // 16 bytes header + sample data

typedef struct {
    uint32_t magic;           // MAGIC_META (0x4D455441)
    uint32_t sequence;        // Sequence number
    uint32_t center_freq_lo;  // Updated center frequency (low 32 bits)
    uint32_t center_freq_hi;  // Updated center frequency (high 32 bits)
    int32_t  gain_reduction;  // Updated gain reduction
    uint32_t lna_state;       // Updated LNA state
    uint32_t reserved[2];     // Reserved for future use
} meta_update_t;  // 32 bytes total
#pragma pack(pop)

/*============================================================================
 * Configuration
 *============================================================================*/

#define DEFAULT_RELAY_HOST      "localhost"
#define DEFAULT_RELAY_PORT      4536  /* sdr_server data port */
#define CONFIG_FILE             "waterfall.ini"

#define DISPLAY_SAMPLE_RATE     12000
#define DISPLAY_FFT_SIZE        2048
#define DISPLAY_OVERLAP         1024
#define DISPLAY_HZ_PER_BIN      ((float)DISPLAY_SAMPLE_RATE / DISPLAY_FFT_SIZE)
#define ZOOM_MAX_HZ             5000.0f

#define DEFAULT_WINDOW_WIDTH    1024
#define DEFAULT_WINDOW_HEIGHT   600
#define MIN_WINDOW_WIDTH        400
#define MIN_WINDOW_HEIGHT       300

#define PANEL_WIDTH             250
#define PANEL_HEIGHT            220

/*============================================================================
 * Global State
 *============================================================================*/

/* Connection */
static socket_t g_socket = SOCKET_INVALID;
static char g_relay_host[256] = DEFAULT_RELAY_HOST;
static int g_relay_port = DEFAULT_RELAY_PORT;
static bool g_connected = false;
static uint32_t g_sample_rate = DISPLAY_SAMPLE_RATE;

/* Discovery */
static bool g_discovery_enabled = true;
static char g_node_id[64] = "WATERFALL-1";
static bool g_auto_connect = true;  /* Auto-connect to discovered services */

/* Thread-safe discovery state (set by callback, read by main loop) */
static volatile bool g_service_discovered = false;
static char g_discovered_ip[64] = {0};
static int g_discovered_port = 0;

/* Protocol state */
static uint32_t g_sample_format = SAMPLE_FORMAT_F32;
static uint32_t g_last_sequence = 0;
static uint8_t *g_raw_buffer = NULL;
static int g_raw_buffer_size = 0;

/* Decimation (2 MSPS → 12 kHz) */
static pn_decimate_t g_decimator_i;
static pn_decimate_t g_decimator_q;

/* Auto-reconnect */
#define RECONNECT_INTERVAL_MS 5000
static uint32_t g_last_reconnect_time = 0;

/* Window */
static int g_window_width = DEFAULT_WINDOW_WIDTH;
static int g_window_height = DEFAULT_WINDOW_HEIGHT;
static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static uint8_t *g_pixels = NULL;

/* FFT */
static kiss_fft_cfg g_fft_cfg = NULL;
static kiss_fft_cpx *g_fft_in = NULL;
static kiss_fft_cpx *g_fft_out = NULL;
static float *g_window_func = NULL;
static float *g_magnitudes = NULL;

/* I/Q buffer */
typedef struct { float i, q; } iq_sample_t;
static iq_sample_t *g_iq_buffer = NULL;
static int g_iq_buffer_idx = 0;
static int g_new_samples = 0;

/* Display */
static float g_peak_db = -40.0f;
static float g_floor_db = -80.0f;
static float g_gain_offset = 0.0f;
#define AGC_ATTACK  0.05f
#define AGC_DECAY   0.002f

/* Settings panel */
static bool g_show_settings = false;

#ifdef HAS_GUI
static ui_core_t *g_ui = NULL;
static widget_input_t g_input_host;
static widget_input_t g_input_port;
static widget_slider_t g_slider_gain;
static widget_button_t g_btn_connect;
#endif

/*============================================================================
 * Config File
 *============================================================================*/

static void load_config(void) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64], value[192];
        if (sscanf(line, "%63[^=]=%191s", key, value) == 2) {
            if (strcmp(key, "host") == 0) {
                strncpy(g_relay_host, value, sizeof(g_relay_host) - 1);
            } else if (strcmp(key, "port") == 0) {
                g_relay_port = atoi(value);
            } else if (strcmp(key, "width") == 0) {
                g_window_width = atoi(value);
                if (g_window_width < MIN_WINDOW_WIDTH) g_window_width = MIN_WINDOW_WIDTH;
            } else if (strcmp(key, "height") == 0) {
                g_window_height = atoi(value);
                if (g_window_height < MIN_WINDOW_HEIGHT) g_window_height = MIN_WINDOW_HEIGHT;
            } else if (strcmp(key, "gain") == 0) {
                g_gain_offset = (float)atof(value);
            }
        }
    }
    fclose(f);
    printf("Loaded config from %s\n", CONFIG_FILE);
}

static void save_config(void) {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) return;

    fprintf(f, "; Phoenix Waterfall Configuration\n");
    fprintf(f, "host=%s\n", g_relay_host);
    fprintf(f, "port=%d\n", g_relay_port);
    fprintf(f, "width=%d\n", g_window_width);
    fprintf(f, "height=%d\n", g_window_height);
    fprintf(f, "gain=%.1f\n", g_gain_offset);
    fclose(f);
}

/*============================================================================
 * Window Function (Blackman-Harris)
 * Called once at startup - reduces spectral leakage in FFT
 *============================================================================*/

static void generate_blackman_harris(float *window, int size) {
    const float a0 = 0.35875f, a1 = 0.48829f, a2 = 0.14128f, a3 = 0.01168f;
    const float pi = 3.14159265358979323846f;
    for (int i = 0; i < size; i++) {
        float n = (float)i / (float)(size - 1);
        window[i] = a0 - a1*cosf(2*pi*n) + a2*cosf(4*pi*n) - a3*cosf(6*pi*n);
    }
}

/*============================================================================
 * TCP Helpers
 *============================================================================*/

static bool tcp_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

static void tcp_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static socket_t tcp_connect(const char *host, int port) {
    struct addrinfo hints, *result, *rp;
    char port_str[16];
    socket_t sock = SOCKET_INVALID;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &result) != 0) return SOCKET_INVALID;

    for (rp = result; rp; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == SOCKET_INVALID) continue;
        if (connect(sock, rp->ai_addr, (int)rp->ai_addrlen) == 0) break;
        socket_close(sock);
        sock = SOCKET_INVALID;
    }
    freeaddrinfo(result);
    return sock;
}

static recv_result_t tcp_recv_exact(socket_t sock, void *buf, int n) {
    char *ptr = (char*)buf;
    int remaining = n;
    while (remaining > 0) {
        int received = recv(sock, ptr, remaining, 0);
        if (received > 0) {
            ptr += received;
            remaining -= received;
        } else if (received == 0) {
            return RECV_ERROR;
        } else {
            int err = socket_errno;
            if (err == EWOULDBLOCK_VAL || err == ETIMEDOUT_VAL) return RECV_TIMEOUT;
            return RECV_ERROR;
        }
    }
    return RECV_OK;
}

/*============================================================================
 * Connection Management
 *============================================================================*/

static void disconnect_from_relay(void) {
    if (g_socket != SOCKET_INVALID) {
        socket_close(g_socket);
        g_socket = SOCKET_INVALID;
    }
    g_connected = false;
    g_last_reconnect_time = SDL_GetTicks();  /* Record disconnect time for retry */
}

static bool connect_to_relay(void) {
    if (g_connected) return true;

    printf("Connecting to %s:%d...\n", g_relay_host, g_relay_port);
    g_socket = tcp_connect(g_relay_host, g_relay_port);
    if (g_socket == SOCKET_INVALID) {
        printf("Connection failed\n");
        g_last_reconnect_time = SDL_GetTicks();
        return false;
    }

#ifdef _WIN32
    DWORD timeout = 5000;
    setsockopt(g_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(g_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    /* Read PHXI stream header (32 bytes) */
    phxi_stream_header_t header;
    if (tcp_recv_exact(g_socket, &header, sizeof(header)) != RECV_OK) {
        printf("Failed to receive header\n");
        socket_close(g_socket);
        g_socket = SOCKET_INVALID;
        g_last_reconnect_time = SDL_GetTicks();
        return false;
    }

    if (header.magic != MAGIC_PHXI) {
        printf("Invalid header magic: 0x%08X (expected PHXI 0x%08X)\n", header.magic, MAGIC_PHXI);
        socket_close(g_socket);
        g_socket = SOCKET_INVALID;
        g_last_reconnect_time = SDL_GetTicks();
        return false;
    }

    /* Extract stream parameters */
    g_sample_rate = header.sample_rate;
    g_sample_format = header.sample_format;
    g_last_sequence = 0;  /* Reset sequence tracking */

    const char *format_str = (g_sample_format == SAMPLE_FORMAT_S16) ? "S16" :
                            (g_sample_format == SAMPLE_FORMAT_F32) ? "F32" :
                            (g_sample_format == SAMPLE_FORMAT_U8) ? "U8" : "Unknown";
    printf("Connected: %u Hz %s I/Q stream\n", g_sample_rate, format_str);

    /* Initialize decimation (e.g., 2 MHz → 12 kHz = factor ~167) */
    int decimation_factor = g_sample_rate / DISPLAY_SAMPLE_RATE;
    if (decimation_factor < 1) decimation_factor = 1;
    printf("Decimation: %d:1 (%u Hz → %u Hz)\n", decimation_factor, g_sample_rate, DISPLAY_SAMPLE_RATE);
    
    pn_decimate_init(&g_decimator_i, decimation_factor, (float)g_sample_rate);
    pn_decimate_init(&g_decimator_q, decimation_factor, (float)g_sample_rate);

#ifdef _WIN32
    timeout = 100;
    setsockopt(g_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    tv.tv_sec = 0; tv.tv_usec = 100000;
    setsockopt(g_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    g_connected = true;
    return true;
}

/*============================================================================
 * Window/Buffer Management
 *============================================================================*/

static bool resize_buffers(void) {
    free(g_pixels);
    g_pixels = (uint8_t*)calloc(g_window_width * g_window_height * 3, 1);
    if (!g_pixels) return false;

    free(g_magnitudes);
    g_magnitudes = (float*)malloc(g_window_width * sizeof(float));
    if (!g_magnitudes) return false;

    if (g_texture) SDL_DestroyTexture(g_texture);
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   g_window_width, g_window_height);
    return g_texture != NULL;
}

/*============================================================================
 * Color Mapping (HOT PATH - called per pixel)
 * Converts magnitude to RGB using blue→cyan→green→yellow→red gradient
 *============================================================================*/

static void magnitude_to_rgb(float mag, float peak_db, float floor_db,
                              uint8_t *r, uint8_t *g, uint8_t *b) {
    float db = 20.0f * log10f(mag + 1e-10f) + g_gain_offset;
    float range = peak_db - floor_db;
    if (range < 20.0f) range = 20.0f;
    float norm = (db - floor_db) / range;
    if (norm < 0.0f) norm = 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    if (norm < 0.25f) {
        *r = 0; *g = 0; *b = (uint8_t)(norm * 4.0f * 255.0f);
    } else if (norm < 0.5f) {
        *r = 0; *g = (uint8_t)((norm - 0.25f) * 4.0f * 255.0f); *b = 255;
    } else if (norm < 0.75f) {
        *r = (uint8_t)((norm - 0.5f) * 4.0f * 255.0f);
        *g = 255;
        *b = (uint8_t)((0.75f - norm) * 4.0f * 255.0f);
    } else {
        *r = 255; *g = (uint8_t)((1.0f - norm) * 4.0f * 255.0f); *b = 0;
    }
}

/*============================================================================
 * Settings Panel
 *============================================================================*/

#ifdef HAS_GUI
static void init_settings_panel(void) {
    int panel_x = (g_window_width - PANEL_WIDTH) / 2;
    int panel_y = (g_window_height - PANEL_HEIGHT) / 2;
    int x = panel_x + 15;
    int y = panel_y + 40;

    widget_input_init(&g_input_host, x, y, 220, 24, "Host", 64, false);
    widget_input_set_text(&g_input_host, g_relay_host);
    y += 50;

    widget_input_init(&g_input_port, x, y, 80, 24, "Port", 6, true);
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", g_relay_port);
    widget_input_set_text(&g_input_port, port_str);
    y += 50;

    widget_slider_init(&g_slider_gain, x, y, 220, 20, -30, 30, "Gain (dB)");
    g_slider_gain.value = (int)g_gain_offset;
    g_slider_gain.format = "%+d dB";
    y += 45;

    widget_button_init(&g_btn_connect, x, y, 100, 28, "Connect");
}

static void update_settings_panel(mouse_state_t *mouse, SDL_Event *event) {
    if (!g_show_settings) return;

    /* Update widgets */
    if (widget_input_update(&g_input_host, mouse, event)) {
        strncpy(g_relay_host, g_input_host.text, sizeof(g_relay_host) - 1);
    }
    if (widget_input_update(&g_input_port, mouse, event)) {
        g_relay_port = atoi(g_input_port.text);
        if (g_relay_port <= 0) g_relay_port = DEFAULT_RELAY_PORT;
    }
    if (widget_slider_update(&g_slider_gain, mouse)) {
        g_gain_offset = (float)g_slider_gain.value;
    }

    if (widget_button_update(&g_btn_connect, mouse)) {
        if (g_connected) {
            disconnect_from_relay();
        } else {
            connect_to_relay();
        }
        save_config();
    }

    /* Update button labels */
    g_btn_connect.label = g_connected ? "Disconnect" : "Connect";
}

static void draw_settings_panel(void) {
    if (!g_show_settings || !g_ui) return;

    int panel_x = (g_window_width - PANEL_WIDTH) / 2;
    int panel_y = (g_window_height - PANEL_HEIGHT) / 2;

    /* Panel background */
    ui_draw_rect(g_ui, panel_x, panel_y, PANEL_WIDTH, PANEL_HEIGHT, COLOR_BG_PANEL);
    ui_draw_rect_outline(g_ui, panel_x, panel_y, PANEL_WIDTH, PANEL_HEIGHT, COLOR_ACCENT_DIM);

    /* Title */
    ui_draw_text_centered(g_ui, g_ui->font_large, "Settings",
                          panel_x, panel_y + 10, PANEL_WIDTH, COLOR_ACCENT);

    /* Status indicator */
    const char *status;
    uint32_t status_color;
    if (g_connected) {
        status = "CONNECTED";
        status_color = COLOR_GREEN;
    } else {
        status = "DISCONNECTED";
        status_color = COLOR_RED;
    }
    ui_draw_text(g_ui, g_ui->font_small, status, panel_x + 15, panel_y + PANEL_HEIGHT - 25, status_color);

    /* Draw widgets */
    widget_input_draw(&g_input_host, g_ui);
    widget_input_draw(&g_input_port, g_ui);
    widget_slider_draw(&g_slider_gain, g_ui);
    widget_button_draw(&g_btn_connect, g_ui);
}

static void reposition_settings_panel(void) {
    int panel_x = (g_window_width - PANEL_WIDTH) / 2;
    int panel_y = (g_window_height - PANEL_HEIGHT) / 2;
    int x = panel_x + 15;
    int y = panel_y + 40;

    g_input_host.x = x; g_input_host.y = y; y += 50;
    g_input_port.x = x; g_input_port.y = y; y += 50;
    g_slider_gain.x = x; g_slider_gain.y = y; y += 45;
    g_btn_connect.x = x; g_btn_connect.y = y;
}
#endif

/*============================================================================
 * Status Indicator (non-GUI fallback)
 *============================================================================*/

static void draw_status_indicator(void) {
    int size = 12;
    int x = g_window_width - size - 5;
    int y = 5;

    uint8_t r, g, b;
    if (g_connected) {
        r = 0; g = 255; b = 0;
    } else {
        r = 255; g = 0; b = 0;
    }

    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < g_window_width && py >= 0 && py < g_window_height) {
                int idx = (py * g_window_width + px) * 3;
                g_pixels[idx] = r;
                g_pixels[idx+1] = g;
                g_pixels[idx+2] = b;
            }
        }
    }
}

/*============================================================================
 * Service Discovery Callback
 *============================================================================*/

static void on_service_discovered(const char *id, const char *service,
                                   const char *ip, int ctrl_port, int data_port,
                                   const char *caps, bool is_bye, void *userdata) {
    (void)userdata;
    (void)ctrl_port;
    (void)caps;
    
    if (is_bye) {
        printf("[DISCOVERY] Service left: %s '%s'\n", service, id);
        return;
    }
    
    printf("[DISCOVERY] Found %s '%s' at %s:%d\n", service, id, ip, data_port);
    
    /* Thread-safe: only set flags for main loop to process */
    if (strcmp(service, PN_SVC_SDR_SERVER) == 0) {
        strncpy(g_discovered_ip, ip, sizeof(g_discovered_ip) - 1);
        g_discovered_port = data_port;
        g_service_discovered = true;
    }
}

/*============================================================================
 * Main
 *============================================================================*/

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --host HOST       Relay server hostname (default: %s)\n", DEFAULT_RELAY_HOST);
    printf("  --port PORT       Display stream port (default: %d)\n", DEFAULT_RELAY_PORT);
    printf("  --test-pattern    Generate test tone (no network)\n");
    printf("  --node-id ID      Node ID for discovery (default: WATERFALL-1)\n");
    printf("  --no-discovery    Disable service discovery\n");
    printf("  --no-auto         Disable auto-connect to discovered services\n");
    printf("  --help            Show this help\n\n");
    printf("Runtime keys:\n");
    printf("  Tab        Toggle settings panel\n");
    printf("  +/-        Adjust gain\n");
    printf("  Q/Esc      Quit\n\n");
    printf("Window is resizable. Settings saved to %s\n", CONFIG_FILE);
}

int main(int argc, char *argv[]) {
    /* Load config first */
    load_config();

    /* Parse command line (overrides config) */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i+1 < argc) {
            strncpy(g_relay_host, argv[++i], sizeof(g_relay_host)-1);
        } else if (strcmp(argv[i], "--port") == 0 && i+1 < argc) {
            g_relay_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--node-id") == 0 && i+1 < argc) {
            strncpy(g_node_id, argv[++i], sizeof(g_node_id)-1);
        } else if (strcmp(argv[i], "--no-discovery") == 0) {
            g_discovery_enabled = false;
        } else if (strcmp(argv[i], "--no-auto") == 0) {
            g_auto_connect = false;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    print_version("Phoenix SDR - Waterfall");
    printf("Window: %dx%d\n", g_window_width, g_window_height);
    printf("Relay: %s:%d\n", g_relay_host, g_relay_port);

    /* Initialize networking */
    if (!tcp_init()) {
        fprintf(stderr, "Failed to initialize networking\n");
        return 1;
    }

    /* Initialize discovery */
    if (g_discovery_enabled) {
        if (pn_discovery_init(0) == 0) {
            /* Listen for services on LAN */
            pn_listen(on_service_discovered, NULL);
            /* Announce ourselves */
            pn_announce(g_node_id, PN_SVC_WATERFALL, 0, 0, "display");
            printf("Discovery: ENABLED (announcing as %s)\n", g_node_id);
            
            /* Query existing services in registry */
            const pn_service_t *sdr = pn_find_service(PN_SVC_SDR_SERVER);
            if (sdr) {
                printf("[DISCOVERY] Found existing sdr_server at %s:%d\n", 
                       sdr->ip, sdr->data_port);
                strncpy(g_discovered_ip, sdr->ip, sizeof(g_discovered_ip) - 1);
                g_discovered_port = sdr->data_port;
                g_service_discovered = true;
            }
        } else {
            fprintf(stderr, "Warning: Discovery init failed\n");
            g_discovery_enabled = false;
        }
    } else {
        printf("Discovery: DISABLED\n");
    }

    /* Initialize SDL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    g_window = SDL_CreateWindow(
        "Phoenix Waterfall",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        g_window_width, g_window_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(g_window, MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed\n");
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

#ifdef HAS_GUI
    g_ui = ui_core_init(g_renderer);
    if (g_ui) {
        init_settings_panel();
    }
#endif

    /* Allocate FFT buffers */
    g_fft_cfg = kiss_fft_alloc(DISPLAY_FFT_SIZE, 0, NULL, NULL);
    g_fft_in = (kiss_fft_cpx*)malloc(DISPLAY_FFT_SIZE * sizeof(kiss_fft_cpx));
    g_fft_out = (kiss_fft_cpx*)malloc(DISPLAY_FFT_SIZE * sizeof(kiss_fft_cpx));
    g_window_func = (float*)malloc(DISPLAY_FFT_SIZE * sizeof(float));
    g_iq_buffer = (iq_sample_t*)calloc(DISPLAY_FFT_SIZE, sizeof(iq_sample_t));

    if (!g_fft_cfg || !g_fft_in || !g_fft_out || !g_window_func || !g_iq_buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    if (!resize_buffers()) {
        fprintf(stderr, "Failed to allocate display buffers\n");
        return 1;
    }

    generate_blackman_harris(g_window_func, DISPLAY_FFT_SIZE);

    printf("\nPress Tab for settings, Q to quit\n\n");

    /* Wait for service discovery and auto-connect */
    g_show_settings = false;

    /* Main loop */
    bool running = true;
    static float *sample_buffer = NULL;
    static int sample_buffer_size = 0;
    mouse_state_t mouse = {0};

    while (running) {
        /* Process discovered services (thread-safe from callback) */
        if (g_service_discovered && !g_connected) {
            g_service_discovered = false;  /* Clear flag */
            
            /* Update connection settings */
            strncpy(g_relay_host, g_discovered_ip, sizeof(g_relay_host) - 1);
            g_relay_port = g_discovered_port;
            
#ifdef HAS_GUI
            /* Update UI widgets safely (we're in main thread) */
            widget_input_set_text(&g_input_host, g_discovered_ip);
            char port_str[16];
            snprintf(port_str, sizeof(port_str), "%d", g_discovered_port);
            widget_input_set_text(&g_input_port, port_str);
#endif
            
            /* Auto-connect if enabled */
            if (g_auto_connect) {
                printf("[DISCOVERY] Auto-connecting to %s:%d\n", g_relay_host, g_relay_port);
                connect_to_relay();
            } else {
                printf("[DISCOVERY] Updated connection fields to %s:%d (auto-connect disabled)\n",
                       g_relay_host, g_relay_port);
            }
        }

        /* Auto-reconnect timer (works with or without discovery) */
        if (!g_connected) {
            uint32_t now = SDL_GetTicks();
            if (now - g_last_reconnect_time >= RECONNECT_INTERVAL_MS) {
                /* Attempt reconnection (discovery provides target, or use configured host/port) */
                if (g_discovery_enabled || strlen(g_relay_host) > 0) {
                    printf("[AUTO-RECONNECT] Attempting connection to %s:%d...\n", 
                           g_relay_host, g_relay_port);
                    connect_to_relay();
                }
            }
        }
        
        /* Reset per-frame mouse state */
        mouse.left_clicked = false;
        mouse.left_released = false;
        mouse.wheel_y = 0;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        g_window_width = event.window.data1;
                        g_window_height = event.window.data2;
                        if (g_window_width < MIN_WINDOW_WIDTH) g_window_width = MIN_WINDOW_WIDTH;
                        if (g_window_height < MIN_WINDOW_HEIGHT) g_window_height = MIN_WINDOW_HEIGHT;
                        resize_buffers();
#ifdef HAS_GUI
                        if (g_ui) reposition_settings_panel();
#endif
                        save_config();
                    }
                    break;

                case SDL_MOUSEMOTION:
                    mouse.x = event.motion.x;
                    mouse.y = event.motion.y;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse.left_down = true;
                        mouse.left_clicked = true;
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse.left_down = false;
                        mouse.left_released = true;
                    }
                    break;

                case SDL_MOUSEWHEEL:
                    mouse.wheel_y = event.wheel.y;
                    break;

                case SDL_KEYDOWN:
                    /* Don't process keys when input is focused */
#ifdef HAS_GUI
                    if (g_input_host.focused || g_input_port.focused) break;
#endif
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            if (g_show_settings) {
                                g_show_settings = false;
                            } else {
                                running = false;
                            }
                            break;
                        case SDLK_q:
                            if (!g_show_settings) running = false;
                            break;
                        case SDLK_TAB:
                            g_show_settings = !g_show_settings;
                            break;
                        case SDLK_PLUS:
                        case SDLK_EQUALS:
                        case SDLK_KP_PLUS:
                            g_gain_offset += 3.0f;
#ifdef HAS_GUI
                            g_slider_gain.value = (int)g_gain_offset;
#endif
                            break;
                        case SDLK_MINUS:
                        case SDLK_KP_MINUS:
                            g_gain_offset -= 3.0f;
#ifdef HAS_GUI
                            g_slider_gain.value = (int)g_gain_offset;
#endif
                            break;
                    }
                    break;
            }

            /* Pass events to settings panel */
#ifdef HAS_GUI
            if (g_show_settings && g_ui) {
                update_settings_panel(&mouse, &event);
            }
#endif
        }

        /* Get data */
        bool got_samples = false;

        /*====================================================================
         * HOT PATH - Sample Acquisition (TCP from sdr_server PHXI/IQDQ)
         *====================================================================*/
        if (g_connected) {
            iqdq_data_frame_t frame;
            recv_result_t result = tcp_recv_exact(g_socket, &frame, sizeof(frame));

            if (result == RECV_TIMEOUT) {
                /* No data */
            } else if (result == RECV_ERROR) {
                printf("Connection lost\n");
                disconnect_from_relay();
            } else if (frame.magic == MAGIC_IQDQ) {
                /* Check sequence for dropped frames */
                if (g_last_sequence != 0 && frame.sequence != g_last_sequence + 1) {
                    uint32_t dropped = frame.sequence - g_last_sequence - 1;
                    printf("WARNING: Dropped %u frame(s) (seq %u → %u)\n", 
                           dropped, g_last_sequence, frame.sequence);
                }
                g_last_sequence = frame.sequence;

                /* Calculate bytes per sample based on format */
                int bytes_per_sample = (g_sample_format == SAMPLE_FORMAT_S16) ? 2 :
                                      (g_sample_format == SAMPLE_FORMAT_F32) ? 4 : 1;
                int data_bytes = frame.num_samples * 2 * bytes_per_sample;  /* I+Q pairs */

                /* Allocate raw buffer for network data */
                if (data_bytes > g_raw_buffer_size) {
                    g_raw_buffer = (uint8_t*)realloc(g_raw_buffer, data_bytes);
                    g_raw_buffer_size = data_bytes;
                }

                /* Allocate sample buffer for float conversion */
                int float_bytes = frame.num_samples * 2 * sizeof(float);
                if (float_bytes > sample_buffer_size) {
                    sample_buffer = (float*)realloc(sample_buffer, float_bytes);
                    sample_buffer_size = float_bytes;
                }

                /* HOT PATH - Read samples and convert to float */
                if (tcp_recv_exact(g_socket, g_raw_buffer, data_bytes) == RECV_OK) {
                    /* Convert samples to float32 based on format */
                    if (g_sample_format == SAMPLE_FORMAT_S16) {
                        pn_s16_to_float((int16_t*)g_raw_buffer, sample_buffer, frame.num_samples);
                    } else if (g_sample_format == SAMPLE_FORMAT_U8) {
                        pn_u8_to_float(g_raw_buffer, sample_buffer, frame.num_samples);
                    } else {  /* SAMPLE_FORMAT_F32 */
                        memcpy(sample_buffer, g_raw_buffer, float_bytes);
                    }

                    /* HOT PATH - Decimate and accumulate to FFT buffer */
                    for (uint32_t s = 0; s < frame.num_samples; s++) {
                        float i_sample = sample_buffer[s * 2];
                        float q_sample = sample_buffer[s * 2 + 1];
                        
                        float decimated_i, decimated_q;
                        bool i_ready = pn_decimate_process(&g_decimator_i, i_sample, &decimated_i);
                        bool q_ready = pn_decimate_process(&g_decimator_q, q_sample, &decimated_q);
                        
                        /* Both channels should decimate in sync */
                        if (i_ready && q_ready) {
                            g_iq_buffer[g_iq_buffer_idx].i = decimated_i;
                            g_iq_buffer[g_iq_buffer_idx].q = decimated_q;
                            g_iq_buffer_idx = (g_iq_buffer_idx + 1) % DISPLAY_FFT_SIZE;
                            g_new_samples++;
                        }
                    }
                    got_samples = (g_new_samples >= DISPLAY_OVERLAP);
                } else {
                    disconnect_from_relay();
                }
            } else if (frame.magic == MAGIC_META) {
                /* META frame - read remaining bytes (32 - 16 = 16 bytes) */
                meta_update_t meta;
                memcpy(&meta, &frame, sizeof(frame));  /* Copy header we already read */
                if (tcp_recv_exact(g_socket, ((uint8_t*)&meta) + sizeof(frame), 
                                  sizeof(meta) - sizeof(frame)) == RECV_OK) {
                    printf("META update: seq=%u\n", meta.sequence);
                    
                    /* Check if parameters changed requiring reconnection */
                    uint64_t new_freq = ((uint64_t)meta.center_freq_hi << 32) | meta.center_freq_lo;
                    (void)new_freq;  /* Unused for now, but logged */
                    
                    /* If sample format or rate changes, trigger complete reinit */
                    printf("  Center freq: %llu Hz, Gain: %.1f dB, LNA: %u\n",
                           (unsigned long long)new_freq, 
                           meta.gain_reduction / 10.0f, 
                           meta.lna_state);
                    /* Note: sdr_server doesn't currently send format/rate changes in META,
                     * but if it did, we'd disconnect and reconnect here */
                }
            } else {
                printf("Unknown frame magic: 0x%08X\n", frame.magic);
            }
        } else {
            /* Not connected - auto-reconnect handled below */
        }

        /* Skip if no data AND settings panel not open */
        if (!got_samples && !g_show_settings) {
            SDL_Delay(10);
            continue;
        }

        /*====================================================================
         * HOT PATH - FFT Processing
         * Apply Blackman-Harris window and compute FFT
         *====================================================================*/
        if (got_samples) {
            g_new_samples = 0;

            /* Apply window function to I/Q samples */
            for (int i = 0; i < DISPLAY_FFT_SIZE; i++) {
                int idx = (g_iq_buffer_idx + i) % DISPLAY_FFT_SIZE;
                g_fft_in[i].r = g_iq_buffer[idx].i * g_window_func[i];
                g_fft_in[i].i = g_iq_buffer[idx].q * g_window_func[i];
            }
            kiss_fft(g_fft_cfg, g_fft_in, g_fft_out);

            /*================================================================
             * HOT PATH - Magnitude Calculation
             * Map FFT bins to screen pixels with frequency zoom
             *================================================================*/
            float bin_hz = DISPLAY_HZ_PER_BIN;
            for (int i = 0; i < g_window_width; i++) {
                float freq = ((float)i / g_window_width - 0.5f) * 2.0f * ZOOM_MAX_HZ;
                int bin = (freq >= 0) ? (int)(freq / bin_hz + 0.5f)
                                      : DISPLAY_FFT_SIZE + (int)(freq / bin_hz - 0.5f);
                if (bin < 0) bin = 0;
                if (bin >= DISPLAY_FFT_SIZE) bin = DISPLAY_FFT_SIZE - 1;

                float re = g_fft_out[bin].r;
                float im = g_fft_out[bin].i;
                g_magnitudes[i] = sqrtf(re*re + im*im) / DISPLAY_FFT_SIZE;
            }

            /*================================================================
             * HOT PATH - Auto-Gain (Attack/Decay AGC)
             * Track peak and floor dB levels for color mapping
             *================================================================*/
            float frame_max = -200.0f, frame_min = 200.0f;
            for (int i = 0; i < g_window_width; i++) {
                float db = 20.0f * log10f(g_magnitudes[i] + 1e-10f);
                if (db > frame_max) frame_max = db;
                if (db < frame_min) frame_min = db;
            }
            g_peak_db += ((frame_max > g_peak_db) ? AGC_ATTACK : AGC_DECAY) * (frame_max - g_peak_db);
            g_floor_db += ((frame_min < g_floor_db) ? AGC_ATTACK : AGC_DECAY) * (frame_min - g_floor_db);

            /*================================================================
             * HOT PATH - Waterfall Scroll and Row Draw
             * Scroll existing pixels down, draw new row at top
             *================================================================*/
            memmove(g_pixels + g_window_width * 3, g_pixels,
                    g_window_width * (g_window_height - 1) * 3);

            for (int x = 0; x < g_window_width; x++) {
                uint8_t r, g, b;
                magnitude_to_rgb(g_magnitudes[x], g_peak_db, g_floor_db, &r, &g, &b);
                g_pixels[x*3] = r;
                g_pixels[x*3+1] = g;
                g_pixels[x*3+2] = b;
            }

            /* Status indicator overlay */
            draw_status_indicator();
        }

        /*====================================================================
         * HOT PATH - Render to Screen
         *====================================================================*/
        SDL_UpdateTexture(g_texture, NULL, g_pixels, g_window_width * 3);
        SDL_RenderClear(g_renderer);
        SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);

        /* Draw settings panel on top */
#ifdef HAS_GUI
        if (g_show_settings && g_ui) {
            draw_settings_panel();
        }
#endif

        SDL_RenderPresent(g_renderer);
        
        /* Small delay when settings panel open but no data */
        if (!got_samples) {
            SDL_Delay(16);  /* ~60fps */
        }
    }

    /* Cleanup */
    save_config();
    free(sample_buffer);
    disconnect_from_relay();
    
    /* Shutdown discovery (automatically sends BYE) */
    if (g_discovery_enabled) {
        pn_discovery_shutdown();
    }
    
    tcp_cleanup();

#ifdef HAS_GUI
    if (g_ui) ui_core_shutdown(g_ui);
#endif

    free(g_magnitudes);
    free(g_pixels);
    free(g_iq_buffer);
    free(g_window_func);
    free(g_fft_out);
    free(g_fft_in);
    kiss_fft_free(g_fft_cfg);

    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();

    printf("Done.\n");
    return 0;
}
