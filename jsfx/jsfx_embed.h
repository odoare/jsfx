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
// from the thread that created the instance. Returns <0 if there is nothing
// to draw with, 1 if the frame buffer changed, 0 if it did not. The pixels
// (LICE: 0xAARRGGBB, rowspan in pixels, bottom-up if flipped) stay valid
// until the next call.
int jsfx_embed_gfx_frame(SX_Instance *, int w, int h, int mouse_x, int mouse_y, int scale256,
                         const unsigned int **pixels, int *rowspan, bool *flipped);

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
