#ifndef NTUXPKG_H
#define NTUXPKG_H

#include <stdint.h>
#include <stddef.h>
#include <window.h>

#define PKG_NAME_MAX    64
#define PKG_DESC_MAX    256
#define PKG_AUTHOR_MAX  64
#define PKG_VERSION_MAX 16
#define PKG_PATH_MAX    256
#define PKG_URL_MAX     512
#define PKG_MAX_LIST    128

#define PKG_WIN_W       640
#define PKG_WIN_H       400

typedef struct {
    char name[PKG_NAME_MAX];
    char version[PKG_VERSION_MAX];
    char description[PKG_DESC_MAX];
    char author[PKG_AUTHOR_MAX];
    int installed;
    char installed_path[PKG_PATH_MAX];
    char installed_version[PKG_VERSION_MAX];
} pkg_entry_t;

void pkg_ui_draw(window_t id, const pkg_entry_t* entries, int count,
                 int selected, int scroll_off,
                 const char* status_msg);

void pkg_ui_draw_header(window_t id, int x, int y, int w);
void pkg_ui_draw_list(window_t id, const pkg_entry_t* entries, int count,
                      int selected, int scroll_off,
                      int x, int y, int w, int h);
void pkg_ui_draw_details(window_t id, const pkg_entry_t* selected_pkg,
                         int x, int y, int w, int h);
void pkg_ui_draw_status(window_t id, const char* msg, int x, int y, int w);

int pkg_ui_list_row_at(int mx, int my, int x, int y, int w, int h, int row_count);

/* HTTP helpers */
int pkg_net_fetch_list(const char* server, pkg_entry_t* entries, int max);
int pkg_net_fetch_meta(const char* server, const char* name, pkg_entry_t* entry);
int pkg_net_download_elf(const char* server, const char* name,
                         void** out_data, size_t* out_size);

/* Local DB helpers */
int pkg_db_load(pkg_entry_t* entries, int* count);
int pkg_db_save(const pkg_entry_t* entries, int count);
int pkg_db_register(pkg_entry_t* entries, int* count,
                    const char* name, const char* version, const char* path);
int pkg_db_unregister(pkg_entry_t* entries, int* count, const char* name);

#endif
