// mbparse.c — Mega Bezel .slangp resolver for the Semu tap.
//
// Resolves a preset's #reference chain into a single flattened parameter map
// (outer files override referenced ones, matching slang-preset semantics) and
// resolves texture paths (BackgroundImage etc., any "*.png" value) to absolute
// paths relative to the file that set them. We use this to learn which artwork a
// MegaBezel preset uses + its geometry params, so the tap can sample the SAME
// PNG assets instead of being beholden to RetroArch's shader chain.
//
// Usage: mbparse <preset.slangp> [--all]
//   default: prints BackgroundImage + key geometry params + param count
//   --all  : dumps every flattened key=value
//
// Build (host): cc -O2 -o mbparse mbparse.c
// Build (Deck): nix run nixpkgs#zig -- cc -target x86_64-linux-gnu -O2 -o mbparse mbparse.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXK 8192
static char keys[MAXK][96], vals[MAXK][1024];
static int nk;

static void setkv(const char *k, const char *v) {
    for (int i = 0; i < nk; i++)
        if (!strcmp(keys[i], k)) { strncpy(vals[i], v, 1023); vals[i][1023] = 0; return; }
    if (nk >= MAXK) return;
    strncpy(keys[nk], k, 95); keys[nk][95] = 0;
    strncpy(vals[nk], v, 1023); vals[nk][1023] = 0;
    nk++;
}
static const char *getkv(const char *k) {
    for (int i = 0; i < nk; i++) if (!strcmp(keys[i], k)) return vals[i];
    return 0;
}

// dir = everything before the last '/'
static void dir_of(const char *path, char *out) {
    strcpy(out, path);
    char *s = strrchr(out, '/');
    if (s) *s = 0; else strcpy(out, ".");
}

// Join base + "/" + rel and normalize "." / ".." (base assumed absolute).
static void resolve(const char *base, const char *rel, char *out) {
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s/%s", base, rel);
    char *parts[512]; int np = 0;
    char *save = 0, *tok = strtok_r(tmp, "/", &save);
    while (tok) {
        if (!strcmp(tok, ".")) { /* skip */ }
        else if (!strcmp(tok, "..")) { if (np > 0) np--; }
        else parts[np++] = tok;
        tok = strtok_r(0, "/", &save);
    }
    out[0] = 0;
    for (int i = 0; i < np; i++) { strcat(out, "/"); strcat(out, parts[i]); }
}

static int ends_with(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && !strcmp(s + ls - lf, suf);
}

static char visited[8192][1024];
static int nv;

static void parse(const char *path) {
    for (int i = 0; i < nv; i++) if (!strcmp(visited[i], path)) return;  // cycle / dup guard
    if (nv < 8192) { strncpy(visited[nv], path, 1023); visited[nv][1023] = 0; nv++; }
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "warn: cannot open %s\n", path); return; }
    char dir[2048]; dir_of(path, dir);
    char line[2048];
    while (fgets(line, sizeof line, f)) {
        char *c = strstr(line, "//"); if (c) *c = 0;   // strip line comment
        char ref[1024];
        if (sscanf(line, " #reference \"%1023[^\"]\"", ref) == 1) {
            char rp[4096]; resolve(dir, ref, rp);
            parse(rp);                                   // load base first, then our overrides win
            continue;
        }
        char k[96], v[1024];
        if (sscanf(line, " %95[A-Za-z0-9_] = \"%1023[^\"]\"", k, v) == 2) {
            if (ends_with(v, ".png") || ends_with(v, ".jpg")) {
                char ap[4096]; resolve(dir, v, ap); setkv(k, ap);   // resolve art path to absolute
            } else setkv(k, v);
            continue;
        }
    }
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <preset.slangp> [--all]\n", argv[0]); return 1; }
    parse(argv[1]);
    int all = (argc > 2 && !strcmp(argv[2], "--all"));
    if (all) {
        for (int i = 0; i < nk; i++) printf("%s = \"%s\"\n", keys[i], vals[i]);
        return 0;
    }
    const char *bg = getkv("BackgroundImage");
    printf("BackgroundImage = %s\n", bg ? bg : "(none — generic/generated)");
    const char *interest[] = {
        "HSM_ASPECT_RATIO_MODE", "HSM_INT_SCALE_MODE", "HSM_INT_SCALE_MAX_HEIGHT", "HSM_BG_FILL_MODE",
        "HSM_SCREEN_POSITION_X", "HSM_SCREEN_POSITION_Y", "HSM_SCREEN_SCALE",
        "HSM_TUBE_DIFFUSE_IMAGE_BRIGHTNESS",
        "HSM_CURVATURE_MODE", "HSM_CURVATURE_2D_SCALE_LONG_AXIS", "HSM_CURVATURE_2D_SCALE_SHORT_AXIS",
        "HSM_CURVATURE_3D_RADIUS", "HSM_CURVATURE_3D_VIEW_DIST",
        "HSM_CURVATURE_3D_TILT_ANGLE_X", "HSM_CURVATURE_3D_TILT_ANGLE_Y",
        "HSM_REFLECT_GLOBAL_AMOUNT", "HSM_REFLECT_BLUR_MAX", "HSM_REFLECT_NOISE_AMOUNT",
        "HSM_REFLECT_FALLOFF_AMOUNT", "HSM_REFLECT_FULLSCREEN_GLOW", "HSM_REFLECT_DIRECTION",
        "HSM_BZL_WIDTH", "HSM_BZL_HEIGHT", "HSM_BZL_OPACITY", "HSM_BZL_BRIGHTNESS",
        "HSM_BZL_INNER_CORNER_RADIUS_SCALE", "HSM_BZL_NOISE", 0 };
    for (int i = 0; interest[i]; i++) {
        const char *v = getkv(interest[i]);
        if (v) printf("%s = %s\n", interest[i], v);
    }
    printf("# flattened params: %d\n", nk);
    return 0;
}
