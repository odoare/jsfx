/*
 * JSFX
 * Copyright (C) 2004 and onwards, Cockos Incorporated
 * License: LGPL
 *
 * jsfx_embed.h: JSFX as a library in a host that is not REAPER (see
 * jsfx_host_embed.cpp). The host uses the sx_ functions of jsfx_api.h
 * directly (they are plain functions here, not a table), plus these.
 */

#ifndef _JSFX_EMBED_H_
#define _JSFX_EMBED_H_

#ifndef _JS_SFXUI_H_
class SX_Instance;
#endif

// As sx_createInstance(): loads <root>/Effects/<effect>, with <root>/Data as
// the data folder. srate>0 is the sample rate @init sees first. On failure
// returns NULL and, unlike sx_createInstance(), gives the reason (the
// compile error with its line) in err. A section that does not compile is
// a failure (REAPER loads such a script and shows the message), except @gfx.
SX_Instance *jsfx_embed_create(const char *root, const char *effect, int srate, char *err, int errsz);

// The script has a @gfx section.
bool jsfx_embed_has_gfx(SX_Instance *);

// The script's desc: line.
const char *jsfx_embed_description(SX_Instance *);

// Runs @gfx once, without a window, into the script's frame buffer of
// w x h pixels (scale256: 256 for 100 %, 512 for a 200 % screen). Call it
// from the thread that created the instance. mouse_cap: the buttons and
// modifiers as @gfx sees them (1 left, 2 right, 64 middle, 4 control, 8
// shift, 16 alt, 32 windows key; on macOS 4 is command and 32 control),
// or -1 to let the engine ask the system. Returns <0 if there is nothing
// to draw with, 1 if the frame buffer changed, 0 if it did not. The pixels
// (LICE: 0xAARRGGBB, rowspan in pixels, bottom-up if flipped) stay valid
// until the next call.
int jsfx_embed_gfx_frame(SX_Instance *, int w, int h, int mouse_x, int mouse_y, int mouse_cap, int scale256,
                         const unsigned int **pixels, int *rowspan, bool *flipped);

// Input for a display that has no window of its own (the host's).
// The wheel adds to mouse_wheel and mouse_hwheel (120 a notch), which the
// script clears when it has read them.
void jsfx_embed_gfx_wheel(SX_Instance *, double vertical, double horizontal);
// A key pressed (down 1) or released (down 0), as gfx_getchar() returns
// them: a character ('a', 'A', 13 Enter, 27 Escape, 8 Backspace, 9 Tab,
// 1..26 control + letter), a name in characters ('up', 'down', 'left',
// 'rght', 'home', 'end', 'pgup', 'pgdn', 'ins', 'del', 'f1'...), or
// ('u'<<24) + a Unicode character.
void jsfx_embed_gfx_key(SX_Instance *, int code, int down);

// The script's strings that can change, with their text (the strings' lock
// held while fn runs). Keys: "u<n>" a numbered string (0..1023), "n<name>"
// a named one (#name), "a<n>" the nth anonymous one (#); the same script
// gives the same keys on both sides.
void jsfx_embed_enum_strings(SX_Instance *, void (*fn)(void *ctx, const char *key, const char *value, int length), void *ctx);
// Sets one by its key; false when the script has no such string.
bool jsfx_embed_set_string(SX_Instance *, const char *key, const char *value, int length);

// A file slider (slider1:/folder:...): its values are the files of the
// folder, which sx_getParmDisplay() names.
bool jsfx_embed_slider_is_file(SX_Instance *, int parm);

// A slider shown in the effect's slider window: false for one whose name
// starts with '-' (or hidden by slider_show()); it is still a parameter.
bool jsfx_embed_slider_shown(SX_Instance *, int parm);

// What a display without a window needs from its host: gfx_showmenu()
// (the menu string as the script wrote it, the position in the frame's
// pixels; returns the item chosen, 1 for the first, 0 for none; called from
// within @gfx, so it waits for the choice) and gfx_setcursor() (a Windows
// cursor id, IDC_ARROW 32512 etc., and the script's cursor name if any).
struct jsfx_embed_gfx_host
{
  void *ctx;
  int (*showmenu)(void *ctx, const char *menu, int x, int y);
  void (*setcursor)(void *ctx, int resource_id, const char *name);
};
// NULL removes it (before the instance is destroyed).
void jsfx_embed_set_gfx_host(SX_Instance *, const jsfx_embed_gfx_host *);

// Files dropped on the display, for gfx_getdropfile(): NULL clears the list,
// then one call per file.
void jsfx_embed_gfx_drop(SX_Instance *, const char *path);

// Where a slider's value lives (its variable), by parameter index.
double *jsfx_embed_slider_var(SX_Instance *, int parm);

// The size and rate the script asks for in its "@gfx [w h]" line and
// gfx_hz (0 when it does not say).
void jsfx_embed_gfx_request(SX_Instance *, int *w, int *h, int *hz);

// The mirror: an audio instance (where the audio sections run) and a
// display instance of the same script (where @gfx runs), kept in step by the
// host, which copies variables and memory from one to the other.

// Makes an instance a display: @init, @slider, @block and @sample never run
// in it (jsfx_embed_gfx_frame no longer starts with @init).
void jsfx_embed_set_display(SX_Instance *);

// Held while memory is read or written from another thread than the audio
// one: the audio thread takes it to free or reset memory.
void jsfx_embed_lock(SX_Instance *);
void jsfx_embed_unlock(SX_Instance *);

// Every variable of the script's VM, with where its value lives (valid as
// long as the instance; the same script gives the same names on both sides).
void jsfx_embed_enum_vars(SX_Instance *, void (*fn)(void *ctx, const char *name, double *value), void *ctx);

// A variable by name, created if the VM does not have it.
double *jsfx_embed_var(SX_Instance *, const char *name);

// Memory is blocks of jsfx_embed_mem_block_size items. One block, or NULL
// when it is not allocated (allocate false) or cannot be.
enum { jsfx_embed_mem_block_size = 65536 };
double *jsfx_embed_mem_block(SX_Instance *, int block, bool allocate);

// gmem[], the memory the scripts of a process share (named with
// options:gmem=name, else one buffer for all), as blocks of the same size:
// one block, or NULL; and the blocks allocated, up to max.
double *jsfx_embed_gmem_block(SX_Instance *, int block, bool allocate);
int jsfx_embed_gmem_blocks_used(SX_Instance *, int *blocks, int max);

// The blocks allocated (the script touched them), up to max, in order;
// returns how many there are.
int jsfx_embed_mem_blocks_used(SX_Instance *, int *blocks, int max);

// The entry points of jsfx_api.h, as functions.
int sx_processSamples(SX_Instance *, double *buf, int cnt, int nch, int srate, double tempo, int tsnum, int tsdenom, double playstate, double playpos, double playpos_b, double lastwet, double wet, int flags);
void sx_destroyInstance(SX_Instance *);
const char *sx_getEffectName(SX_Instance *);
void sx_loadState(SX_Instance *, const char *);
const char *sx_saveState(SX_Instance *, int *lOut);
void sx_loadSerState(SX_Instance *, const char *buf, int len);
const char *sx_saveSerState(SX_Instance *, int *len);
int sx_getNumParms(SX_Instance *);
double sx_getParmVal(SX_Instance *, int parm, double *minval, double *maxval, double *step);
void sx_getParmName(SX_Instance *, int parm, char *name, int namelen);
void sx_setParmVal(SX_Instance *, int parm, double val, int sampleoffs);
void sx_getParmDisplay(SX_Instance *, int parm, char *disptxt, int len, double *inval);
int sx_parmIsEnum(SX_Instance *, int parm);
bool sx_hasParmChanged(SX_Instance *);
bool sx_getParmValueFromString(SX_Instance *, int parm, const char *buf, double *valptr);
int sx_getCurrentLatency(SX_Instance *);
void sx_updateHostNch(SX_Instance *, int nch);
const char **sx_getPinInfo(SX_Instance *, int isOutput, int *numPins);
double sx_getTailSize(SX_Instance *);
void sx_set_midi_ctx(SX_Instance *, double (*midi_sendrecv)(void *ctx, int action, double *ts, double *msg1, double *msg23, double *midi_bus), void *midi_ctxdata);
void sx_set_host_ctx(SX_Instance *, void *ctx, void (*slider_automate)(void *ctx, int parmidx, bool done));

#endif
