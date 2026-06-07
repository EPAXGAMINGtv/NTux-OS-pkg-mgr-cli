#include "ntuxpkg.h"
#include <stdio.h>
#include <string.h>
#include <syscall.h>

static const uint32_t PANEL  = 0xFF111B24u;
static const uint32_t HEADER = 0xFF1B2633u;
static const uint32_t TEXT   = 0xFFE5F1FFu;
static const uint32_t DIM    = 0xFF9FB3C6u;
static const uint32_t SEL    = 0xFF2E455Cu;
static const uint32_t GREEN  = 0xFF7FDBA7u;
static const uint32_t RED    = 0xFFFF6B6Bu;
static const uint32_t ORANGE = 0xFFFFC67Du;
static const uint32_t ROW_A  = 0xFF121D26u;

void pkg_ui_draw_header(window_t id, int x, int y, int w) {
    window_draw_rect(id, x, y, w, 24, HEADER, 1);
    window_draw_text(id, x + 10, y + 5, TEXT, "NTux Package Manager");
    char buf[64];
    snprintf(buf, sizeof(buf), "F5:Refresh  \x18\x19:Select  I:Install  R:Remove  U:Update");
    window_draw_text(id, x + w - 340, y + 5, DIM, buf);
}

void pkg_ui_draw_status(window_t id, const char* msg, int x, int y, int w) {
    window_draw_rect(id, x, y, w, 22, 0xFF141C24u, 1);
    if (msg && msg[0])
        window_draw_text(id, x + 8, y + 4, DIM, msg);
}

int pkg_ui_list_row_at(int mx, int my, int x, int y, int w, int h, int row_count) {
    (void)w;
    if (mx < x || mx >= x + 200) return -1;
    int row_h = 16;
    int max_show = (h / row_h);
    if (max_show > row_count) max_show = row_count;
    int list_h = max_show * row_h;
    if (my < y || my >= y + list_h) return -1;
    int idx = (my - y) / row_h;
    if (idx < 0 || idx >= row_count) return -1;
    return idx;
}

void pkg_ui_draw(window_t id, const pkg_entry_t* entries, int count,
                 int selected, int scroll_off, const char* status_msg) {
    int w = PKG_WIN_W;
    int h = PKG_WIN_H;

    pkg_ui_draw_header(id, 8, 8, w - 16);

    /* Left: package list */
    int list_x = 10;
    int list_y = 36;
    int list_w = 210;
    int list_h = h - 72;

    window_draw_rect(id, list_x, list_y, list_w, list_h, PANEL, 1);
    window_draw_rect(id, list_x, list_y, list_w, 20, HEADER, 1);
    window_draw_text(id, list_x + 6, list_y + 3, TEXT, "Packages");

    int row_h = 16;
    int max_show = list_h / row_h;
    int start = scroll_off;
    if (start > count) start = 0;

    for (int i = 0; i < max_show && start + i < count; i++) {
        int idx = start + i;
        int ry = list_y + 22 + i * row_h;
        uint32_t bg = (idx == selected) ? SEL : ((idx & 1) ? ROW_A : 0x00000000u);
        if (bg != 0x00000000u)
            window_draw_rect(id, list_x + 2, ry, list_w - 4, row_h - 1, bg, 1);

        char line[96];
        snprintf(line, sizeof(line), "%c %s",
                 entries[idx].installed ? '*' : ' ',
                 entries[idx].name);
        uint32_t col = entries[idx].installed ? TEXT : DIM;
        window_draw_text(id, list_x + 6, ry + 1, col, line);
    }

    /* Right: details panel */
    int det_x = 228;
    int det_y = 36;
    int det_w = w - 238;
    int det_h = list_h;

    window_draw_rect(id, det_x, det_y, det_w, det_h, PANEL, 1);
    if (selected >= 0 && selected < count) {
        pkg_entry_t* pkg = (pkg_entry_t*)&entries[selected];
        char buf[256];
        int ly = det_y + 10;

        window_draw_text(id, det_x + 10, ly, TEXT, pkg->name);
        ly += 24;

        snprintf(buf, sizeof(buf), "Version: %s", pkg->version[0] ? pkg->version : "?");
        window_draw_text(id, det_x + 10, ly, DIM, buf);
        ly += 18;

        snprintf(buf, sizeof(buf), "Author:  %s", pkg->author[0] ? pkg->author : "?");
        window_draw_text(id, det_x + 10, ly, DIM, buf);
        ly += 18;

        if (pkg->description[0]) {
            window_draw_text(id, det_x + 10, ly, DIM, pkg->description);
            ly += 22;
        }

        ly += 10;
        window_draw_text(id, det_x + 10, ly, pkg->installed ? GREEN : RED,
                         pkg->installed ? "INSTALLED" : "NOT INSTALLED");
        ly += 20;

        if (pkg->installed && pkg->installed_version[0]) {
            snprintf(buf, sizeof(buf), "Local version: %s", pkg->installed_version);
            window_draw_text(id, det_x + 10, ly, DIM, buf);
            ly += 18;
        }
        if (pkg->installed && pkg->installed_path[0]) {
            snprintf(buf, sizeof(buf), "Path: %s", pkg->installed_path);
            window_draw_text(id, det_x + 10, ly, DIM, buf);
        }
    } else {
        window_draw_text(id, det_x + 10, det_y + 14, DIM, "Select a package");
    }

    /* Bottom buttons */
    int by = h - 34;
    int bw = 80, bh = 24;
    uint32_t btn_bg = 0xFF1B2633u;

    /* Refresh */
    window_draw_rect(id, w - 340, by, bw, bh, btn_bg, 1);
    window_draw_text(id, w - 335, by + 5, DIM, "F5 Refresh");

    /* Install */
    window_draw_rect(id, w - 250, by, bw, bh, btn_bg, 1);
    window_draw_text(id, w - 245, by + 5, selected >= 0 ? TEXT : DIM, "I Install");

    /* Remove */
    window_draw_rect(id, w - 160, by, bw, bh, btn_bg, 1);
    window_draw_text(id, w - 155, by + 5,
                     (selected >= 0 && selected < count && entries[selected].installed) ? RED : DIM,
                     "R Remove");

    /* Update */
    window_draw_rect(id, w - 70, by, 64, bh, btn_bg, 1);
    window_draw_text(id, w - 65, by + 5,
                     (selected >= 0 && selected < count && entries[selected].installed) ? ORANGE : DIM,
                     "U Upd");

    /* Status bar */
    pkg_ui_draw_status(id, status_msg, 8, h - 14, w - 16);
}
