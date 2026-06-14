#include "ntuxpkg.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syscall.h>

#define HTTP_BUF 65536
#define DB_PATH "/etc/pkg/packages.json"
#define INSTALL_PREFIX "/fat0/"
#define DEF_SERVER "http://5.231.46.3:28700"

static int key_edge(uint8_t sc, uint8_t* last) {
    int now = (sys_kbd_is_pressed(sc) > 0) ? 1 : 0;
    int pressed = (now && !*last) ? 1 : 0;
    *last = (uint8_t)now;
    return pressed;
}

static int http_get_body(const char* url, char* buf, size_t cap) {
    int ret = sys_net_http_get(url, buf, cap);
    if (ret < 0) return -1;
    if ((size_t)ret >= cap) buf[cap - 1] = '\0';
    else buf[ret] = '\0';
    return ret;
}

int pkg_net_fetch_list(const char* server, pkg_entry_t* entries, int max) {
    char url[PKG_URL_MAX];
    char* buf = (char*)malloc(HTTP_BUF);
    if (!buf) return -1;
    snprintf(url, sizeof(url), "%s/packages", server);
    int ret = http_get_body(url, buf, HTTP_BUF);
    if (ret < 0) { free(buf); return -1; }
    int count = 0;
    const char* p = buf;
    while (*p && *p != '[') p++;
    if (!*p) { free(buf); return 0; }
    p++;
    while (*p && count < max) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',')) p++;
        if (*p == ']' || !*p) break;
        if (*p == '"') {
            p++;
            int n = 0;
            while (*p && *p != '"' && n < (int)sizeof(entries[count].name) - 1) {
                if (*p == '\\') { p++; if (*p) entries[count].name[n++] = *p++; }
                else entries[count].name[n++] = *p++;
            }
            entries[count].name[n] = '\0';
            if (*p == '"') p++;
            memset(entries[count].version, 0, sizeof(entries[count].version));
            memset(entries[count].description, 0, sizeof(entries[count].description));
            memset(entries[count].author, 0, sizeof(entries[count].author));
            memset(entries[count].installed_path, 0, sizeof(entries[count].installed_path));
            memset(entries[count].installed_version, 0, sizeof(entries[count].installed_version));
            entries[count].installed = 0;
            count++;
        } else p++;
    }
    free(buf);
    return count;
}

int pkg_net_fetch_meta(const char* server, const char* name, pkg_entry_t* entry) {
    char url[PKG_URL_MAX];
    char* buf = (char*)malloc(HTTP_BUF);
    if (!buf) return -1;
    snprintf(url, sizeof(url), "%s/package/%s", server, name);
    int ret = http_get_body(url, buf, HTTP_BUF);
    if (ret < 0) { free(buf); return -1; }
    entry->description[0] = '\0';
    entry->version[0] = '\0';
    entry->author[0] = '\0';
    const char* p = buf;
    while (*p) {
        if (*p == '"') {
            const char* ks = p + 1;
            const char* ke = strchr(ks, '"');
            if (ke && *(ke + 1) == ':') {
                size_t klen = (size_t)(ke - ks);
                p = ke + 2;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '"') {
                    p++;
                    char vb[PKG_DESC_MAX];
                    int vn = 0;
                    while (*p && *p != '"' && vn < (int)sizeof(vb) - 1) {
                        if (*p == '\\') { p++; if (*p) vb[vn++] = *p++; }
                        else vb[vn++] = *p++;
                    }
                    vb[vn] = '\0';
                    if (*p == '"') p++;
                    if (klen == 4 && memcmp(ks, "name", 4) == 0)
                        strncpy(entry->name, vb, sizeof(entry->name) - 1);
                    else if (klen == 7 && memcmp(ks, "version", 7) == 0)
                        strncpy(entry->version, vb, sizeof(entry->version) - 1);
                    else if (klen == 11 && memcmp(ks, "description", 11) == 0)
                        strncpy(entry->description, vb, sizeof(entry->description) - 1);
                    else if (klen == 6 && memcmp(ks, "author", 6) == 0)
                        strncpy(entry->author, vb, sizeof(entry->author) - 1);
                }
                continue;
            }
        }
        p++;
    }
    free(buf);
    return 0;
}

int pkg_net_download_elf(const char* server, const char* name,
                         void** out_data, size_t* out_size) {
    char url[PKG_URL_MAX];
    char* buf = (char*)malloc(HTTP_BUF);
    if (!buf) return -1;
    snprintf(url, sizeof(url), "%s/download/%s", server, name);
    int ret = http_get_body(url, buf, HTTP_BUF);
    if (ret < 0 || ret == 0) { free(buf); return -1; }
    *out_data = buf;
    *out_size = (size_t)ret;
    return 0;
}

/* -- Local DB -- */

int pkg_db_load(pkg_entry_t* entries, int* count) {
    char buf[8192];
    uint64_t len = 0;
    int rc = sys_fs_read_file(DB_PATH, buf, sizeof(buf) - 1, &len);
    if (rc != 0) { *count = 0; return 0; }
    buf[len] = '\0';
    *count = 0;
    const char* p = strstr(buf, "\"packages\"");
    if (!p) return 0;
    p = strchr(p, '[');
    if (!p) return 0;
    p++;
    while (*p && *count < PKG_MAX_LIST) {
        while (*p && *p != '{') { if (*p == ']') return 0; p++; }
        if (!*p || *p == ']') break;
        p++;
        pkg_entry_t* e = &entries[*count];
        memset(e, 0, sizeof(*e));
        e->installed = 1;
        while (*p && *p != '}') {
            while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',')) p++;
            if (*p == '"') {
                const char* ks = p + 1;
                const char* ke = strchr(ks, '"');
                if (!ke) break;
                size_t klen = (size_t)(ke - ks);
                p = ke + 1;
                while (*p && *p != ':') p++;
                if (*p == ':') p++;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '"') {
                    p++;
                    char val[256];
                    int vn = 0;
                    while (*p && *p != '"' && vn < 255) {
                        if (*p == '\\') { p++; if (*p) val[vn++] = *p++; }
                        else val[vn++] = *p++;
                    }
                    val[vn] = '\0';
                    if (*p == '"') p++;
                    if (klen == 4 && memcmp(ks, "name", 4) == 0)
                        strncpy(e->name, val, sizeof(e->name) - 1);
                    else if (klen == 7 && memcmp(ks, "version", 7) == 0)
                        strncpy(e->installed_version, val, sizeof(e->installed_version) - 1);
                    else if (klen == 14 && memcmp(ks, "installed_path", 14) == 0)
                        strncpy(e->installed_path, val, sizeof(e->installed_path) - 1);
                }
            } else p++;
        }
        if (*p == '}') p++;
        (*count)++;
    }
    return 0;
}

int pkg_db_save(const pkg_entry_t* entries, int count) {
    char buf[8192];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "{\"packages\":[");
    int first = 1;
    for (int i = 0; i < count; i++) {
        if (!entries[i].installed) continue;
        if (!first) {
            if ((size_t)pos + 1 >= sizeof(buf)) break;
            buf[pos++] = ',';
        }
        first = 0;
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
            "{\"name\":\"%s\",\"version\":\"%s\",\"installed_path\":\"%s\"}",
            entries[i].name, entries[i].installed_version, entries[i].installed_path);
    }
    if ((size_t)pos + 2 < sizeof(buf)) {
        buf[pos++] = ']';
        buf[pos++] = '}';
        buf[pos] = '\0';
    }
    sys_fs_mkdir("/etc", "pkg");
    return sys_fs_write_file(DB_PATH, buf, (size_t)pos);
}

int pkg_db_register(pkg_entry_t* entries, int* count,
                    const char* name, const char* version, const char* path) {
    for (int i = 0; i < *count; i++) {
        if (entries[i].installed && strcmp(entries[i].name, name) == 0) {
            strncpy(entries[i].installed_version, version, sizeof(entries[i].installed_version) - 1);
            strncpy(entries[i].installed_path, path, sizeof(entries[i].installed_path) - 1);
            return pkg_db_save(entries, *count);
        }
    }
    if (*count >= PKG_MAX_LIST) return -1;
    pkg_entry_t* e = &entries[*count];
    memset(e, 0, sizeof(*e));
    e->installed = 1;
    strncpy(e->name, name, sizeof(e->name) - 1);
    strncpy(e->installed_version, version, sizeof(e->installed_version) - 1);
    strncpy(e->installed_path, path, sizeof(e->installed_path) - 1);
    (*count)++;
    return pkg_db_save(entries, *count);
}

int pkg_db_unregister(pkg_entry_t* entries, int* count, const char* name) {
    for (int i = 0; i < *count; i++) {
        if (entries[i].installed && strcmp(entries[i].name, name) == 0) {
            entries[i].installed = 0;
            return pkg_db_save(entries, *count);
        }
    }
    return -1;
}

/* -- Main entry -- */

static int in_rect(int mx, int my, int rx, int ry, int rw, int rh) {
    return (mx >= rx && mx < rx + rw && my >= ry && my < ry + rh);
}

void ntux_user_entry(void) {
    window_t id = 0x504B474D475200ull;
    if (window_init() != 0 ||
        window_create(id, 100, 60, PKG_WIN_W, PKG_WIN_H,
                      0xFF0B1016u, "NTux Package Manager") != 0) {
        puts("[ntuxpkg] window_create failed");
        sys_exit(1);
    }

    pkg_entry_t entries[PKG_MAX_LIST];
    memset(entries, 0, sizeof(entries));
    int pkg_count = 0;
    int selected = -1;
    int scroll_off = 0;
    char server_url[PKG_URL_MAX];
    strncpy(server_url, DEF_SERVER, sizeof(server_url) - 1);

    char status[128];
    snprintf(status, sizeof(status), "Connecting to %s ...", server_url);

    pkg_entry_t local_entries[PKG_MAX_LIST];
    int local_count = 0;
    pkg_db_load(local_entries, &local_count);

    int pkg_count_srv = pkg_net_fetch_list(server_url, entries, PKG_MAX_LIST);
    if (pkg_count_srv < 0) {
        snprintf(status, sizeof(status), "Failed to connect to %s", server_url);
        pkg_count = 0;
    } else {
        pkg_count = pkg_count_srv;
        snprintf(status, sizeof(status), "Connected: %d packages available", pkg_count);
        for (int i = 0; i < pkg_count; i++) {
            for (int j = 0; j < local_count; j++) {
                if (local_entries[j].installed &&
                    strcmp(entries[i].name, local_entries[j].name) == 0) {
                    entries[i].installed = 1;
                    strncpy(entries[i].installed_path, local_entries[j].installed_path,
                            sizeof(entries[i].installed_path) - 1);
                    strncpy(entries[i].installed_version, local_entries[j].installed_version,
                            sizeof(entries[i].installed_version) - 1);
                    break;
                }
            }
        }
    }

    uint8_t last_up = 0, last_down = 0, last_esc = 0;
    int last_left = 0;

    for (;;) {
        if (window_should_close(id)) break;
        if (key_edge(0x01, &last_esc)) break;

        window_input_state_t st;
        memset(&st, 0, sizeof(st));
        window_get_input_state(id, &st);

        if (key_edge(0x48, &last_up)) {
            if (selected > 0) { selected--; if (selected < scroll_off) scroll_off = selected; }
        }
        if (key_edge(0x50, &last_down)) {
            if (selected < pkg_count - 1) {
                selected++;
                int max_visible = (PKG_WIN_H - 100) / 16;
                if (selected >= scroll_off + max_visible) scroll_off = selected - max_visible + 1;
            }
        }

        if (st.mouse_left && !last_left) {
            int row = pkg_ui_list_row_at(st.mouse_x, st.mouse_y, 10, 36, 200, PKG_WIN_H - 72, pkg_count);
            if (row >= 0 && row < pkg_count) {
                selected = row;
                if (selected < scroll_off) scroll_off = selected;
                int max_visible = (PKG_WIN_H - 100) / 16;
                if (selected >= scroll_off + max_visible) scroll_off = selected - max_visible + 1;
            }
            if (in_rect(st.mouse_x, st.mouse_y, PKG_WIN_W - 350, PKG_WIN_H - 34, 80, 24)) {
                /* Refresh */
                pkg_count_srv = pkg_net_fetch_list(server_url, entries, PKG_MAX_LIST);
                if (pkg_count_srv < 0) {
                    snprintf(status, sizeof(status), "Refresh failed: cannot reach %s", server_url);
                } else {
                    pkg_count = pkg_count_srv;
                    pkg_db_load(local_entries, &local_count);
                    for (int i = 0; i < pkg_count; i++) {
                        for (int j = 0; j < local_count; j++) {
                            if (local_entries[j].installed &&
                                strcmp(entries[i].name, local_entries[j].name) == 0) {
                                entries[i].installed = 1;
                                strncpy(entries[i].installed_path, local_entries[j].installed_path,
                                        sizeof(entries[i].installed_path) - 1);
                                strncpy(entries[i].installed_version, local_entries[j].installed_version,
                                        sizeof(entries[i].installed_version) - 1);
                                break;
                            }
                        }
                    }
                    snprintf(status, sizeof(status), "Refreshed: %d packages", pkg_count);
                }
            }
            if (in_rect(st.mouse_x, st.mouse_y, PKG_WIN_W - 260, PKG_WIN_H - 34, 80, 24)) {
                /* Install */
                if (selected >= 0 && selected < pkg_count) {
                    const char* name = entries[selected].name;
                    snprintf(status, sizeof(status), "Installing %s ...", name);
                    pkg_entry_t meta;
                    memset(&meta, 0, sizeof(meta));
                    if (pkg_net_fetch_meta(server_url, name, &meta) != 0) {
                        snprintf(status, sizeof(status), "Failed to fetch metadata for %s", name);
                    } else {
                        void* elf_data = NULL;
                        size_t elf_size = 0;
                        if (pkg_net_download_elf(server_url, name, &elf_data, &elf_size) != 0) {
                            snprintf(status, sizeof(status), "Failed to download %s", name);
                        } else {
                            char path[PKG_PATH_MAX];
                            snprintf(path, sizeof(path), "%s/%s", INSTALL_PREFIX, name);
                            sys_fs_mkdir("/usr", "bin");
                            int wr = sys_fs_write_file(path, elf_data, elf_size);
                            free(elf_data);
                            if (wr != 0) {
                                snprintf(status, sizeof(status), "Failed to write %s", path);
                            } else {
                                const char* ver = meta.version[0] ? meta.version : "1.0.0";
                                pkg_db_register(local_entries, &local_count, name, ver, path);
                                entries[selected].installed = 1;
                                strncpy(entries[selected].installed_path, path,
                                        sizeof(entries[selected].installed_path) - 1);
                                strncpy(entries[selected].installed_version, ver,
                                        sizeof(entries[selected].installed_version) - 1);
                                snprintf(status, sizeof(status), "Installed %s v%s", name, ver);
                            }
                        }
                    }
                }
            }
            if (in_rect(st.mouse_x, st.mouse_y, PKG_WIN_W - 170, PKG_WIN_H - 34, 80, 24)) {
                /* Remove */
                if (selected >= 0 && selected < pkg_count && entries[selected].installed) {
                    const char* name = entries[selected].name;
                    char path[PKG_PATH_MAX];
                    snprintf(path, sizeof(path), "%s/%s", INSTALL_PREFIX, name);
                    sys_fs_remove(path);
                    pkg_db_unregister(local_entries, &local_count, name);
                    entries[selected].installed = 0;
                    memset(entries[selected].installed_path, 0, sizeof(entries[selected].installed_path));
                    memset(entries[selected].installed_version, 0, sizeof(entries[selected].installed_version));
                    snprintf(status, sizeof(status), "Removed %s", name);
                }
            }
            if (in_rect(st.mouse_x, st.mouse_y, PKG_WIN_W - 80, PKG_WIN_H - 34, 70, 24)) {
                /* Update */
                if (selected >= 0 && selected < pkg_count && entries[selected].installed) {
                    const char* name = entries[selected].name;
                    snprintf(status, sizeof(status), "Checking for updates: %s ...", name);
                    pkg_entry_t meta;
                    memset(&meta, 0, sizeof(meta));
                    if (pkg_net_fetch_meta(server_url, name, &meta) == 0 && meta.version[0]) {
                        if (strcmp(meta.version, entries[selected].installed_version) != 0) {
                            void* elf_data = NULL;
                            size_t elf_size = 0;
                            if (pkg_net_download_elf(server_url, name, &elf_data, &elf_size) == 0) {
                                char path[PKG_PATH_MAX];
                                snprintf(path, sizeof(path), "%s/%s", INSTALL_PREFIX, name);
                                sys_fs_write_file(path, elf_data, elf_size);
                                free(elf_data);
                                strncpy(entries[selected].installed_version, meta.version,
                                        sizeof(entries[selected].installed_version) - 1);
                                strncpy(entries[selected].installed_path, path,
                                        sizeof(entries[selected].installed_path) - 1);
                                pkg_db_register(local_entries, &local_count, name, meta.version, path);
                                snprintf(status, sizeof(status), "Updated %s → v%s", name, meta.version);
                            } else {
                                snprintf(status, sizeof(status), "Download failed for %s", name);
                            }
                        } else {
                            snprintf(status, sizeof(status), "%s is up-to-date (v%s)", name, meta.version);
                        }
                    } else {
                        snprintf(status, sizeof(status), "Could not fetch metadata for %s", name);
                    }
                }
            }
        }
        last_left = st.mouse_left;

        window_clear(id, 0xFF0B1016u);
        pkg_ui_draw(id, entries, pkg_count, selected, scroll_off, status);
        window_present(id);

        usleep(33000);
    }

    sys_exit(0);
}
