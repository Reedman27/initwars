#include "assets.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef INITWARS_DATADIR
#define INITWARS_DATADIR "/usr/local/share/init-wars"
#endif

static char exe_dir[PATH_MAX];

void assets_init(const char *argv0)
{
    exe_dir[0] = '\0';
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n > 0) {
        buf[n] = '\0';
    } else if (argv0 && strlen(argv0) < sizeof buf) {
        snprintf(buf, sizeof buf, "%s", argv0);
    } else {
        return;
    }
    char *slash = strrchr(buf, '/');
    if (slash) {
        *slash = '\0';
        snprintf(exe_dir, sizeof exe_dir, "%s", buf);
    }
}

static bool try_path(const char *dir, const char *rel, char *out, size_t outsz)
{
    char p[PATH_MAX];
    int n = snprintf(p, sizeof p, "%s/%s", dir, rel);
    if (n < 0 || (size_t)n >= sizeof p || (size_t)n >= outsz) return false;
    if (access(p, R_OK) != 0) return false;
    memcpy(out, p, (size_t)n + 1);
    return true;
}

bool assets_find(const char *rel, char *out, size_t outsz)
{
    if (!rel || strstr(rel, "..")) return false;   /* never escape the asset dirs */
    const char *env = getenv("INITWARS_ASSETS");
    if (env && *env && try_path(env, rel, out, outsz)) return true;
    if (try_path("assets", rel, out, outsz)) return true;
    if (exe_dir[0]) {
        char d[PATH_MAX + 32];
        snprintf(d, sizeof d, "%s/assets", exe_dir);
        if (try_path(d, rel, out, outsz)) return true;
        snprintf(d, sizeof d, "%s/../share/init-wars", exe_dir);
        if (try_path(d, rel, out, outsz)) return true;
    }
    if (try_path(INITWARS_DATADIR, rel, out, outsz)) return true;
    if (try_path("/usr/share/init-wars", rel, out, outsz)) return true;
    return false;
}
