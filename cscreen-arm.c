// SPDX-FileCopyrightText: 2026 mc kindsmüller
// SPDX-License-Identifier: MIT
//
// cscreen-arm: native (arm64) re-implementation of cscreen 1.x
//
// Goal: same command-line interface and output format as the original
// cscreen (Objective-C, x86_64/i386), reconstructed from its usage text and
// observed output. Behaviour that could not be observed is marked "ASSUMPTION".
//
// Build:  clang -O2 -Wall -framework ApplicationServices -o cscreen cscreen-arm.c
//
#include <ApplicationServices/ApplicationServices.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_DISPLAYS 32

// Usage text copied verbatim from the original binary (incl. tabs/spaces).
static const char *USAGE =
"Usage: cscreen [-d <depth>] [-x <width>] [-y <height>] [-r <refresh>] [-s <display>] [-v] [-m] [-f] [-l] [-h]\n"
"           [-d <depth>]    : specifies the bit depth (bits per pixel)\n"
"           [-x <width>]    : specifies the width in pixels\n"
"           [-y <height>]   : specifies the height in pixels\n"
"           [-r <refresh>]  : specifies the refresh rate in Hz\n"
"           [-s <display>]  : specifies which display to use (defaults to main display)\n"
"\t\t   use a as the option to -s to specify the action on all displays\n"
"           [-i <displayID>]: picks a display based on CGDirectDisplayID (permanent per display)\n"
"           continue to use '-s a' for \"all displays\"\n"
"           [-v]\t           : display valid modes (use -s to specify display or nothing for the default)\n"
"           [-m]            : require an exact match\n"
"           [-f]\t           : forces settings (ignores safety mechanisms; USE AT YOUR OWN RISK)\n"
"           [-l]            : lists the current displays and modes\n"
"           [-p]            : sets the requested display to have the menu bar\n"
"           [-h]            : displays the usage\n"
"           \n"
"           Note: Using -p will change the display index so you will likely want to use -l again to show the current\n"
"           displays if you wish to use -p a second time.\n";

static const char *HEADER = "DisplayID  Index     Depth     Width     Height  Refresh\n";
static const char *HINT   = "use -h to display all usage options\n";

// Original reports 32 for all modes. The pixel-encoding API it used is
// deprecated; all current macOS framebuffer modes are 32 bpp.
static int mode_depth(CGDisplayModeRef m) { (void)m; return 32; }

// Observed: 59.94 Hz and 47.95 Hz modes are shown as 60 and 48 -> rounding.
static int mode_refresh(CGDisplayModeRef m) {
    return (int)lround(CGDisplayModeGetRefreshRate(m));
}

static void print_row(CGDirectDisplayID id, int index, CGDisplayModeRef m) {
    printf("%8u%8d%10d%10zu%11zu%6d\n", id, index, mode_depth(m),
           CGDisplayModeGetWidth(m), CGDisplayModeGetHeight(m), mode_refresh(m));
}

int main(int argc, char *argv[]) {
    int opt_d = 0, opt_x = 0, opt_y = 0, opt_r = 0;
    int opt_v = 0, opt_m = 0, opt_f = 0, opt_l = 0, opt_p = 0, opt_h = 0;
    int all = 0, sel_index = 0;              // 0 = default (main display)
    long sel_id = -1;                        // -i <displayID>
    int c;

    while ((c = getopt(argc, argv, "d:x:y:r:s:i:vmflph")) != -1) {
        switch (c) {
            case 'd': opt_d = atoi(optarg); break;
            case 'x': opt_x = atoi(optarg); break;
            case 'y': opt_y = atoi(optarg); break;
            case 'r': opt_r = atoi(optarg); break;
            case 's':
                if (strcmp(optarg, "a") == 0) all = 1;
                else sel_index = atoi(optarg);
                break;
            case 'i': sel_id = strtol(optarg, NULL, 10); break;
            case 'v': opt_v = 1; break;
            case 'm': opt_m = 1; break;
            case 'f': opt_f = 1; break;   // ASSUMPTION: accepted, no extra safety checks implemented
            case 'l': opt_l = 1; break;
            case 'p': opt_p = 1; break;
            case 'h': opt_h = 1; break;
            default:  fputs(USAGE, stdout); return 1;
        }
    }
    (void)opt_f;

    if (opt_h) { fputs(USAGE, stdout); return 0; }

    CGDirectDisplayID ids[MAX_DISPLAYS];
    uint32_t n = 0;
    if (CGGetActiveDisplayList(MAX_DISPLAYS, ids, &n) != kCGErrorSuccess || n == 0) {
        fprintf(stderr, "cscreen: could not get display list\n");
        return 1;
    }

    // Build list of selected display indices (1-based, as shown by -l).
    int sel[MAX_DISPLAYS], nsel = 0;
    if (sel_id >= 0) {
        for (uint32_t i = 0; i < n; i++)
            if (ids[i] == (CGDirectDisplayID)sel_id) sel[nsel++] = (int)i + 1;
        if (nsel == 0) { fprintf(stderr, "cscreen: no display with ID %ld\n", sel_id); return 1; }
    } else if (all) {
        for (uint32_t i = 0; i < n; i++) sel[nsel++] = (int)i + 1;
    } else if (sel_index > 0) {
        if (sel_index > (int)n) { fprintf(stderr, "cscreen: invalid display index %d\n", sel_index); return 1; }
        sel[nsel++] = sel_index;
    } else {
        // Default: main display (first in list); for listing without -s: all displays
        sel[nsel++] = 1;
    }
    int explicit_sel = (sel_id >= 0 || all || sel_index > 0);

    int changed = 0;

    // ---- -p: make display the main (menu bar) display -----------------------
    if (opt_p) {
        if (nsel != 1) { fprintf(stderr, "cscreen: -p needs exactly one display\n"); return 1; }
        CGDirectDisplayID target = ids[sel[0] - 1];
        CGDirectDisplayID oldMain = CGMainDisplayID();
        if (target != oldMain) {
            // ASSUMPTION: swap origins of target and old main display.
            CGRect t = CGDisplayBounds(target);
            CGDisplayConfigRef cfg;
            if (CGBeginDisplayConfiguration(&cfg) != kCGErrorSuccess) return 1;
            CGConfigureDisplayOrigin(cfg, target, 0, 0);
            CGConfigureDisplayOrigin(cfg, oldMain, (int32_t)t.origin.x, (int32_t)t.origin.y);
            if (CGCompleteDisplayConfiguration(cfg, kCGConfigurePermanently) != kCGErrorSuccess) {
                fprintf(stderr, "cscreen: could not set main display\n");
                return 1;
            }
        }
        changed = 1;
    }

    // ---- -d/-x/-y/-r: change display mode -----------------------------------
    if (opt_d || opt_x || opt_y || opt_r) {
        for (int k = 0; k < nsel; k++) {
            CGDirectDisplayID id = ids[sel[k] - 1];
            CGDisplayModeRef cur = CGDisplayCopyDisplayMode(id);
            size_t ww = opt_x ? (size_t)opt_x : CGDisplayModeGetWidth(cur);
            size_t wh = opt_y ? (size_t)opt_y : CGDisplayModeGetHeight(cur);
            int    wr = opt_r ? opt_r : mode_refresh(cur);
            int    wd = opt_d ? opt_d : mode_depth(cur);

            CFArrayRef modes = CGDisplayCopyAllDisplayModes(id, NULL);
            CGDisplayModeRef best = NULL;
            double best_score = INFINITY;
            for (CFIndex i = 0; modes && i < CFArrayGetCount(modes); i++) {
                CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
                double dw = fabs((double)CGDisplayModeGetWidth(m) - ww);
                double dh = fabs((double)CGDisplayModeGetHeight(m) - wh);
                double dr = fabs((double)mode_refresh(m) - wr);
                double dd = fabs((double)mode_depth(m) - wd);
                int exact = (dw == 0 && dh == 0 && dr == 0 && dd == 0);
                if (opt_m && !exact) continue;
                // ASSUMPTION: without -m pick the closest mode; size dominates refresh/depth.
                double score = (dw + dh) * 1000.0 + dr * 10.0 + dd;
                if (score < best_score) { best_score = score; best = m; }
            }
            if (!best) {
                fprintf(stderr, "cscreen: no matching mode for display %d\n", sel[k]);
                if (modes) CFRelease(modes);
                CGDisplayModeRelease(cur);
                return 1;
            }
            CGDisplayConfigRef cfg;
            if (CGBeginDisplayConfiguration(&cfg) == kCGErrorSuccess) {
                CGConfigureDisplayWithDisplayMode(cfg, id, best, NULL);
                if (CGCompleteDisplayConfiguration(cfg, kCGConfigurePermanently) != kCGErrorSuccess)
                    fprintf(stderr, "cscreen: could not set mode for display %d\n", sel[k]);
            }
            if (modes) CFRelease(modes);
            CGDisplayModeRelease(cur);
        }
        changed = 1;
    }

    // Refresh list after changes (indices may have changed after -p)
    if (changed) CGGetActiveDisplayList(MAX_DISPLAYS, ids, &n);

    // ---- -v: list valid modes ------------------------------------------------
    if (opt_v) {
        for (int k = 0; k < (explicit_sel ? nsel : (int)n); k++) {
            int idx = explicit_sel ? sel[k] : k + 1;
            CGDirectDisplayID id = ids[idx - 1];
            CFArrayRef modes = CGDisplayCopyAllDisplayModes(id, NULL);
            fputs(HEADER, stdout);
            for (CFIndex i = 0; modes && i < CFArrayGetCount(modes); i++)
                print_row(id, (int)i + 1,
                          (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i));
            if (modes) CFRelease(modes);
        }
        fputs(HINT, stdout);
        return 0;
    }

    // ---- -l or no action: list current displays -----------------------------
    if (opt_l || !changed) {
        fputs(HEADER, stdout);
        for (int k = 0; k < (explicit_sel ? nsel : (int)n); k++) {
            int idx = explicit_sel ? sel[k] : k + 1;
            CGDisplayModeRef m = CGDisplayCopyDisplayMode(ids[idx - 1]);
            print_row(ids[idx - 1], idx, m);
            CGDisplayModeRelease(m);
        }
        fputs(HINT, stdout);
    }
    return 0;
}
