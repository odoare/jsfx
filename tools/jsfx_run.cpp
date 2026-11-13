/*
 * jsfx_run: a test of JSFX as a library (jsfx/jsfx_embed.h). Loads a script,
 * lists its sliders, runs a sine through it and, with --gfx, draws one frame
 * of its @gfx to a PPM file.
 *
 *   jsfx_run ROOT EFFECT [N=VALUE ...] [--seconds S] [--block B] [--gfx W H out.ppm] [--mirror]
 *
 * --mirror draws @gfx in a second instance of the script, a display one
 * (jsfx_embed_set_display: no audio code runs in it), after copying the
 * variables and the memory the display touches from the audio instance, as
 * FxmeNet's client does over the network.
 *
 * ROOT holds Effects/ and Data/ (REAPER's resource folder does), EFFECT is a
 * path under Effects/, N a slider's number as sx_getParmName() counts them
 * (from 0). Example:
 *
 *   jsfx_run ~/.config/REAPER utility/volume 0=-6
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

class SX_Instance;
#include "../jsfx/jsfx_embed.h"

static double rms(const std::vector<double> &v, int nch, int ch)
{
  double s = 0.0;
  size_t n = 0;
  for (size_t i = (size_t) ch; i < v.size(); i += (size_t) nch) { s += v[i]*v[i]; n++; }
  return n ? sqrt(s / (double) n) : 0.0;
}

int main(int argc, char **argv)
{
  if (argc < 3)
  {
    fprintf(stderr, "usage: jsfx_run ROOT EFFECT [N=VALUE ...] [--seconds S] [--block B] [--gfx W H out.ppm]\n");
    return 2;
  }

  const int srate = 48000, nch = 2;
  double seconds = 1.0;
  int block = 128, gfxw = 0, gfxh = 0;
  const char *gfxout = NULL;
  bool mirror = false;

  char err[1024];
  SX_Instance *sx = jsfx_embed_create(argv[1], argv[2], srate, err, sizeof(err));
  if (!sx)
  {
    fprintf(stderr, "%s: %s\n", argv[2], err);
    return 1;
  }

  for (int i = 3; i < argc; i++)
  {
    const char *eq = strchr(argv[i], '=');
    if (!strcmp(argv[i], "--seconds") && i+1 < argc) seconds = atof(argv[++i]);
    else if (!strcmp(argv[i], "--block") && i+1 < argc) block = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--gfx") && i+3 < argc) { gfxw = atoi(argv[i+1]); gfxh = atoi(argv[i+2]); gfxout = argv[i+3]; i += 3; }
    else if (!strcmp(argv[i], "--mirror")) mirror = true;
    else if (eq) sx_setParmVal(sx, atoi(argv[i]), atof(eq+1), 0);
    else { fprintf(stderr, "unknown argument %s\n", argv[i]); return 2; }
  }
  if (block < 1) block = 128;

  int nin = 0, nout = 0;
  sx_getPinInfo(sx, 0, &nin);
  sx_getPinInfo(sx, 1, &nout);
  printf("%s\n  @gfx: %s, pins: %d in, %d out\n", jsfx_embed_description(sx),
         jsfx_embed_has_gfx(sx) ? "yes" : "no", nin, nout);

  const int np = sx_getNumParms(sx);
  for (int p = 0; p < np; p++)
  {
    char name[256], disp[256];
    double mn = 0, mx = 0, step = 0;
    const double v = sx_getParmVal(sx, p, &mn, &mx, &step);
    sx_getParmName(sx, p, name, sizeof(name));
    disp[0] = 0;
    sx_getParmDisplay(sx, p, disp, sizeof(disp), NULL);
    printf("  %2d  %-32s %g  [%g .. %g, step %g]  %s%s\n", p, name, v, mn, mx, step, disp,
           sx_parmIsEnum(sx, p) ? "  (enum)" : "");
  }

  // a 440 Hz sine at -12 dB on both channels
  const int total = (int) (seconds * srate);
  std::vector<double> in((size_t) total * nch), out;
  for (int i = 0; i < total; i++)
    in[(size_t) i*nch] = in[(size_t) i*nch + 1] = 0.25 * sin(2.0 * M_PI * 440.0 * i / srate);
  out = in;

  for (int pos = 0; pos < total; pos += block)
  {
    const int n = total - pos < block ? total - pos : block;
    sx_processSamples(sx, &out[(size_t) pos*nch], n, nch, srate,
                      120.0, 4, 4, 1.0, (double) pos / srate, (double) pos / srate * 2.0, 1.0, 1.0, 0);
  }

  printf("  %d samples in blocks of %d, latency %d\n", total, block, sx_getCurrentLatency(sx));
  printf("  rms in  %.6f %.6f\n  rms out %.6f %.6f\n", rms(in, nch, 0), rms(in, nch, 1), rms(out, nch, 0), rms(out, nch, 1));

  if (gfxout)
  {
    const unsigned int *px = NULL;
    int span = 0;
    bool flipped = false;
    int r = -1;
    SX_Instance *view = sx;
    if (mirror)
    {
      view = jsfx_embed_create(argv[1], argv[2], srate, err, sizeof(err));
      if (!view) { fprintf(stderr, "display: %s\n", err); return 1; }
      jsfx_embed_set_display(view);
    }
    for (int frame = 0; frame < 3; frame++)   // scripts often lay out on the first frames
    {
      if (mirror)
      {
        // variables by name, then every block the display has touched so far
        struct Copy { SX_Instance *to; int n; } c = { view, 0 };
        jsfx_embed_enum_vars(sx, [](void *ctx, const char *name, double *value) {
          Copy *c = (Copy *) ctx;
          if (double *d = jsfx_embed_var(c->to, name)) { *d = *value; c->n++; }
        }, &c);
        int blocks[256];
        const int nb = jsfx_embed_mem_blocks_used(view, blocks, 256);
        int copied = 0;
        for (int b = 0; b < nb && b < 256; b++)
        {
          const double *from = jsfx_embed_mem_block(sx, blocks[b], false);
          double *to = jsfx_embed_mem_block(view, blocks[b], true);
          if (from && to) { memcpy(to, from, sizeof(double) * jsfx_embed_mem_block_size); copied++; }
        }
        printf("  frame %d: %d variables, %d of %d touched blocks copied\n", frame, c.n, copied, nb);
      }
      r = jsfx_embed_gfx_frame(view, gfxw, gfxh, gfxw/2, gfxh/2, 0, 256, &px, &span, &flipped);
    }
    if (r < 0 || !px)
    {
      fprintf(stderr, "no @gfx frame\n");
      return 1;
    }
    FILE *fp = fopen(gfxout, "wb");
    if (!fp) { fprintf(stderr, "cannot write %s\n", gfxout); return 1; }
    fprintf(fp, "P6\n%d %d\n255\n", gfxw, gfxh);
    for (int y = 0; y < gfxh; y++)
    {
      const unsigned int *row = px + (size_t) (flipped ? gfxh-1-y : y) * span;
      for (int x = 0; x < gfxw; x++)
      {
        const unsigned char rgb[3] = { (unsigned char) (row[x] >> 16), (unsigned char) (row[x] >> 8), (unsigned char) row[x] };
        fwrite(rgb, 1, 3, fp);
      }
    }
    fclose(fp);
    printf("  @gfx frame %d x %d written to %s\n", gfxw, gfxh, gfxout);
  }

  sx_destroyInstance(sx);
  return 0;
}
