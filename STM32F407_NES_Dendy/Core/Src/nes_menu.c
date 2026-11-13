#include "nes_menu.h"

#ifndef LCD_PLAYBACK_DIAGNOSTICS
#include "fatfs.h"
#include "ili9341.h"
#include "main.h"
#include "nes_game.h"
#include "nes_module.h"
#include "sd_browser.h"
#include "lvgl.h"
#include <string.h>

#define MENU_WIDTH ILI9341_HEIGHT
#define MENU_HEIGHT ILI9341_WIDTH
#define DRAW_ROWS 12U
#define LIST_ROWS 6U
#define PATH_UNITS 512U

enum { KEY_OK = 0x09, KEY_BACK = 0x02, KEY_UP = 0x10, KEY_DOWN = 0x20 };
typedef enum { MODE_SELECT, MODE_FILES, MODE_GAMES } MenuMode;

typedef struct {
    TCHAR path[PATH_UNITS];
    TCHAR selected_name[_MAX_LFN + 1];
    lv_obj_t *title;
    lv_obj_t *location;
    lv_obj_t *rows[LIST_ROWS];
    lv_obj_t *footer;
    unsigned selected;
    unsigned first;
    unsigned count;
    MenuMode mode;
    bool selected_directory;
} Menu;

#define MENU_DRAW_ADDRESS (NES_MODULE_ADDRESS + LV_MEM_SIZE)
#define MENU_STATE_ADDRESS (MENU_DRAW_ADDRESS + MENU_WIDTH * DRAW_ROWS * sizeof(lv_color_t))
static Menu *const menu = (Menu *)(uintptr_t)MENU_STATE_ADDRESS;
static bool in_menu;
static bool return_requested;
static uint8_t held;

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;
    uint16_t count = width * height;
    ILI9341_BeginWindow(area->x1, area->y1, width, height);
    ILI9341_StartPixels((const uint16_t *)pixels, count);
    if (!ILI9341_WaitPixels()) {
        ILI9341_BeginWindow(area->x1, area->y1, width, height);
        ILI9341_WritePixels((const uint16_t *)pixels, count);
    }
    lv_disp_flush_ready(driver);
}

static lv_obj_t *make_label(int y, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_obj_set_width(label, MENU_WIDTH - 24);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 12, y);
    return label;
}

static size_t path_length(const TCHAR *path)
{
    const TCHAR *end = path;
    while (*end) ++end;
    return (size_t)(end - path);
}

static void utf8_name(char *out, size_t capacity, const TCHAR *name)
{
    while (*name && capacity >= 5) {
        uint32_t c = *name++;
        if (c >= 0xD800 && c <= 0xDBFF && *name >= 0xDC00 && *name <= 0xDFFF) {
            c = 0x10000 + ((c - 0xD800) << 10) + (*name++ - 0xDC00);
        } else if (c >= 0xD800 && c <= 0xDFFF) {
            c = '?';
        }
        unsigned length = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
        capacity -= length;
        for (unsigned i = length - 1; i > 0; --i) {
            out[i] = 0x80 | (c & 0x3F);
            c >>= 6;
        }
        out[0] = length == 1 ? c : c | ((0xF0U << (4 - length)) & 0xFFU);
        out += length;
    }
    *out = 0;
}

static bool is_rom(const FILINFO *entry)
{
    size_t n = path_length(entry->fname);
    return !(entry->fattrib & AM_DIR) && n >= 4 && entry->fname[n - 4] == '.' &&
           ff_wtoupper(entry->fname[n - 3]) == 'N' &&
           ff_wtoupper(entry->fname[n - 2]) == 'E' &&
           ff_wtoupper(entry->fname[n - 1]) == 'S';
}

static void show_row(unsigned row, const char *name, bool active)
{
    lv_obj_t *label = menu->rows[row];
    lv_label_set_text(label, name);
    lv_obj_set_style_text_color(label, lv_color_hex(active ? 0x7DE2BC : 0xE1E9FA), 0);
    lv_obj_set_style_bg_opa(label, active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(label, lv_color_hex(0x253956), 0);
}

static const char *error_text(void)
{
    const char *state = NesGame_Status();
    if (strcmp(state, "mmc1_module_missing") == 0) return "Missing /NES/InfoNES-MMC1.bin";
    if (strcmp(state, "cnrom_module_missing") == 0) return "Missing /NES/InfoNES-CNROM.bin";
    if (strcmp(state, "mmc3_module_missing") == 0) return "Missing /NES/InfoNES-MMC3.bin";
    if (strcmp(state, "save_read_error") == 0) return "Cannot read game save";
    if (strcmp(state, "save_write_error") == 0) return "Cannot write game save";
    if (strcmp(state, "axrom_module_missing") == 0) return "Missing /NES/InfoNES-AxROM.bin";
    if (strcmp(state, "uxrom_module_missing") == 0) return "Missing /NES/InfoNES-UxROM.bin";
    if (strcmp(state, "mapper_unsupported") == 0) return "Unsupported NES mapper";
    if (strcmp(state, "game_read_error") == 0) return "SD read error during gameplay";
    if (strcmp(state, "module_missing") == 0) return "Missing /NES/InfoNES.bin";
    if (strcmp(state, "module_invalid") == 0) return "Invalid InfoNES module";
    return "Unsupported or damaged ROM";
}

static void refresh(void)
{
    menu->selected_name[0] = 0;
    for (unsigned row = 0; row < LIST_ROWS; ++row) show_row(row, "", false);

    if (menu->mode == MODE_SELECT) {
        menu->count = 2;
        lv_label_set_text_static(menu->title, "NES / DENDY");
        lv_label_set_text_static(menu->location, "Select mode");
        show_row(0, "1. Browser", menu->selected == 0);
        show_row(1, "2. Games", menu->selected == 1);
        lv_label_set_text_static(menu->footer, "Up / Down    A / Start: Open");
        return;
    }

    char text[192];
    utf8_name(text, sizeof(text), menu->path + 2);
    lv_label_set_text(menu->location, text);
    lv_label_set_text_static(menu->title, menu->mode == MODE_FILES ? "Browser" : "Games");
    lv_label_set_text_static(menu->footer, "Up / Down    A / Start: Open    B: Back");

    DIR dir;
    FRESULT result = retSD == 0 ? f_opendir(&dir, menu->path) : FR_NOT_READY;
    if (result == FR_NOT_ENABLED && f_mount(&SDFatFS, u"0:", 0) == FR_OK)
        result = f_opendir(&dir, menu->path);

    unsigned index = 0;
    if (result == FR_OK) {
        FILINFO entry;
        while ((result = f_readdir(&dir, &entry)) == FR_OK && entry.fname[0]) {
            if (menu->mode == MODE_GAMES && !(entry.fattrib & AM_DIR) && !is_rom(&entry)) continue;
            if (index >= menu->first && index - menu->first < LIST_ROWS) {
                bool directory = (entry.fattrib & AM_DIR) != 0;
                size_t prefix = directory ? 2U : 0U;
                if (directory) memcpy(text, "/ ", prefix);
                utf8_name(text + prefix, sizeof(text) - prefix, entry.fname);
                show_row(index - menu->first, text, index == menu->selected);
                if (index == menu->selected) {
                    memcpy(menu->selected_name, entry.fname,
                           (path_length(entry.fname) + 1) * sizeof(TCHAR));
                    menu->selected_directory = directory;
                }
            }
            ++index;
            if (index == menu->first + LIST_ROWS + 1) break;
        }
        f_closedir(&dir);
    }
    menu->count = index;
    if (result != FR_OK) {
        menu->count = 0;
        menu->selected_name[0] = 0;
        for (unsigned row = 0; row < LIST_ROWS; ++row) show_row(row, "", false);
        show_row(0, menu->mode == MODE_GAMES && result == FR_NO_PATH ?
                    "Create /game on the SD card" : "Cannot read directory", false);
    } else if (index == 0) {
        show_row(0, menu->mode == MODE_GAMES ? "No .nes files" : "Empty directory", false);
    } else if (menu->selected >= index) {
        menu->selected = 0;
        menu->first = 0;
        refresh();
    }
}

static void select_mode(MenuMode mode)
{
    menu->mode = mode;
    menu->selected = menu->first = 0;
    if (mode == MODE_FILES) memcpy(menu->path, u"0:/", sizeof(u"0:/"));
    if (mode == MODE_GAMES) memcpy(menu->path, u"0:/game", sizeof(u"0:/game"));
    refresh();
}

void NesMenu_Init(void)
{
    _Static_assert(MENU_STATE_ADDRESS + sizeof(Menu) <= NES_MODULE_ADDRESS + NES_MODULE_CAPACITY,
                   "Menu exceeds reserved SRAM");
    static lv_disp_draw_buf_t draw_buffer;
    static lv_disp_drv_t driver;
    memset(menu, 0, sizeof(*menu));
    ILI9341_SetLandscape();
    lv_init();
    lv_disp_draw_buf_init(&draw_buffer, (void *)(uintptr_t)MENU_DRAW_ADDRESS, NULL,
                          MENU_WIDTH * DRAW_ROWS);
    lv_disp_drv_init(&driver);
    driver.hor_res = MENU_WIDTH;
    driver.ver_res = MENU_HEIGHT;
    driver.draw_buf = &draw_buffer;
    driver.flush_cb = flush;
    lv_disp_drv_register(&driver);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x141C30), 0);

    menu->title = make_label(8, &lv_font_montserrat_24, 0xF3F6FF);
    menu->location = make_label(39, LV_FONT_DEFAULT, 0xB8C9E7);
    for (unsigned row = 0; row < LIST_ROWS; ++row)
        menu->rows[row] = make_label(61 + row * 24, LV_FONT_DEFAULT, 0xE1E9FA);
    menu->footer = make_label(217, LV_FONT_DEFAULT, 0xB8C9E7);
    in_menu = true;
    held = SD_Browser_NesPad();
    select_mode(MODE_SELECT);
}

static void activate(void)
{
    if (menu->mode == MODE_SELECT) {
        select_mode(menu->selected == 0 ? MODE_FILES : MODE_GAMES);
        return;
    }
    if (!menu->selected_name[0]) {
        refresh();
        return;
    }
    if (menu->mode == MODE_FILES && !menu->selected_directory) {
        lv_label_set_text_static(menu->footer, "File selected    B: Back");
        return;
    }

    TCHAR path[PATH_UNITS];
    size_t base = path_length(menu->path);
    size_t name = path_length(menu->selected_name);
    bool slash = menu->path[base - 1] != '/';
    if (base + slash + name >= PATH_UNITS) {
        lv_label_set_text_static(menu->footer, "Path too long    B: Back");
        return;
    }
    memcpy(path, menu->path, base * sizeof(TCHAR));
    if (slash) path[base++] = '/';
    memcpy(path + base, menu->selected_name, (name + 1) * sizeof(TCHAR));

    if (menu->selected_directory) {
        memcpy(menu->path, path, (base + name + 1) * sizeof(TCHAR));
        menu->selected = menu->first = 0;
        refresh();
        return;
    }

    lv_disp_remove(lv_disp_get_default());
    lv_deinit();
    in_menu = false;
    NesGame_Start(path);
    if (strcmp(NesGame_Status(), "running") != 0) {
        NesMenu_Init();
        select_mode(MODE_GAMES);
        lv_label_set_text_static(menu->footer, error_text());
    }
}

void NesMenu_RequestReturn(void)
{
    return_requested = true;
}

void NesMenu_Poll(void)
{
    if (return_requested) {
        return_requested = false;
        if (in_menu) {
            select_mode(MODE_SELECT);
            held = SD_Browser_NesPad();
        } else {
            // LVGL reuses the SRAM occupied by InfoNES during play.
            NesGame_Stop();
            NesMenu_Init();
            if (strcmp(NesGame_Status(), "save_write_error") == 0) {
                select_mode(MODE_GAMES);
                lv_label_set_text_static(menu->footer, error_text());
            }
        }
        return;
    }

    if (!in_menu) {
        NesGame_Poll();
        if (strcmp(NesGame_Status(), "game_read_error") == 0) {
            NesMenu_Init();
            select_mode(MODE_GAMES);
            lv_label_set_text_static(menu->footer, error_text());
        }
        return;
    }

    lv_timer_handler();
    uint8_t current = SD_Browser_NesPad();
    uint8_t pressed = current & ~held;
    held = current;
    if (pressed & KEY_BACK) {
        size_t length = path_length(menu->path);
        const size_t root_length = menu->mode == MODE_GAMES ? 7U : 3U;
        if (menu->mode != MODE_SELECT && length > root_length) {
            while (length > root_length && menu->path[length - 1] != '/') --length;
            menu->path[length > root_length ? length - 1 : length] = 0;
            menu->selected = menu->first = 0;
            refresh();
        } else if (menu->mode != MODE_SELECT) {
            select_mode(MODE_SELECT);
        }
    } else if (pressed & KEY_OK) {
        activate();
    } else if ((pressed & KEY_UP) && menu->selected > 0) {
        --menu->selected;
        if (menu->selected < menu->first) --menu->first;
        refresh();
    } else if ((pressed & KEY_DOWN) && menu->selected + 1 < menu->count) {
        ++menu->selected;
        if (menu->selected >= menu->first + LIST_ROWS) ++menu->first;
        refresh();
    }
}
#endif
