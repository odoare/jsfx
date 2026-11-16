/*
 * JSFX
 * Copyright (C) 2004 and onwards, Cockos Incorporated
 * License: LGPL
 *
 * jsfx_host_embed.cpp: JSFX as a library inside a host that is not REAPER
 * (FxmeNet: github.com/odoare/FxmeNet). It takes the place of sfxui.cpp,
 * sfx_edit.cpp and jsfx_api.cpp: no window, no editor, no REAPER API.
 *
 *  - the REAPER function pointers exist and stay NULL (the core tests them);
 *  - the parts of sfxui.cpp that are not window code are repeated here
 *    (slider shaping, image loading, sx_processSamples, the EEL mutex);
 *  - the window hooks the core calls are stubs.
 *
 * The API for the host is jsfx_embed.h.
 */

#include <math.h> // before curses.h, whose macros (move) break the C++ headers

#include "sfxui.h"
#include "sfx_edit.h"
#include "jsfx_embed.h"

#define EEL_LICE_STANDALONE_NOINITQUIT
#define EEL_LICE_WANT_STANDALONE
#define EEL_LICE_API_ONLY
#include "../WDL/eel2/eel_lice.h"

HINSTANCE g_hInst;
const char *g_config_slider_classname = "REAPERhfader";

/// REAPER's API: absent
void (*FreeHeapPtr)(void*);
bool (*fxPresetPromptForAction)(HWND hwndDlg, void *parentid, int action, char *buf, int bufsz);
bool *(*DoFxLastTweakParmCtxMenu)(void* fxdsp, HWND h, int, int, const char*);
void *(*fxSetBypass)(void*, bool);
void *(*fxSetWet)(void*, double val, bool done);
bool (*fxGetSetDeltaSolo)(void *ctx, bool *delta_solo);
bool (*fxLoadReaperPreset)(void*, const char*);
int (*fxGetReaperPresetNamesRaw)(void*, char***);
int (*fxGetPlacement)(void *fxdsp, int *chain_pos, int *flags);
void (*fxRenameReaperPreset)(void*, const char*, const char*);
int (*fxDoReaperPresetAction)(void*, const char*, int);
bool (*fxImportExportRPL)(void*, HWND, bool);
bool (*fxGetSetWantAllKeys)(void*, bool*);
const char * (*get_ini_file)();
HWND (*GetMainHwnd)();
void (*fxSetUndoPoint)(void*);

int (*fxGetSetHostNumChan)(void *fxdsp, int *set_numchan);
int (*fxGetSetPinMap2)(void *fxdsp, bool isout, unsigned int *mapping, int ch_offs, int *isset_sz);
int (*fxGetSetPinmapperFlags)(void *fxdsp, int *set_flags);

const char *(*get_eel_funcdesc)(const char *);

ReaProject *(*GetProjectFromFxDsp)(void *);
MediaTrack *(*GetTrack)(ReaProject *, int idx);
bool (*GetTrackName)(MediaTrack *, char*, int);

PCM_source *(*_PCM_Source_CreateFromFile)(const char *filename);
bool (*IsMediaExtension)(const char *extparm, bool wantOthers);
bool (*Plugin_FindNewFileName)(ReaProject *__proj, int trackid, const char *trackname, const char *ext, char *buf, int bufsz);
PCM_sink *(*PCM_Sink_Create)(const char *filename, const void *cfg, int cfg_l, int nch, int srate, bool buildpeaks);
int (*InsertMedia)(const char* file, int mode);
void (*SetEditCurPos)(double time, bool moveview, bool seekplay);
double (*GetProjectLength)(ReaProject *__proj);
bool (*SetTempoTimeSigMarker)(ReaProject* __proj, int ptidx, double tpos, int mpos, double bpos, double bpm, int tsnum, int tsdenom, bool lineartempo);
double (*GetCursorPositionEx)(ReaProject*);
int (*FindTempoTimeSigMarker)(ReaProject* __proj, double tpos);
bool (*GetTempoTimeSigMarker)(ReaProject* __proj, int ptidx, double* tpos, int* mpos, double* bpos, double* bpm, int* tsnum, int* tsdenom, bool* lineartempo);
void (*CSurf_OnTempoChange)(double);
double (*Master_GetTempo)();
void (*EnsureNotCompletelyOffscreen)(RECT *);
int (*GetWindowDPIScalingForDialog)(HWND);
void (*SetWindowAccessibilityString)(HWND h, const char *, int mode);
#ifndef _WIN32
void (*Mac_MakeDefaultWindowMenu)(HWND);
#endif
const char * (*GetAppVersion)(void);

void (*_NSEEL_HOSTSTUB_EnterMutex)();
void (*_NSEEL_HOSTSTUB_LeaveMutex)();
void ** (*eel_gmem_attach)(const char *nm, bool is_alloc);
int (*plugin_register)(const char *, void *);

int *g_config_fontsize_ptr, *g_config_maxsug_ptr, *g_config_editflag_ptr;

void sx_provideAPIFunctionGetter(void *(*)(const char *)) { }


/// the EEL mutex (sfxui.cpp)
static WDL_Mutex sfx_mutex;
void NSEEL_HOSTSTUB_EnterMutex() { sfx_mutex.Enter(); }
void NSEEL_HOSTSTUB_LeaveMutex() { sfx_mutex.Leave(); }


/// window hooks: nothing to do without a window
void curses_registerChildClass(HINSTANCE) { }
void curses_unregisterChildClass(HINSTANCE) { }

void SX_Instance::add_idle() { }
void SX_Instance::remove_idle() { }
void SX_Instance::run_idle() { }
void SX_Instance::calcPeaks(double *, int, int, int) { }

void SX_Instance::gfx_notify_general_statechange() { }

void SX_Instance::DoRecompile(HWND, int resetFlags, WDL_FastString *strout)
{
  // as sfxui.cpp's, without the dialog
  m_mutex.Enter();
  char buf[2048];
  lstrcpyn_safe(buf,m_fn.Get(),sizeof(buf));
  const bool ok = LoadEffect(buf,resetFlags,NULL);
  if (ok) DoImageLoads(false);
  if (strout) strout->Set(ok ? "" : m_last_error);
  m_mutex.Leave();
}

bool SX_Instance::getCurPresetName(char* name, int maxlen)
{
  lstrcpyn_safe(name, m_curpresetname.Get(), maxlen);
  return true;
}

bool SX_Instance::loadPreset(const char*) { return false; }


/// from sfxui.cpp, unchanged
void SX_Instance::DoImageLoads(bool fullReset)
{
  if (fullReset)
  {
    int x;
    for (x=0;x<m_lice_state->m_gfx_images.GetSize();x++)
    {
      m_lice_state->m_gfx_images.Get()[x].clear();
    }
    m_lice_state->m_gfx_font_active=-1;
    for (x=0;x<m_lice_state->m_gfx_fonts.GetSize();x++)
    {
      if (LICE_FUNCTION_VALID(LICE__DestroyFont)) LICE__DestroyFont(m_lice_state->m_gfx_fonts.Get()[x].font);
      memset(&m_lice_state->m_gfx_fonts.Get()[x],0,sizeof(m_lice_state->m_gfx_fonts.Get()[x]));
    }
  }
  if (LICE_FUNCTION_VALID(LICE_LoadImage))
  {
    WDL_FastString tmp;
    int fullfnlen=0;

    if (m_full_actual_fn_used.GetLength())
    {
      const char *ptr=m_full_actual_fn_used.Get()+m_full_actual_fn_used.GetLength();
      while (ptr>=m_full_actual_fn_used.Get() && *ptr != '\\' && *ptr != '/') ptr--;
      fullfnlen = (int) (ptr-m_full_actual_fn_used.Get()+1);
    }

    int x;
    for (x = 0; x < m_localfilenames.GetSize(); x ++)
    {
      const char *fn=m_localfilenames.Get(x);
      if (fn && strlen(fn)>4 && !stricmp(fn+strlen(fn)-4,".png"))
      {
        if (x >= m_lice_state->m_gfx_images.GetSize())
        {
          if (x > 4096) continue;

          const int oldsz=m_lice_state->m_gfx_images.GetSize();
          eel_lice_state::img_state *c = m_lice_state->m_gfx_images.Resize(x+1);

          const int newsz=m_lice_state->m_gfx_images.GetSize();
          if (newsz>oldsz && c) memset(c+oldsz,0,sizeof(eel_lice_state::img_state) * (newsz-oldsz));

          if (x >= newsz) continue;
        }

        m_lice_state->m_gfx_images.Get()[x].clear();

        bool ok = false;
        if (fullfnlen>0)
        {
          tmp.Set(m_full_actual_fn_used.Get(),fullfnlen);
          tmp.Append(fn);
          ok = m_lice_state->do_load_image(x, tmp.Get());
        }

        if (!ok && m_datadir.GetLength())
        {
          tmp.Set(m_datadir.Get());
          tmp.Append("/");
          tmp.Append(fn);
          ok = m_lice_state->do_load_image(x, tmp.Get());
        }
      }
    }
  }
}

double effectSlider::_sc_inv(double v) const
{
  switch (ui_is_shape)
  {
    case SHAPE_LOG_PLAIN: return exp(v) - ui_shape_parm;
    case SHAPE_POW: return v < 0.0 ? -pow(-v,ui_shape_parm) : pow(v,ui_shape_parm);
    case SHAPE_LOG_MIDPOINT: return range_min + (exp(ui_shape_parm*v) - 1.0) / ui_shape_parm2;
  }
  return v;
}
double effectSlider::_sc(double v) const
{
  switch (ui_is_shape)
  {
    case SHAPE_LOG_PLAIN: return log(wdl_max(v + ui_shape_parm,1e-100));
    case SHAPE_POW: return v < 0.0 ? -pow(-v,1.0/ui_shape_parm) : pow(v,1.0/ui_shape_parm);
    case SHAPE_LOG_MIDPOINT:
      v = (v-range_min) * ui_shape_parm2 + 1.0;
      return log(wdl_max(v,1e-100))/ui_shape_parm;
  }
  return v;
}

double effectSlider::scaleFromExternal(double cv) const
{
  if (!external_shaping) return cv;

  double v = _sc_inv(cv);
  if (range_scale > 0.0)
    v = floor(v/range_scale+0.5)*range_scale;

  return v;
}

double effectSlider::scaleToExternal(double cv) const
{
  if (!external_shaping) return cv;

  return _sc(cv);
}


/// the sx_ entry points that sat in sfxui.cpp
void sx_deleteUI(SX_Instance *) { }
HWND sx_createUI(SX_Instance *, HINSTANCE, HWND, void *) { return NULL; }
HWND sx_getUIwnd(SX_Instance *) { return NULL; }
INT_PTR sx_inlineEditor(SX_Instance *, int, void *, void *) { return 0; }

void sx_setCurPresetName(SX_Instance *sx, const char *name)
{
  if (sx && name) sx->m_curpresetname.Set(name);
}

bool sx_getCurPresetName(SX_Instance *sx, char* name, int maxlen)
{
  if (!sx) return false;
  return sx->getCurPresetName(name, maxlen);
}

bool sx_loadPreset(SX_Instance *sx, const char* name)
{
  if (!sx) return false;
  return sx->loadPreset(name);
}

double sx_getTailSize(SX_Instance *sx)
{
  if (!sx || !sx->m_ext_tail_size) return 0.0;
  return *sx->m_ext_tail_size;
}

int sx_processSamples(SX_Instance *sx, double *buf, int cnt, int nch, int srate,
                      double tempo, int tsnum, int tsdenom,
                      double playstate, double playpos, double playpos_b,
                      double lastwet, double wet, int flags)
{
  if (!sx) return 0;
  return sx->process_samples(buf,cnt,nch,srate,
                             tempo,tsnum,tsdenom,
                             playstate,playpos,playpos_b,
                             lastwet,wet,flags);
}


/// jsfx_embed.h
extern int g_last_srate;

SX_Instance *jsfx_embed_create(const char *root, const char *effect, int srate, char *err, int errsz)
{
  if (err && errsz>0) err[0]=0;
  if (srate > 0) g_last_srate = srate;

  // sx_createInstance() deletes the instance when the script does not
  // compile, and its message with it: done here to keep the message
  SX_Instance *sx=new SX_Instance;
  sx->m_name.Set(effect);
  sx->m_datadir.Set(root);
  sx->m_effectdir.Set(root);
#ifdef _WIN32
  sx->m_datadir.Append("\\Data");
  sx->m_effectdir.Append("\\Effects");
#else
  sx->m_datadir.Append("/Data");
  sx->m_effectdir.Append("/Effects");
  for (char *p = (char *)sx->m_name.Get(); *p; p++) if (*p == '\\') *p='/';
#endif
  sx->m_last_error[0]=0;

  // LoadEffect() succeeds when a section does not compile (REAPER then shows
  // the message in the effect's window, and the section does nothing). Here
  // that is a failure, except for @gfx, which the audio does not need.
  if (sx->LoadEffect(sx->m_name.Get(), SX_Instance::LOADEFFECT_RESETFLAG_CONFIGITEMS|SX_Instance::LOADEFFECT_RESETFLAG_SLIDERS, NULL) &&
      !sx->m_init_err && !sx->m_slider_err && !sx->m_onblock_err && !sx->m_sample_err && !sx->m_serialize_err)
  {
    sx->DoImageLoads(false);
    return sx;
  }

  if (err && errsz>0) lstrcpyn_safe(err, sx->m_last_error[0] ? sx->m_last_error : "could not load the script", errsz);
  delete sx;
  return NULL;
}

bool jsfx_embed_has_gfx(SX_Instance *sx)
{
  return sx && sx->gfx_hasCode();
}

const char *jsfx_embed_description(SX_Instance *sx)
{
  return sx ? sx->m_description.Get() : "";
}


#if !defined(_WIN32) && !defined(__APPLE__)
// SWELL is built into this library (no libSwell.so, nothing exported): what
// swell-appstub-generic.cpp would give to modules is not needed.
extern "C" void *SWELLAPI_GetFunc(const char *) { return NULL; }
#endif


int jsfx_embed_gfx_frame(SX_Instance *sx, int w, int h, int mouse_x, int mouse_y, int mouse_cap, int scale256,
                         const unsigned int **pixels, int *rowspan, bool *flipped)
{
  // as sx_inlineEditor()'s paint (sfxui.cpp), without REAPER's bitmap
  eel_lice_state *ps = sx ? sx->m_lice_state : NULL;
  if (!ps || !sx->gfx_hasCode() || w < 1 || h < 1) return -1;

  sx->m_in_gfx++;
  if (sx->m_need_init)
  {
    sx->m_mutex.Enter();
    sx->m_init_mutex.Enter();
    if (sx->m_need_init) sx->on_slider_change();
    sx->m_mutex.Leave();
  }
  else
  {
    sx->m_init_mutex.Enter();
  }

  RECT r = { 0,0, w, h };
  int dr = ps->setup_frame(NULL,r, mouse_x,mouse_y, scale256 > 0 ? scale256 : 256);
  if (dr>=0)
  {
    // setup_frame() read the buttons and modifiers from the system, which a
    // host without a window for it does not have: the host gives them.
    if (mouse_cap >= 0 && ps->m_mouse_cap) *ps->m_mouse_cap = (EEL_F) mouse_cap;
    sx->gfx_runCode(0);
    if (ps->m_framebuffer_dirty) dr = 1;
    ps->m_framebuffer_dirty = 0;
    if (ps->m_framebuffer)
    {
      if (pixels) *pixels = (const unsigned int *) ps->m_framebuffer->getBits();
      if (rowspan) *rowspan = ps->m_framebuffer->getRowSpan();
      if (flipped) *flipped = ps->m_framebuffer->isFlipped();
    }
  }
  sx->m_init_mutex.Leave();
  sx->m_in_gfx--;
  return dr;
}


/// The mirror (FxmeNet's doc/jsfx-plan.md, phase 3): the audio instance on
/// the server and a display instance on the client, of the same script.

void jsfx_embed_set_display(SX_Instance *sx)
{
  // @init, @slider, @block and @sample never run here: their results come
  // from the other side. gfx_runCode() refuses to run while an init is
  // pending, and gfx_frame would run one.
  if (sx) sx->m_need_init = 0;
}

void jsfx_embed_gfx_request(SX_Instance *sx, int *w, int *h, int *hz)
{
  if (w) *w = sx ? sx->m_gfx_reqw : 0;
  if (h) *h = sx ? sx->m_gfx_reqh : 0;
  if (hz) *hz = sx ? sx->m_gfx_hz : 0;
}

void jsfx_embed_lock(SX_Instance *sx) { if (sx) sx->m_init_mutex.Enter(); }
void jsfx_embed_unlock(SX_Instance *sx) { if (sx) sx->m_init_mutex.Leave(); }

struct jsfx_embed_enum_ctx
{
  void (*fn)(void *, const char *, double *);
  void *ctx;
};

static int jsfx_embed_enum_one(const char *name, EEL_F *val, void *c)
{
  jsfx_embed_enum_ctx *e = (jsfx_embed_enum_ctx *) c;
  if (name && val) e->fn(e->ctx, name, val);
  return 1;
}

void jsfx_embed_enum_vars(SX_Instance *sx, void (*fn)(void *ctx, const char *name, double *value), void *ctx)
{
  if (!sx || !sx->m_vm || !fn) return;
  jsfx_embed_enum_ctx e = { fn, ctx };
  NSEEL_VM_enumallvars(sx->m_vm, jsfx_embed_enum_one, &e);
}

double *jsfx_embed_var(SX_Instance *sx, const char *name)
{
  return sx && sx->m_vm && name ? NSEEL_VM_regvar(sx->m_vm, name) : NULL;
}

double *jsfx_embed_mem_block(SX_Instance *sx, int block, bool allocate)
{
  if (!sx || !sx->m_vm || block < 0 || block >= NSEEL_RAM_BLOCKS) return NULL;
  int valid = 0;
  const unsigned int offs = (unsigned int) block * NSEEL_RAM_ITEMSPERBLOCK;
  EEL_F *p = allocate ? NSEEL_VM_getramptr(sx->m_vm, offs, &valid)
                      : NSEEL_VM_getramptr_noalloc(sx->m_vm, offs, &valid);
  return valid == NSEEL_RAM_ITEMSPERBLOCK ? p : NULL;
}

int jsfx_embed_mem_blocks_used(SX_Instance *sx, int *blocks, int max)
{
  if (!sx || !sx->m_vm) return 0;
  compileContext *c = (compileContext *) sx->m_vm;
  if (!c->ram_state) return 0;
  int n = 0;
  for (int b = 0; b < NSEEL_RAM_BLOCKS; b ++)
    if (c->ram_state->blocks[b])
    {
      if (n < max && blocks) blocks[n] = b;
      n++;
    }
  return n;
}

void jsfx_embed_gfx_wheel(SX_Instance *sx, double vertical, double horizontal)
{
  eel_lice_state *ps = sx ? sx->m_lice_state : NULL;
  if (!ps) return;
  if (ps->m_mouse_wheel) *ps->m_mouse_wheel += vertical;
  if (ps->m_mouse_hwheel) *ps->m_mouse_hwheel += horizontal;
}

void jsfx_embed_gfx_key(SX_Instance *sx, int code, int down)
{
  // as eel_lice's window procedure: the queue gfx_getchar() reads, and the
  // keys held, which gfx_getchar(key) tests
  eel_lice_state *ps = sx ? sx->m_lice_state : NULL;
  if (!ps || !code) return;

  int bare = code;
  if (bare >= ('u'<<24) && bare < ('v'<<24)) bare &= 0xffffff;
  const int lower = bare >= 1 && bare < 27 ? bare+'a'-1 : bare >= 'A' && bare <= 'Z' ? bare+'a'-'A' : bare;

  int *st = ps->hwnd_standalone_kb_state;
  const int n = (int) (sizeof(ps->hwnd_standalone_kb_state) / sizeof(ps->hwnd_standalone_kb_state[0]));
  int zp = n-1, x;
  for (x = 0; x < n && st[x] != lower; x ++) if (x < zp && !st[x]) zp = x;
  if (!down)
  {
    if (x < n) st[x] = 0;
    return;
  }
  if (x == n) st[zp] = lower;

  const int qsize = (int) (sizeof(ps->m_kb_queue)/sizeof(ps->m_kb_queue[0]));
  if (ps->m_kb_queue_valid >= qsize) // full: the oldest goes
  {
    ps->m_kb_queue_valid--;
    ps->m_kb_queue_pos++;
  }
  ps->m_kb_queue[(ps->m_kb_queue_pos + ps->m_kb_queue_valid++) & (qsize-1)] = code;
}

double *jsfx_embed_slider_var(SX_Instance *sx, int parm)
{
  if (!sx) return NULL;
  const int a = sx->parmToIndex(parm);
  effectSlider *slid = a >= 0 ? sx->m_sliders.Get(a) : NULL;
  return slid ? slid->slider : NULL;
}


/// gmem[]: the memory shared by the scripts of a process, named
/// (options:gmem=name: up to NSEEL_RAM_BLOCKS_GMEM_NAMED blocks) or not (one
/// buffer of NSEEL_SHARED_GRAM_SIZE items), seen here as blocks.

double *jsfx_embed_gmem_block(SX_Instance *sx, int block, bool allocate)
{
  if (!sx || !sx->m_vm || block < 0) return NULL;
  compileContext *c = (compileContext *) sx->m_vm;
  EEL_F ***named = (EEL_F ***) c->gram_blocks;
  const unsigned int offs = (unsigned int) block * NSEEL_RAM_ITEMSPERBLOCK;
  if (named)
  {
    if (block >= NSEEL_RAM_BLOCKS_GMEM_NAMED) return NULL;
    if (!allocate) return *named ? (*named)[block] : NULL;
    EEL_F *p = __NSEEL_RAMAllocGMEM(named, offs);
    return p == &nseel_ramalloc_onfail ? NULL : p;
  }
  if (offs >= NSEEL_SHARED_GRAM_SIZE) return NULL;
  if (!nseel_gmembuf_default)
  {
    if (!allocate) return NULL;
    __NSEEL_RAMAllocGMEM(NULL, 0);
  }
  return nseel_gmembuf_default ? nseel_gmembuf_default + offs : NULL;
}

int jsfx_embed_gmem_blocks_used(SX_Instance *sx, int *blocks, int max)
{
  if (!sx || !sx->m_vm) return 0;
  compileContext *c = (compileContext *) sx->m_vm;
  EEL_F ***named = (EEL_F ***) c->gram_blocks;
  int n = 0;
  const int count = named ? NSEEL_RAM_BLOCKS_GMEM_NAMED : NSEEL_SHARED_GRAM_SIZE / NSEEL_RAM_ITEMSPERBLOCK;
  for (int b = 0; b < count; b ++)
    if (named ? (*named && (*named)[b]) : nseel_gmembuf_default != NULL)
    {
      if (n < max && blocks) blocks[n] = b;
      n++;
    }
  return n;
}

bool jsfx_embed_slider_is_file(SX_Instance *sx, int parm)
{
  if (!sx) return false;
  const int a = sx->parmToIndex(parm);
  effectSlider *slid = a >= 0 ? sx->m_sliders.Get(a) : NULL;
  return slid && slid->is_file_slider;
}

bool jsfx_embed_slider_shown(SX_Instance *sx, int parm)
{
  if (!sx) return true;
  const int a = sx->parmToIndex(parm);
  effectSlider *slid = a >= 0 ? sx->m_sliders.Get(a) : NULL;
  return !slid || slid->show != 0;
}

/// Menus, cursors and dropped files for a display without a window.


extern int (*eel_lice_embed_showmenu)(void *user_ctx, const char *menu, int x, int y);
extern void (*eel_lice_embed_setcursor)(void *user_ctx, int resource_id, const char *name);

// <map> cannot be used here (curses.h's macros): WDL's keyed array. Used
// from the thread that runs @gfx only.
static void jsfx_embed_free_host(jsfx_embed_gfx_host *h) { delete h; }
static WDL_PtrKeyedArray<jsfx_embed_gfx_host *> s_gfx_hosts(jsfx_embed_free_host);

static int jsfx_embed_showmenu_hook(void *user_ctx, const char *menu, int x, int y)
{
  jsfx_embed_gfx_host *h = s_gfx_hosts.Get((INT_PTR) user_ctx);
  return h && h->showmenu ? h->showmenu(h->ctx, menu, x, y) : 0;
}

static void jsfx_embed_setcursor_hook(void *user_ctx, int resource_id, const char *name)
{
  jsfx_embed_gfx_host *h = s_gfx_hosts.Get((INT_PTR) user_ctx);
  if (h && h->setcursor) h->setcursor(h->ctx, resource_id, name);
}

void jsfx_embed_set_gfx_host(SX_Instance *sx, const jsfx_embed_gfx_host *host)
{
  if (!sx) return;
  eel_lice_embed_showmenu = jsfx_embed_showmenu_hook;
  eel_lice_embed_setcursor = jsfx_embed_setcursor_hook;
  s_gfx_hosts.Delete((INT_PTR) sx);
  if (host) s_gfx_hosts.Insert((INT_PTR) sx, new jsfx_embed_gfx_host(*host));
}

void jsfx_embed_gfx_drop(SX_Instance *sx, const char *path)
{
  eel_lice_state *ps = sx ? sx->m_lice_state : NULL;
  if (!ps) return;
  if (!path) ps->m_ddrop_files.Empty(true, free);
  else ps->m_ddrop_files.Add(strdup(path));
}
