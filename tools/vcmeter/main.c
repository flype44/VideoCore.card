/*
    vcmeter - a small Grafana-like meter in a Picasso96 memory window (PiP) of VideoCore.card.

    Two graphs with a history that scrolls to the left (68k MIPS, efficiency = 68k instructions per ARM cycle) and
    three readouts (core temperature, core voltage, ARM counter / ARM clock in GHz). The Emu68 counters are read like
    EmuControl does (movec between SuperState() and UserState()), the temperature, voltage and clock through
    mailbox.resource (about 90 us a call: once a second only).

    The window is drawn in a back buffer in system memory, then copied to the video memory of the PiP right after
    the vertical blank (WaitBOVP). Pixels are written directly, no RastPort call, with a small 5x7 font of its own.

    Arguments:
      SECS=n        run for n seconds (default 0: until closed)
      FMT=BGRA|ARGB pixel format of the window (default BGRA)
      VBL=n         refresh every n vertical blanks (default 3: 20 per second at 60 Hz)
      COLS=n        graph columns per second (default 10, at most one per refresh)
      NOBUF         draw straight into the PiP instead of the back buffer
      THEME=DEFAULT|DRACULA
      BORDERLESS    no frame (the default): the window follows the mouse while the left button is down, ESC or
                    Ctrl-C closes it
      FRAMED        a normal window with a title bar and gadgets instead
      X=n Y=n       start position of the window on the screen (default: where Intuition puts it)
      BRIGHTNESS=n  brightness of the window, 20 (dark) to 100 (default: normal). VideoCore.card takes
                    P96PIP_Brightness as the alpha of the plane, upside down. The HVS darkens the window (colours
                    times alpha, over the key colour that Picasso96 fills the area of a PiP with): it is not
                    a transparency over the desktop. The mouse wheel changes it by 5 (up: brighter), the
                    middle button sets it back to 100.

    A double click on the window switches between the framed and the borderless window.

    A demonstrator of the memory window of VideoCore.card (Picasso96 special feature), not part of the driver.
    Build it with the target vcmeter, see CMakeLists.txt.
*/

#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/picasso96.h>
#include <proto/mailbox.h>

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;
struct IntuitionBase *IntuitionBase;
struct Library *P96Base;
APTR MailboxBase;

static const char version[] __attribute__((used)) = "$VER: vcmeter 1.0";

#define WIDTH       336
#define HEIGHT      73
#define HISTORY     (WIDTH - 2)
#define PANELS      5

/* ---- Themes ---- */

struct Theme
{
    ULONG back;         /* between the panels */
    ULONG panel;
    ULONG frame;
    ULONG label;
    ULONG series[PANELS];
};

static const struct Theme themeDefault =
{
    0x0b0c0e, 0x14161a, 0x2a2d33, 0x9aa0a6,
    { 0x4fd1c5, 0xf6ad55, 0xfc8181, 0xb794f4, 0x63b3ed }
};

/* the Dracula palette: background darker, background, current line, foreground, green, orange, red, purple, cyan */
static const struct Theme themeDracula =
{
    0x21222c, 0x282a36, 0x44475a, 0xf8f8f2,
    { 0x50fa7b, 0xffb86c, 0xff5555, 0xbd93f9, 0x8be9fd }
};

static const struct Theme *theme = &themeDefault;

/* case insensitive comparison of an argument with an upper case name */
static BOOL IsName(const char *s, const char *name)
{
    while (*name)
    {
        char c = *s++;

        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        if (c != *name++)
            return FALSE;
    }

    return *s == 0;
}

/* ---- Emu68 counters, the low words are enough for deltas over a fraction of a second ---- */

static inline ULONG ReadCounter(ULONG which)
{
    ULONG res = 0;

    switch (which)
    {
        case 0: asm volatile("movec #0xe0, %0" : "=r"(res)); break;   /* frequency of the time counter, Hz */
        case 1: asm volatile("movec #0xe1, %0" : "=r"(res)); break;   /* time counter */
        case 2: asm volatile("movec #0xe3, %0" : "=r"(res)); break;   /* 68k instructions */
        case 3: asm volatile("movec #0xe5, %0" : "=r"(res)); break;   /* ARM cycles */
    }

    return res;
}

struct Counters
{
    ULONG freq;
    ULONG time;
    ULONG insn;
    ULONG cycles;
};

static void GetCounters(struct Counters *c)
{
    APTR ssp = SuperState();

    c->freq = ReadCounter(0);
    c->time = ReadCounter(1);
    c->insn = ReadCounter(2);
    c->cycles = ReadCounter(3);

    if (ssp)
        UserState(ssp);
}

/* ---- Mailbox ---- */

static ULONG MailboxValue(ULONG tag, ULONG arg)
{
    static ULONG req[8];

    req[0] = 4 * 8;
    req[1] = 0;
    req[2] = tag;
    req[3] = 8;
    req[4] = 0;
    req[5] = arg;
    req[6] = 0;
    req[7] = 0;

    MB_RawCommand(req);

    return req[6];
}

/* ---- Panels ---- */

struct Panel
{
    WORD x, y, w, h;
    const char *label;
    ULONG lo, hi;       /* range of the graph; hi == 0: automatic, from 0 */
    UWORD dec;          /* decimals of the value (stored as an integer, 1 = tenths, 2 = hundredths) */
    BOOL ratio;         /* the value is shown as "value/aux" */
    ULONG aux;
    UWORD count;
    ULONG value[HISTORY];
};

#define PANEL_MIPS      0
#define PANEL_EFF       1
#define PANEL_TEMP      2
#define PANEL_VOLT      3
#define PANEL_ARM       4

static struct Panel panels[PANELS] =
{
    {   0,  0, 167, 59, "MIPS 68K",      0,   0, 0 },
    { 169,  0, 167, 59, "EFFICIENCY %",  0, 100, 0 },
    {   0, 61, 114, 12, "TEMP CELSIUS",  0,   0, 1 },
    { 116, 61, 110, 12, "CORE VOLTAGE",  0,   0, 2 },
    { 228, 61, 108, 12, "ARM GHZ",       0,   0, 2, TRUE },
};

static ULONG Pow10(UWORD n)
{
    ULONG r = 1;

    while (n--)
        r *= 10;

    return r;
}

/* the value as text: an integer with 'dec' decimals, returns the length */
static int FormatValue(char *buf, ULONG v, UWORD dec)
{
    char tmp[12];
    int n = 0, len = 0;
    ULONG whole = v / Pow10(dec);
    ULONG frac = v % Pow10(dec);

    do
    {
        tmp[n++] = '0' + whole % 10;
        whole /= 10;
    } while (whole != 0);

    while (n > 0)
        buf[len++] = tmp[--n];

    if (dec != 0)
    {
        buf[len++] = '.';
        for (n = dec; n > 0; n--)
        {
            ULONG p = Pow10(n - 1);
            buf[len++] = '0' + frac / p;
            frac %= p;
        }
    }
    buf[len] = 0;

    return len;
}

/* ---- 5x7 font: digits, '.', '%', '/', space and the letters of the labels ---- */

static const char glyphChars[] = "0123456789.%/ACEFGHIKLMNOPRSTUVYZ ";

static const UBYTE glyphs[][7] =
{
    { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e },   /* 0 */
    { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e },   /* 1 */
    { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f },   /* 2 */
    { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e },   /* 3 */
    { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 },   /* 4 */
    { 0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e },   /* 5 */
    { 0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e },   /* 6 */
    { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },   /* 7 */
    { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e },   /* 8 */
    { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c },   /* 9 */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c },   /* . */
    { 0x19, 0x1a, 0x02, 0x04, 0x08, 0x0b, 0x13 },   /* % */
    { 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 },   /* / */
    { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },   /* A */
    { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e },   /* C */
    { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f },   /* E */
    { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 },   /* F */
    { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f },   /* G */
    { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 },   /* H */
    { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e },   /* I */
    { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 },   /* K */
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f },   /* L */
    { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 },   /* M */
    { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 },   /* N */
    { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },   /* O */
    { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 },   /* P */
    { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 },   /* R */
    { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e },   /* S */
    { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },   /* T */
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e },   /* U */
    { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 },   /* V */
    { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 },   /* Y */
    { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f },   /* Z */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   /* space */
};

#define GLYPH_W     6       /* 5 pixels and one of space */

/* ---- Drawing into the locked bitmap ---- */

static UBYTE *screen;
static ULONG pitch;
static ULONG format;        /* RGBFTYPE of the locked bitmap */

static inline ULONG Swap32(ULONG c)
{
    return (c << 24) | ((c & 0xff00) << 8) | ((c >> 8) & 0xff00) | (c >> 24);
}

/* 0x00RRGGBB to the pixel of the 32 bit format of the bitmap */
static ULONG PixelValue(ULONG c)
{
    switch (format)
    {
        case RGBFB_A8B8G8R8: return Swap32(c) >> 8;
        case RGBFB_R8G8B8A8: return c << 8;
        case RGBFB_B8G8R8A8: return Swap32(c);
        default:             return c;                  /* A8R8G8B8 */
    }
}

static inline ULONG *Pixel(LONG x, LONG y)
{
    return (ULONG *)(screen + y * pitch) + x;
}

static inline void Plot(LONG x, LONG y, ULONG color)
{
    *Pixel(x, y) = PixelValue(color);
}

static void FillRect(LONG x, LONG y, LONG w, LONG h, ULONG color)
{
    LONG i;

    color = PixelValue(color);

    for (; h > 0; h--, y++)
    {
        ULONG *p = Pixel(x, y);

        for (i = 0; i < w; i++)
            p[i] = color;
    }
}

static void DrawText(LONG x, LONG y, const char *s, ULONG color)
{
    for (; *s; s++, x += GLYPH_W)
    {
        const char *g = glyphChars;
        const UBYTE *rows;
        LONG r, c;

        while (*g && *g != *s)
            g++;
        if (*g == 0)
            continue;

        rows = glyphs[g - glyphChars];
        for (r = 0; r < 7; r++)
            for (c = 0; c < 5; c++)
                if (rows[r] & (0x10 >> c))
                    Plot(x + c, y + r, color);
    }
}

static int TextWidth(const char *s)
{
    int n = 0;

    while (s[n])
        n++;

    return n * GLYPH_W - 1;
}

/* top of the graph for a maximum: 1, 2 or 5 times a power of ten above it */
static ULONG NiceTop(ULONG max)
{
    ULONG p = 1;

    if (max < 10)
        return 10;

    while (p * 10 <= max)
        p *= 10;

    if (max <= p)
        return p;
    if (max <= 2 * p)
        return 2 * p;
    if (max <= 5 * p)
        return 5 * p;

    return 10 * p;
}

static void DrawPanel(struct Panel *p, ULONG color)
{
    LONG gx = p->x + 1, gy = p->y + 10, gw = p->w - 2, gh = p->h - 11;
    ULONG lo = p->lo, hi = p->hi;
    ULONG fill = (color & 0xfcfcfc) >> 2;
    char text[24];
    LONG i, y;

    if (hi == 0)
    {
        ULONG max = 0;

        for (i = 0; i < p->count; i++)
            if (p->value[i] > max)
                max = p->value[i];

        hi = NiceTop(max);
    }

    /* frame, background, label and value */
    FillRect(p->x, p->y, p->w, p->h, theme->frame);
    FillRect(p->x + 1, p->y + 1, p->w - 2, p->h - 2, theme->panel);
    DrawText(p->x + 4, p->y + 2, p->label, theme->label);

    if (p->count != 0)
    {
        int len = FormatValue(text, p->value[p->count - 1], p->dec);

        if (p->ratio)
        {
            text[len++] = '/';
            FormatValue(text + len, p->aux, p->dec);
        }
        DrawText(p->x + p->w - 4 - TextWidth(text), p->y + 2, text, color);
    }

    /* a thin panel is a readout only, no graph */
    if (p->h < 20)
        return;

    /* grid: top, middle and bottom of the graph */
    FillRect(gx, gy, gw, 1, theme->frame);
    FillRect(gx, gy + gh / 2, gw, 1, theme->frame);

    /* the history, the newest sample at the right edge: a line over a dim area */
    for (i = 0; i < p->count; i++)
    {
        ULONG v = p->value[p->count - 1 - i];
        LONG x = gx + gw - 1 - i;
        LONG h;

        v = v > lo ? v - lo : 0;
        h = v * (gh - 1) / (hi - lo);
        if (h > gh - 1)
            h = gh - 1;

        Plot(x, gy + gh - 1 - h, color);
        for (y = gy + gh - h; y < gy + gh; y++)
            Plot(x, y, fill);
    }
}

static void Render(struct BitMap *bm)
{
    struct RenderInfo ri;
    LONG lock = p96LockBitMap(bm, (UBYTE *)&ri, sizeof(ri));
    int i;

    screen = ri.Memory;
    pitch = ri.BytesPerRow;
    format = ri.RGBFormat;

    FillRect(0, 0, WIDTH, HEIGHT, theme->back);
    for (i = 0; i < PANELS; i++)
        DrawPanel(&panels[i], theme->series[i]);

    p96UnlockBitMap(bm, lock);
}

static void Push(struct Panel *p, ULONG v)
{
    LONG i;

    if (p->count < p->w - 2)
        p->count++;
    else
        for (i = 0; i < p->count - 1; i++)
            p->value[i] = p->value[i + 1];

    p->value[p->count - 1] = v;
}

/* a readout: the value only, no history */
static void SetValue(struct Panel *p, ULONG v)
{
    p->count = 1;
    p->value[0] = v;
}

/* the values read through the mailbox, once a second */
static void ReadMailbox(void)
{
    if (MailboxBase == NULL)
        return;

    SetValue(&panels[PANEL_TEMP], MailboxValue(0x00030006, 0) / 100);          /* milli degrees -> tenths */
    SetValue(&panels[PANEL_VOLT], MailboxValue(0x00030003, 1) / 10000);        /* micro volts -> hundredths */
    panels[PANEL_ARM].aux = (MailboxValue(0x00030047, 3) + 5000000) / 10000000;   /* Hz -> hundredths of GHz */
}

/* ---- Main ---- */

#define ARG_SECS        0
#define ARG_FMT         1
#define ARG_COLS        2
#define ARG_NOBUF       3
#define ARG_VBL         4
#define ARG_THEME       5
#define ARG_BORDERLESS  6
#define ARG_FRAMED      7
#define ARG_X           8
#define ARG_Y           9
#define ARG_DEBUG       10
#define ARG_BRIGHTNESS  11

static ULONG brightness = 0;    /* P96PIP_Brightness as VideoCore.card takes it: 0 is full brightness, see BRIGHTNESS */
static ULONG brightnessPercent = 100;

#define BRIGHTNESS_STEP 5       /* percent for a notch of the mouse wheel */
#define BRIGHTNESS_MIN  20      /* the window is never darker than that */

/* VideoCore.card takes P96PIP_Brightness the other way round: 0 is the default, full brightness, and 0xffffffff is
   black (Picasso96 sends 0 when a program says nothing) */
static ULONG BrightnessValue(ULONG percent)
{
    return (100 - percent) * 42949672;
}

/* The PiP window and what comes with it. A double click closes it and opens the other kind (a frame cannot be
   added to or taken off an open window) */
struct Meter
{
    struct Window *win;
    struct BitMap *bm;          /* source bitmap of the PiP, in video memory */
    struct RastPort *rp;
    struct ViewPort *vp;
    BOOL borderless;
};

static void CloseMeter(struct Meter *m)
{
    if (m->win == NULL)
        return;

    p96PIP_Close(m->win);
    m->win = NULL;
}

static BOOL OpenMeter(struct Meter *m, BOOL borderless, BOOL hasLeft, LONG left, BOOL hasTop, LONG top,
                      ULONG srcFormat, LONG *error)
{
    struct TagItem tags[] =
    {
        { P96PIP_SourceWidth,  WIDTH },
        { P96PIP_SourceHeight, HEIGHT },
        { P96PIP_SourceFormat, srcFormat },
        { P96PIP_Type,         P96PIPT_MemoryWindow },
        { P96PIP_ErrorCode,    (ULONG)error },
        { P96PIP_Brightness,   brightness },
        { WA_InnerWidth,       WIDTH },
        { WA_InnerHeight,      HEIGHT },
        { hasLeft ? WA_Left : TAG_IGNORE, left },
        { hasTop ? WA_Top : TAG_IGNORE, top },
        { WA_Activate,         TRUE },
        { WA_RMBTrap,          TRUE },
        { WA_Borderless,       borderless },
        { WA_DragBar,          !borderless },
        { WA_DepthGadget,      !borderless },
        { WA_CloseGadget,      !borderless },
        { WA_SimpleRefresh,    TRUE },
        { WA_IDCMP,            IDCMP_CLOSEWINDOW | IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS },
        { WA_PubScreenName,    (ULONG)"Workbench" },
        { WA_Title,            (ULONG)(borderless ? NULL : "VideoCore Meter") },
        { TAG_DONE, 0 }
    };

    m->win = p96PIP_OpenTagList(tags);
    if (m->win == NULL)
        return FALSE;

    m->borderless = borderless;

    m->bm = NULL;
    m->rp = NULL;
    p96PIP_GetTags(m->win, P96PIP_SourceBitMap, (ULONG)&m->bm, TAG_END);
    p96PIP_GetTags(m->win, P96PIP_SourceRPort, (ULONG)&m->rp, TAG_END);
    if (m->bm == NULL || m->rp == NULL)
    {
        CloseMeter(m);
        return FALSE;
    }

    m->vp = &m->win->WScreen->ViewPort;

    return TRUE;
}

/* the contents of the window at x, y of the screen, whatever the frame is */
static void PlaceMeter(struct Meter *m, LONG x, LONG y)
{
    LONG dx = x - (m->win->LeftEdge + m->win->BorderLeft);
    LONG dy = y - (m->win->TopEdge + m->win->BorderTop);

    if (dx != 0 || dy != 0)
        MoveWindow(m->win, dx, dy);
}

/* the other kind of window, the contents staying where they are; when it cannot be opened the old kind comes
   back, FALSE when neither can */
static BOOL SwitchFrame(struct Meter *m, ULONG srcFormat)
{
    LONG x = m->win->LeftEdge + m->win->BorderLeft;
    LONG y = m->win->TopEdge + m->win->BorderTop;
    BOOL borderless = !m->borderless;
    LONG error = 0;

    CloseMeter(m);

    if (!OpenMeter(m, borderless, TRUE, x, TRUE, y, srcFormat, &error) &&
        !OpenMeter(m, !borderless, TRUE, x, TRUE, y, srcFormat, &error))
        return FALSE;

    PlaceMeter(m, x, y);

    return TRUE;
}

int main(void)
{
    struct RDArgs *rda;
    LONG args[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    BPTR logFile = 0;                           /* DEBUG: the messages received, in T:vcmeter.log */
    LONG left = 0, top = 0;                     /* start position, when given */
    BOOL hasLeft = FALSE, hasTop = FALSE;
    ULONG srcFormat = RGBFB_B8G8R8A8;
    ULONG secs = 0, vbl, columnCount = 0, refreshCount = 0;    /* secs 0: until closed */
    ULONG columns = 10;                         /* new graph columns per second */
    ULONG vblPerRefresh = 3;                    /* refresh every n vertical blanks */
    ULONG refreshPerColumn, columnsPerSecond;
    BOOL noBuffer = FALSE, borderless = TRUE;
    int result = 5;
    LONG error = 0;
    struct Meter meter;
    struct BitMap *back = NULL;
    struct Counters old, now;
    BOOL closing = FALSE;
    BOOL haveClick = FALSE;                     /* a first click, waiting for the second */
    ULONG clickSecs = 0, clickMicros = 0;
    BOOL dragging = FALSE;                      /* a borderless window follows the mouse while the button is down */
    LONG dragX = 0, dragY = 0;                  /* where the mouse was, on the screen */

    meter.win = NULL;

    SysBase = *(struct ExecBase **)4;
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 37);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
    P96Base = OpenLibrary("Picasso96API.library", 2);
    MailboxBase = OpenResource("mailbox.resource");

    if (DOSBase == NULL || GfxBase == NULL || IntuitionBase == NULL || P96Base == NULL)
    {
        Printf("cannot open the libraries\n");
        goto out;
    }

    if (MailboxBase == NULL)
        Printf("no mailbox.resource: temperature, voltage and clock are not available\n");

    rda = ReadArgs("SECS/N,FMT/K,COLS/N,NOBUF/S,VBL/N,THEME/K,BORDERLESS/S,FRAMED/S,X/N,Y/N,DEBUG/S,BRIGHTNESS/N", args, NULL);
    if (rda != NULL)
    {
        if (args[ARG_SECS] != 0)
            secs = *(LONG *)args[ARG_SECS];
        if (args[ARG_FMT] != 0 && IsName((const char *)args[ARG_FMT], "ARGB"))
            srcFormat = RGBFB_A8R8G8B8;
        if (args[ARG_COLS] != 0)
        {
            columns = *(LONG *)args[ARG_COLS];
            if (columns < 1)
                columns = 1;
            if (columns > 60)
                columns = 60;
        }
        noBuffer = args[ARG_NOBUF] != 0;
        if (args[ARG_VBL] != 0)
        {
            vblPerRefresh = *(LONG *)args[ARG_VBL];
            if (vblPerRefresh < 1)
                vblPerRefresh = 1;
            if (vblPerRefresh > 60)
                vblPerRefresh = 60;
        }
        if (args[ARG_X] != 0)
        {
            left = *(LONG *)args[ARG_X];
            hasLeft = TRUE;
        }
        if (args[ARG_Y] != 0)
        {
            top = *(LONG *)args[ARG_Y];
            hasTop = TRUE;
        }
        if (args[ARG_BRIGHTNESS] != 0)
        {
            brightnessPercent = *(LONG *)args[ARG_BRIGHTNESS];
            if (brightnessPercent < BRIGHTNESS_MIN)
                brightnessPercent = BRIGHTNESS_MIN;
            if (brightnessPercent > 100)
                brightnessPercent = 100;
            brightness = BrightnessValue(brightnessPercent);
        }
        if (args[ARG_DEBUG] != 0)
            logFile = Open("T:vcmeter.log", MODE_NEWFILE);
        if (args[ARG_FRAMED] != 0)
            borderless = FALSE;

        if (args[ARG_THEME] != 0)
        {
            const char *name = (const char *)args[ARG_THEME];

            if (IsName(name, "DRACULA"))
                theme = &themeDracula;
            else if (!IsName(name, "DEFAULT"))
                Printf("unknown theme, using DEFAULT (DEFAULT or DRACULA)\n");
        }
        FreeArgs(rda);
    }

    if (!OpenMeter(&meter, borderless, hasLeft, left, hasTop, top, srcFormat, &error))
    {
        Printf("p96PIP_OpenTags failed, error %ld\n", error);
        goto out;
    }

    /* the back buffer, in system memory, same format as the window: drawn while the beam is busy elsewhere,
       copied to the window right after the vertical blank */
    if (!noBuffer)
    {
        back = p96AllocBitMap(WIDTH, HEIGHT, 32, BMF_CLEAR | BMF_USERPRIVATE, NULL, srcFormat);
        if (back == NULL)
        {
            Printf("no back buffer\n");
            goto closewin;
        }
    }

    ReadMailbox();
    GetCounters(&old);

    /* one refresh every 'vblPerRefresh' vertical blanks; a new graph column at most at each refresh */
    refreshPerColumn = 60 / columns / vblPerRefresh;
    if (refreshPerColumn == 0)
        refreshPerColumn = 1;
    columnsPerSecond = 60 / (vblPerRefresh * refreshPerColumn);
    if (columnsPerSecond == 0)
        columnsPerSecond = 1;

    for (vbl = 0; (secs == 0 || vbl < secs * 60) && !closing; vbl++)
    {
        struct IntuiMessage *msg;
        BOOL refresh = vbl % vblPerRefresh == 0;
        BOOL doubleClick = FALSE;
        ULONG oldPercent = brightnessPercent;

        while ((msg = (struct IntuiMessage *)GetMsg(meter.win->UserPort)) != NULL)
        {
            ULONG class = msg->Class;
            UWORD code = msg->Code;
            ULONG seconds = msg->Seconds;
            ULONG micros = msg->Micros;
            LONG mouseX = meter.win->LeftEdge + msg->MouseX;        /* on the screen */
            LONG mouseY = meter.win->TopEdge + msg->MouseY;

            ReplyMsg((struct Message *)msg);

            if (logFile)
            {
                FPrintf(logFile, "class %lx code %lx sec %lu micros %lu\n", (LONG)class, (LONG)code, (LONG)seconds,
                        (LONG)micros);
                Flush(logFile);
            }

            if (class == IDCMP_CLOSEWINDOW)
                closing = TRUE;
            else if (class == IDCMP_RAWKEY && code == 0x45)         /* ESC */
                closing = TRUE;
            else if (class == IDCMP_RAWKEY && code == 0x7a)         /* wheel up: brighter */
                brightnessPercent = brightnessPercent + BRIGHTNESS_STEP > 100 ? 100 : brightnessPercent + BRIGHTNESS_STEP;
            else if (class == IDCMP_RAWKEY && code == 0x7b)         /* wheel down: darker */
                brightnessPercent = brightnessPercent > BRIGHTNESS_MIN + BRIGHTNESS_STEP ? brightnessPercent - BRIGHTNESS_STEP : BRIGHTNESS_MIN;
            else if (class == IDCMP_RAWKEY && code == 0x6a)         /* middle button: back to full brightness */
                brightnessPercent = 100;
            else if (class == IDCMP_MOUSEBUTTONS && code == SELECTDOWN)
            {
                BOOL isDouble = haveClick && DoubleClick(clickSecs, clickMicros, seconds, micros);

                if (logFile)
                {
                    FPrintf(logFile, "  SELECTDOWN haveClick %ld DoubleClick %ld\n", (LONG)haveClick, (LONG)isDouble);
                    Flush(logFile);
                }

                if (isDouble)
                {
                    doubleClick = TRUE;
                    haveClick = FALSE;
                }
                else
                {
                    clickSecs = seconds;
                    clickMicros = micros;
                    haveClick = TRUE;
                }

                /* a borderless window has no drag bar, and a system gadget over it takes the clicks: it is
                   moved by hand, by the distance the mouse has travelled */
                dragging = meter.borderless;
                dragX = mouseX;
                dragY = mouseY;
            }
            else if (class == IDCMP_MOUSEBUTTONS && code == SELECTUP)
                dragging = FALSE;
        }

        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C)
            closing = TRUE;

        /* the wheel or the middle button changed the brightness: the driver changes the alpha of the plane in place */
        if (brightnessPercent != oldPercent && !closing)
        {
            brightness = BrightnessValue(brightnessPercent);
            p96PIP_SetTags(meter.win, P96PIP_Brightness, brightness, TAG_END);
        }

        /* a double click: the other kind of window */
        if (doubleClick && !closing)
        {
            dragging = FALSE;
            if (!SwitchFrame(&meter, srcFormat))
            {
                Printf("cannot reopen the window\n");
                break;
            }
        }

        if (dragging && !closing)
        {
            LONG x = meter.win->WScreen->MouseX;
            LONG y = meter.win->WScreen->MouseY;

            if (x != dragX || y != dragY)
            {
                MoveWindow(meter.win, x - dragX, y - dragY);
                dragX = x;
                dragY = y;
            }
        }

        /* a new column of the graphs */
        if (refresh && refreshCount++ % refreshPerColumn == 0)
        {
            ULONG us, mips, eff, arm;

            GetCounters(&now);

            us = (now.time - old.time) / (now.freq / 1000000);
            if (us == 0)
                us = 1;

            mips = (now.insn - old.insn) / us;
            eff = (now.insn - old.insn) / ((now.cycles - old.cycles) / 100 + 1);
            arm = ((now.cycles - old.cycles) / us + 5) / 10;        /* ARM cycles per us = MHz -> hundredths of GHz */
            old = now;

            Push(&panels[PANEL_MIPS], mips);
            Push(&panels[PANEL_EFF], eff);
            SetValue(&panels[PANEL_ARM], arm);

            if (++columnCount % columnsPerSecond == 0)
                ReadMailbox();
        }

        /* draw during the frame, copy right after the next vertical blank; the other blanks only count */
        if (refresh)
            Render(noBuffer ? meter.bm : back);
        WaitBOVP(meter.vp);
        if (refresh && !noBuffer)
            BltBitMapRastPort(back, 0, 0, meter.rp, 0, 0, WIDTH, HEIGHT, 0xc0);
    }

    result = 0;

closewin:
    CloseMeter(&meter);
    if (back != NULL)
        p96FreeBitMap(back);
    if (logFile)
        Close(logFile);

out:
    if (P96Base != NULL)
        CloseLibrary(P96Base);
    if (GfxBase != NULL)
        CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase != NULL)
        CloseLibrary((struct Library *)IntuitionBase);
    if (DOSBase != NULL)
        CloseLibrary((struct Library *)DOSBase);

    return result;
}
