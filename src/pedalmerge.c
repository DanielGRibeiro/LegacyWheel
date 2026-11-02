/*
 * LegacyWheel - proxy dinput.dll / dinput8.dll para jogos antigos (32 e 64 bits).
 * Nasceu para o Rig'n'Roll, que so aceita um controle. Este proxy:
 *   - esconde ate 6 dispositivos extras (pedais, aro, cambio...) na enumeracao;
 *   - quando o jogo le o volante (GetDeviceState), injeta os eixos dos extras
 *     em eixos do volante e soma os botoes deles aos do volante;
 *   - corrige, limita e para no tempo certo os efeitos de Force Feedback;
 *   - repassa todo o resto para o DirectInput real.
 *
 * O mesmo codigo gera dinput.dll (DirectInput 1-7) e dinput8.dll; o .def
 * escolhe as exportacoes. Configuracao em pedalmerge.ini (nome mantido por
 * compatibilidade com instalacoes antigas).
 */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#define INITGUID
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#define logf pm_logf  /* math.h ja tem um logf */

#define NAXES 8
#define NBTN 128
static const char *AXIS_NAMES[NAXES] = { "X", "Y", "Z", "RX", "RY", "RZ", "S0", "S1" };

typedef HRESULT (WINAPI *PFN_DI8Create)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
typedef HRESULT (WINAPI *PFN_DICreate)(HINSTANCE, DWORD, LPVOID *, LPUNKNOWN);
typedef HRESULT (WINAPI *PFN_DICreateEx)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);

static HMODULE g_self, g_real8, g_real7;
static PFN_DI8Create g_create8;
static char g_dir[MAX_PATH], g_modName[32];
static int g_log = 1;
static DWORD g_wheelId;              /* VID | PID << 16, igual a guidProduct.Data1 */
static CRITICAL_SECTION g_cs;

/* Force Feedback, em % */
static int g_ffSpring, g_ffDamper, g_ffFriction, g_ffInertia, g_ffConstant, g_ffPeriodic, g_ffRamp, g_ffGain;
static int g_ffSpringOffsetZero, g_ffAutocenterOff, g_ffSpringGainMax, g_ffEnforceDuration;

/* Direcao: centro fisico (valor bruto 0..65535 do eixo X com o volante reto) e ganho */
static LONG g_steerCenter = 32768;
static double g_steerGain = 1.0;
static double g_steerCurve = 1.0;    /* expoente: <1 = mais sensivel perto do centro */
static double g_steerDeadzone = 0;   /* 0..0.5: salta a zona morta interna do jogo */
static int g_steerRotation = 900;    /* graus do FFBeast, so para o log */
static int g_logSteering;

/* ------------------------------------------------------------------ log */

static void logf(const char *fmt, ...)
{
    if (!g_log) return;
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s\\pedalmerge.log", g_dir);
    EnterCriticalSection(&g_cs);
    FILE *f = fopen(path, "a");
    if (f) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(f, "%02d:%02d:%02d.%03d [%s] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, g_modName);
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);
        fputc('\n', f);
        fclose(f);
    }
    LeaveCriticalSection(&g_cs);
}

static int loadReal8(void)
{
    if (g_create8) return 1;
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);  /* SysWOW64 para processo 32-bit */
    lstrcatA(path, "\\dinput8.dll");
    g_real8 = LoadLibraryA(path);
    if (g_real8) g_create8 = (PFN_DI8Create)GetProcAddress(g_real8, "DirectInput8Create");
    if (!g_create8) logf("ERRO: nao consegui carregar %s", path);
    return g_create8 != NULL;
}

static int loadReal7(void)
{
    if (g_real7) return 1;
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    lstrcatA(path, "\\dinput.dll");
    g_real7 = LoadLibraryA(path);
    if (!g_real7) logf("ERRO: nao consegui carregar %s", path);
    return g_real7 != NULL;
}

/* --------------------------------------------------- devices seen in enum */

static struct { GUID inst; DWORD id; } g_seen[32];
static int g_nseen;

static void rememberDevice(const GUID *inst, DWORD id)
{
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < g_nseen; i++)
        if (IsEqualGUID(&g_seen[i].inst, inst)) { LeaveCriticalSection(&g_cs); return; }
    if (g_nseen < 32) {
        g_seen[g_nseen].inst = *inst;
        g_seen[g_nseen].id = id;
        g_nseen++;
    }
    LeaveCriticalSection(&g_cs);
}

static DWORD productOf(const GUID *inst)
{
    for (int i = 0; i < g_nseen; i++)
        if (IsEqualGUID(&g_seen[i].inst, inst)) return g_seen[i].id;
    return 0;
}

/* ------------------------------------------------------- source devices */

/* Detector de oscilacao: conta inversoes de sentido (com histerese) por segundo
 * e grava no log quando um eixo vai e volta sozinho. */
typedef struct {
    char name[32];
    double ext, mn, mx;
    int dir, rev, lines;
    DWORD t0;
} Jit;
static Jit g_jitSteer = { "direcao (graus)" };

static void jitter(Jit *j, double v, double hyst, double minAmp)
{
    DWORD now = GetTickCount();
    if (!j->t0) { j->t0 = now; j->ext = j->mn = j->mx = v; return; }
    if (v < j->mn) j->mn = v;
    if (v > j->mx) j->mx = v;
    if (j->dir > 0) {
        if (v > j->ext) j->ext = v;
        else if (v < j->ext - hyst) { j->dir = -1; j->rev++; j->ext = v; }
    } else if (j->dir < 0) {
        if (v < j->ext) j->ext = v;
        else if (v > j->ext + hyst) { j->dir = 1; j->rev++; j->ext = v; }
    } else if (fabs(v - j->ext) > hyst) {
        j->dir = v > j->ext ? 1 : -1;
        j->ext = v;
    }
    if (now - j->t0 < 1000) return;
    if (j->rev >= 4 && j->mx - j->mn >= minAmp && j->lines < 300) {
        j->lines++;
        logf("OSCILACAO na entrada: %s %d inversoes/s, entre %.1f e %.1f",
             j->name, j->rev, j->mn, j->mx);
    }
    j->t0 = now;
    j->rev = 0;
    j->mn = j->mx = v;
}

/* Dispositivo extra (pedais, aro, cambio, freio de mao, button box) lido por baixo dos
 * panos via DirectInput 8. Ini novo: secoes [src1]..[src6]. Ini antigo: pedals/rim. */
#define NSRC 6
typedef struct {
    char name[16];
    DWORD id;
    int hide;
    int legacy;                      /* 1 = pedais do ini antigo, 2 = aro do ini antigo */
    int mapAxis[NAXES];              /* eixo do volante para cada eixo da fonte, ou -1 */
    int mapDir[NAXES];               /* 0 = eixo inteiro, +1 = centro->max, -1 = centro->min */
    int invert[NAXES];
    int buttons;                     /* 1 = soma os botoes nos do volante */
    int btnOffset;                   /* botao N da fonte -> botao N + offset do volante */
    IDirectInputDevice8W *dev;
    DWORD lastTry;
    int loggedFirst;
    Jit jit[NAXES];
} Src;
static Src g_src[NSRC];

typedef struct { LONG ax[NAXES]; BYTE btn[NBTN]; } SrcState;

static DIOBJECTDATAFORMAT g_srcObjs[NAXES + NBTN] = {
    { &GUID_XAxis,  0, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_YAxis,  4, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_ZAxis,  8, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_RxAxis, 12, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_RyAxis, 16, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_RzAxis, 20, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_Slider, 24, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_Slider, 28, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    /* botoes preenchidos em DllMain */
};
static DIDATAFORMAT g_srcFmt = {
    sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT), DIDF_ABSAXIS,
    sizeof(SrcState), NAXES + NBTN, g_srcObjs
};

static IDirectInput8W *g_srcDI;

typedef struct { DWORD id; GUID guid; int found; } FindCtx;

static BOOL CALLBACK findCb(LPCDIDEVICEINSTANCEW inst, LPVOID p)
{
    FindCtx *c = (FindCtx *)p;
    if (inst->guidProduct.Data1 == c->id) {
        c->guid = inst->guidInstance;
        c->found = 1;
        return DIENUM_STOP;
    }
    return DIENUM_CONTINUE;
}

static void openSrc(Src *s, HWND hwnd)
{
    if (!s->id) return;
    if (s->dev) {
        if (hwnd) {
            s->dev->lpVtbl->Unacquire(s->dev);
            s->dev->lpVtbl->SetCooperativeLevel(s->dev, hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
            s->dev->lpVtbl->Acquire(s->dev);
        }
        return;
    }
    /* nao procurar de novo a cada frame se o dispositivo estiver desligado */
    DWORD now = GetTickCount();
    if (s->lastTry && now - s->lastTry < 5000) return;
    s->lastTry = now;

    if (!loadReal8()) return;
    if (!g_srcDI && FAILED(g_create8(g_self, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&g_srcDI, NULL))) {
        g_srcDI = NULL;
        return;
    }
    FindCtx c = { s->id };
    g_srcDI->lpVtbl->EnumDevices(g_srcDI, DI8DEVCLASS_GAMECTRL, findCb, &c, DIEDFL_ATTACHEDONLY);
    if (!c.found) { logf("%s (%08lX) nao encontrados", s->name, s->id); return; }

    IDirectInputDevice8W *d;
    if (FAILED(g_srcDI->lpVtbl->CreateDevice(g_srcDI, &c.guid, &d, NULL))) { logf("Falha ao abrir %s", s->name); return; }
    d->lpVtbl->SetDataFormat(d, &g_srcFmt);
    if (hwnd) d->lpVtbl->SetCooperativeLevel(d, hwnd, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

    DIPROPRANGE r;
    r.diph.dwSize = sizeof r;
    r.diph.dwHeaderSize = sizeof r.diph;
    r.diph.dwObj = 0;
    r.diph.dwHow = DIPH_DEVICE;
    r.lMin = 0;
    r.lMax = 65535;
    d->lpVtbl->SetProperty(d, DIPROP_RANGE, &r.diph);

    DIPROPDWORD dz;
    dz.diph.dwSize = sizeof dz;
    dz.diph.dwHeaderSize = sizeof dz.diph;
    dz.diph.dwObj = 0;
    dz.diph.dwHow = DIPH_DEVICE;
    dz.dwData = 0;
    d->lpVtbl->SetProperty(d, DIPROP_DEADZONE, &dz.diph);

    HRESULT hr = d->lpVtbl->Acquire(d);
    s->dev = d;
    logf("%s abertos (hwnd=%p, Acquire=0x%08lX)", s->name, hwnd, hr);
}

static void openSources(HWND hwnd)
{
    for (int i = 0; i < NSRC; i++) openSrc(&g_src[i], hwnd);
}

static int readSrc(Src *s, SrcState *st)
{
    IDirectInputDevice8W *d = s->dev;
    if (!d) {
        openSrc(s, NULL);
        return 0;
    }
    d->lpVtbl->Poll(d);
    HRESULT hr = d->lpVtbl->GetDeviceState(d, sizeof *st, st);
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
        d->lpVtbl->Acquire(d);
        d->lpVtbl->Poll(d);
        hr = d->lpVtbl->GetDeviceState(d, sizeof *st, st);
    }
    return SUCCEEDED(hr);
}

/* --------------------------------------------------------------- config */

static int axisIndex(const char *s)
{
    for (int i = 0; i < NAXES; i++)
        if (!lstrcmpiA(s, AXIS_NAMES[i])) return i;
    return -1;
}

static DWORD parseVidPid(const char *s)
{
    unsigned vid = 0, pid = 0;
    if (sscanf(s, "%x:%x", &vid, &pid) != 2) return 0;
    return (vid & 0xFFFF) | ((pid & 0xFFFF) << 16);
}

/* [ffb], [steering], [map], [invert] e [rim]: relidos com o jogo aberto quando o ini muda.
 * Padroes seguros para direct drive caso falte alguma chave. */
static void loadTuning(const char *ini)
{
    char buf[64];
    for (int k = 0; k < NSRC; k++) {
        Src *s = &g_src[k];
        char sec[8], key[16];
        snprintf(sec, sizeof sec, "src%d", k + 1);
        for (int i = 0; i < NAXES; i++) {
            int axis = -1, dir = 0;
            if (s->legacy == 2) buf[0] = 0;
            else GetPrivateProfileStringA(s->legacy ? "map" : sec, AXIS_NAMES[i], "", buf, sizeof buf, ini);
            size_t n = strlen(buf);
            if (n && (buf[n - 1] == '+' || buf[n - 1] == '-')) {
                dir = buf[n - 1] == '+' ? 1 : -1;
                buf[n - 1] = 0;
            }
            if (buf[0]) axis = axisIndex(buf);
            s->mapDir[i] = dir;
            s->mapAxis[i] = axis;
            snprintf(key, sizeof key, "invert_%s", AXIS_NAMES[i]);
            s->invert[i] = s->legacy == 1 ? GetPrivateProfileIntA("invert", AXIS_NAMES[i], 0, ini)
                         : s->legacy == 2 ? 0 : GetPrivateProfileIntA(sec, key, 0, ini);
        }
        s->buttons = s->legacy == 1 ? 0 : s->legacy == 2 ? 1 : GetPrivateProfileIntA(sec, "buttons", 1, ini);
        s->btnOffset = s->legacy == 2 ? GetPrivateProfileIntA("rim", "button_offset", 0, ini)
                     : s->legacy == 1 ? 0 : GetPrivateProfileIntA(sec, "button_offset", 0, ini);
    }

    g_ffGain = GetPrivateProfileIntA("ffb", "gain", 100, ini);
    g_ffSpring = GetPrivateProfileIntA("ffb", "spring", 100, ini);
    g_ffDamper = GetPrivateProfileIntA("ffb", "damper", 50, ini);
    g_ffFriction = GetPrivateProfileIntA("ffb", "friction", 50, ini);
    g_ffInertia = GetPrivateProfileIntA("ffb", "inertia", 50, ini);
    g_ffConstant = GetPrivateProfileIntA("ffb", "constant", 100, ini);
    g_ffPeriodic = GetPrivateProfileIntA("ffb", "periodic", 25, ini);
    g_ffRamp = GetPrivateProfileIntA("ffb", "ramp", 50, ini);
    g_ffSpringOffsetZero = GetPrivateProfileIntA("ffb", "spring_offset_zero", 1, ini);
    g_ffAutocenterOff = GetPrivateProfileIntA("ffb", "autocenter_off", 1, ini);
    g_ffSpringGainMax = GetPrivateProfileIntA("ffb", "spring_gain_max", 1500, ini);
    g_ffEnforceDuration = GetPrivateProfileIntA("ffb", "enforce_duration", 1, ini);

    g_steerCenter = GetPrivateProfileIntA("steering", "center", 32768, ini);
    GetPrivateProfileStringA("steering", "gain", "1.0", buf, sizeof buf, ini);
    g_steerGain = atof(buf);
    if (g_steerGain <= 0) g_steerGain = 1.0;
    GetPrivateProfileStringA("steering", "curve", "1.0", buf, sizeof buf, ini);
    g_steerCurve = atof(buf);
    if (g_steerCurve < 0.2 || g_steerCurve > 3) g_steerCurve = 1.0;
    g_steerDeadzone = GetPrivateProfileIntA("steering", "deadzone", 0, ini) / 100.0;
    if (g_steerDeadzone < 0 || g_steerDeadzone > 0.5) g_steerDeadzone = 0;
    g_steerRotation = GetPrivateProfileIntA("steering", "rotation", 900, ini);
    g_logSteering = GetPrivateProfileIntA("options", "log_steering", 0, ini);
}

static void reloadTuningIfChanged(void)
{
    static DWORD lastCheck;
    static FILETIME lastWrite;
    DWORD now = GetTickCount();
    if (now - lastCheck < 1000) return;
    lastCheck = now;
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%s\\pedalmerge.ini", g_dir);
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExA(ini, GetFileExInfoStandard, &fa)) return;
    if (!lastWrite.dwLowDateTime && !lastWrite.dwHighDateTime) { lastWrite = fa.ftLastWriteTime; return; }
    if (!CompareFileTime(&lastWrite, &fa.ftLastWriteTime)) return;
    /* arquivo lido no meio de uma gravacao: tenta de novo no proximo segundo */
    char probe[8];
    if (!GetPrivateProfileStringA("ffb", "gain", "", probe, sizeof probe, ini)) return;
    lastWrite = fa.ftLastWriteTime;
    loadTuning(ini);
    logf("ini alterado, recarregado: direcao ganho=%.2f curva=%.2f zona_morta=%d%% | mola=%d%% (max %d) periodico=%d%%",
         g_steerGain, g_steerCurve, (int)(g_steerDeadzone * 100 + 0.5), g_ffSpring, g_ffSpringGainMax, g_ffPeriodic);
}

static void loadConfig(void)
{
    char ini[MAX_PATH], buf[64];
    snprintf(ini, sizeof ini, "%s\\pedalmerge.ini", g_dir);

    g_log = GetPrivateProfileIntA("options", "log", 1, ini);
    GetPrivateProfileStringA("devices", "wheel", "045B:59D7", buf, sizeof buf, ini);
    g_wheelId = parseVidPid(buf);
    for (int k = 0; k < NSRC; k++) {
        Src *s = &g_src[k];
        char sec[8];
        snprintf(sec, sizeof sec, "src%d", k + 1);
        GetPrivateProfileStringA(sec, "device", "?", buf, sizeof buf, ini);
        if (!strcmp(buf, "?") && k < 2) {
            /* ini antigo: [devices] pedals/rim */
            s->legacy = k + 1;
            snprintf(s->name, sizeof s->name, "%s", k ? "aro" : "pedais");
            GetPrivateProfileStringA("devices", k ? "rim" : "pedals", "", buf, sizeof buf, ini);
            s->hide = GetPrivateProfileIntA("devices", k ? "hide_rim" : "hide_pedals", 1, ini);
        } else {
            snprintf(s->name, sizeof s->name, "fonte %d", k + 1);
            s->hide = GetPrivateProfileIntA(sec, "hide", 1, ini);
        }
        s->id = parseVidPid(buf);
        for (int a = 0; a < NAXES; a++) snprintf(s->jit[a].name, sizeof s->jit[a].name, "%s eixo %s", s->name, AXIS_NAMES[a]);
    }

    loadTuning(ini);
}

/* ---------------------------------------------------------- FFB effects */

enum { FX_OTHER, FX_CONSTANT, FX_RAMP, FX_PERIODIC, FX_SPRING, FX_DAMPER, FX_INERTIA, FX_FRICTION };
static const char *FX_NAMES[] = { "outro", "constante", "rampa", "periodico", "mola", "amortecedor", "inercia", "atrito" };

static int effectKind(REFGUID g)
{
    if (IsEqualGUID(g, &GUID_ConstantForce)) return FX_CONSTANT;
    if (IsEqualGUID(g, &GUID_RampForce)) return FX_RAMP;
    if (IsEqualGUID(g, &GUID_Square) || IsEqualGUID(g, &GUID_Sine) || IsEqualGUID(g, &GUID_Triangle) ||
        IsEqualGUID(g, &GUID_SawtoothUp) || IsEqualGUID(g, &GUID_SawtoothDown)) return FX_PERIODIC;
    if (IsEqualGUID(g, &GUID_Spring)) return FX_SPRING;
    if (IsEqualGUID(g, &GUID_Damper)) return FX_DAMPER;
    if (IsEqualGUID(g, &GUID_Inertia)) return FX_INERTIA;
    if (IsEqualGUID(g, &GUID_Friction)) return FX_FRICTION;
    return FX_OTHER;
}

static LONG pct(LONG v, int p) { return (LONG)(((LONGLONG)v * p) / 100); }

static LONG clampMag(LONG v) { return v > 10000 ? 10000 : v < -10000 ? -10000 : v; }

/* Copia os parametros do efeito aplicando as escalas e limites do ini. */
static LPCDIEFFECT adjustEffect(int kind, LPCDIEFFECT in, DWORD flags, DIEFFECT *out, BYTE *buf, DWORD bufSize)
{
    if (!in || in->dwSize < sizeof(DIEFFECT_DX5)) return in;
    memcpy(out, in, in->dwSize < sizeof *out ? in->dwSize : sizeof *out);

    if ((flags & DIEP_GAIN) && kind == FX_SPRING && (LONG)out->dwGain > g_ffSpringGainMax)
        out->dwGain = g_ffSpringGainMax;

    if (!(flags & DIEP_TYPESPECIFICPARAMS) || !in->lpvTypeSpecificParams || in->cbTypeSpecificParams > bufSize)
        return out;
    memcpy(buf, in->lpvTypeSpecificParams, in->cbTypeSpecificParams);
    out->lpvTypeSpecificParams = buf;

    switch (kind) {
    case FX_CONSTANT:
        if (in->cbTypeSpecificParams >= sizeof(DICONSTANTFORCE))
            ((DICONSTANTFORCE *)buf)->lMagnitude = clampMag(pct(clampMag(((DICONSTANTFORCE *)buf)->lMagnitude), g_ffConstant));
        break;
    case FX_RAMP:
        if (in->cbTypeSpecificParams >= sizeof(DIRAMPFORCE)) {
            DIRAMPFORCE *r = (DIRAMPFORCE *)buf;
            r->lStart = clampMag(pct(clampMag(r->lStart), g_ffRamp));
            r->lEnd = clampMag(pct(clampMag(r->lEnd), g_ffRamp));
        }
        break;
    case FX_PERIODIC:
        if (in->cbTypeSpecificParams >= sizeof(DIPERIODIC)) {
            DIPERIODIC *p = (DIPERIODIC *)buf;
            /* O jogo manda magnitude negativa num campo sem sinal (ex.: -1574 vira
             * 4294965722) e o driver entende como forca maxima. Vira positiva com
             * a fase invertida e fica limitada a 10000. */
            LONG m = (LONG)p->dwMagnitude;
            if (m < 0) {
                m = -m;
                p->dwPhase = (p->dwPhase + 18000) % 36000;
            }
            p->dwMagnitude = (DWORD)clampMag(pct(clampMag(m), g_ffPeriodic));
            p->lOffset = clampMag(pct(clampMag(p->lOffset), g_ffPeriodic));
        }
        break;
    case FX_SPRING: case FX_DAMPER: case FX_INERTIA: case FX_FRICTION: {
        int s = kind == FX_SPRING ? g_ffSpring : kind == FX_DAMPER ? g_ffDamper : kind == FX_INERTIA ? g_ffInertia : g_ffFriction;
        DWORD n = in->cbTypeSpecificParams / sizeof(DICONDITION);
        for (DWORD i = 0; i < n; i++) {
            DICONDITION *c = (DICONDITION *)buf + i;
            c->lPositiveCoefficient = pct(c->lPositiveCoefficient, s);
            c->lNegativeCoefficient = pct(c->lNegativeCoefficient, s);
            c->dwPositiveSaturation = (DWORD)pct((LONG)c->dwPositiveSaturation, s);
            c->dwNegativeSaturation = (DWORD)pct((LONG)c->dwNegativeSaturation, s);
            if (kind == FX_SPRING) {
                if (g_ffSpringOffsetZero) c->lOffset = 0;
                /* centro da mola no centro fisico do volante (offset vai de -10000 a 10000) */
                c->lOffset = clampMag(c->lOffset + (LONG)(((LONGLONG)(g_steerCenter - 32768) * 10000) / 32768));
            }
        }
        break;
    }
    }
    return out;
}

static void logEffect(const char *what, int kind, LPCDIEFFECT e, DWORD flags)
{
    if (!e) { logf("FFB %s %s (sem parametros)", what, FX_NAMES[kind]); return; }
    char extra[160] = "";
    if ((flags & DIEP_TYPESPECIFICPARAMS) && e->lpvTypeSpecificParams) {
        if (kind == FX_CONSTANT && e->cbTypeSpecificParams >= sizeof(DICONSTANTFORCE))
            snprintf(extra, sizeof extra, "magnitude=%ld", ((DICONSTANTFORCE *)e->lpvTypeSpecificParams)->lMagnitude);
        else if (kind == FX_PERIODIC && e->cbTypeSpecificParams >= sizeof(DIPERIODIC)) {
            DIPERIODIC *p = (DIPERIODIC *)e->lpvTypeSpecificParams;
            snprintf(extra, sizeof extra, "magnitude=%ld offset=%ld periodo=%lu", (LONG)p->dwMagnitude, p->lOffset, p->dwPeriod);
        } else if (kind >= FX_SPRING && e->cbTypeSpecificParams >= sizeof(DICONDITION)) {
            DICONDITION *c = (DICONDITION *)e->lpvTypeSpecificParams;
            snprintf(extra, sizeof extra, "offset=%ld coef+=%ld coef-=%ld sat+=%lu sat-=%lu deadband=%ld",
                     c->lOffset, c->lPositiveCoefficient, c->lNegativeCoefficient,
                     c->dwPositiveSaturation, c->dwNegativeSaturation, c->lDeadBand);
        }
    }
    logf("FFB %s %s flags=0x%lX ganho=%lu duracao=%lu eixos=%lu %s",
         what, FX_NAMES[kind], flags, e->dwGain, e->dwDuration, e->cAxes, extra);
}

typedef struct WEff {
    const IDirectInputEffectVtbl *lpVtbl;
    IDirectInputEffect *real;
    LONG ref;
    int kind;
    int logCount;
    DWORD duration;      /* microssegundos, INFINITE = sem fim */
    volatile LONG startSeq;
} WEff;

#define ER(This) (((WEff *)(This))->real)

static HRESULT STDMETHODCALLTYPE E_QueryInterface(IDirectInputEffect *This, REFIID riid, LPVOID *ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectInputEffect)) {
        *ppv = This;
        InterlockedIncrement(&((WEff *)This)->ref);
        return S_OK;
    }
    return ER(This)->lpVtbl->QueryInterface(ER(This), riid, ppv);
}
static ULONG STDMETHODCALLTYPE E_AddRef(IDirectInputEffect *This) { return InterlockedIncrement(&((WEff *)This)->ref); }
static ULONG STDMETHODCALLTYPE E_Release(IDirectInputEffect *This)
{
    WEff *w = (WEff *)This;
    LONG n = InterlockedDecrement(&w->ref);
    if (n == 0) {
        w->real->lpVtbl->Release(w->real);
        HeapFree(GetProcessHeap(), 0, w);
    }
    return n;
}
static HRESULT STDMETHODCALLTYPE E_Initialize(IDirectInputEffect *This, HINSTANCE a, DWORD b, REFGUID c) { return ER(This)->lpVtbl->Initialize(ER(This), a, b, c); }
static HRESULT STDMETHODCALLTYPE E_GetEffectGuid(IDirectInputEffect *This, LPGUID a) { return ER(This)->lpVtbl->GetEffectGuid(ER(This), a); }
static HRESULT STDMETHODCALLTYPE E_GetParameters(IDirectInputEffect *This, LPDIEFFECT a, DWORD b) { return ER(This)->lpVtbl->GetParameters(ER(This), a, b); }
static HRESULT STDMETHODCALLTYPE E_SetParameters(IDirectInputEffect *This, LPCDIEFFECT e, DWORD flags)
{
    WEff *w = (WEff *)This;
    if (w->logCount < 30) { w->logCount++; logEffect("SetParameters", w->kind, e, flags); }
    if (e && (flags & DIEP_DURATION)) w->duration = e->dwDuration;
    DIEFFECT tmp;
    BYTE buf[256];
    return w->real->lpVtbl->SetParameters(w->real, adjustEffect(w->kind, e, flags, &tmp, buf, sizeof buf), flags);
}
/* O FFBeast nao respeita a duracao dos efeitos: uma vibracao de batida de 0,1 s
 * continua tocando ate a base reiniciar. O proxy para o efeito no tempo certo. */
typedef struct { WEff *w; LONG seq; HANDLE timer; } StopCtx;
static LONG g_autoStopLogs;

static VOID CALLBACK autoStopCb(PVOID p, BOOLEAN fired)
{
    (void)fired;
    StopCtx *c = (StopCtx *)p;
    if (c->w->startSeq == c->seq) {
        /* zera a intensidade antes de parar, caso o firmware ignore o Stop;
         * o jogo manda os parametros de novo antes de cada Start */
        if (c->w->kind == FX_PERIODIC) {
            DIPERIODIC zero = { 0, 0, 0, 100000 };
            DIEFFECT e;
            memset(&e, 0, sizeof e);
            e.dwSize = sizeof e;
            e.cbTypeSpecificParams = sizeof zero;
            e.lpvTypeSpecificParams = &zero;
            c->w->real->lpVtbl->SetParameters(c->w->real, &e, DIEP_TYPESPECIFICPARAMS);
        }
        HRESULT hr = c->w->real->lpVtbl->Stop(c->w->real);
        if (InterlockedIncrement(&g_autoStopLogs) <= 50)
            logf("FFB auto-stop %s (hr=0x%08lX)", FX_NAMES[c->w->kind], hr);
    }
    DeleteTimerQueueTimer(NULL, c->timer, NULL);
    E_Release((IDirectInputEffect *)c->w);
    HeapFree(GetProcessHeap(), 0, c);
}

static HRESULT STDMETHODCALLTYPE E_Start(IDirectInputEffect *This, DWORD iterations, DWORD flags)
{
    WEff *w = (WEff *)This;
    if (w->logCount < 30) { w->logCount++; logf("FFB Start %s (iteracoes=%lu flags=0x%lX duracao=%lu us)", FX_NAMES[w->kind], iterations, flags, w->duration); }
    LONG seq = InterlockedIncrement(&w->startSeq);
    HRESULT hr = w->real->lpVtbl->Start(w->real, iterations, flags);
    if (SUCCEEDED(hr) && g_ffEnforceDuration && iterations != INFINITE && w->duration != INFINITE && w->duration) {
        ULONGLONG ms = (ULONGLONG)w->duration / 1000 * iterations + 20;
        if (ms > 10000) ms = 10000;
        StopCtx *c = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *c);
        c->w = w;
        c->seq = seq;
        E_AddRef(This);
        if (!CreateTimerQueueTimer(&c->timer, NULL, autoStopCb, c, (DWORD)ms, 0, WT_EXECUTEONLYONCE)) {
            E_Release(This);
            HeapFree(GetProcessHeap(), 0, c);
        }
    }
    return hr;
}
static HRESULT STDMETHODCALLTYPE E_Stop(IDirectInputEffect *This)
{
    InterlockedIncrement(&((WEff *)This)->startSeq);
    return ER(This)->lpVtbl->Stop(ER(This));
}
static HRESULT STDMETHODCALLTYPE E_GetEffectStatus(IDirectInputEffect *This, LPDWORD a) { return ER(This)->lpVtbl->GetEffectStatus(ER(This), a); }
static HRESULT STDMETHODCALLTYPE E_Download(IDirectInputEffect *This) { return ER(This)->lpVtbl->Download(ER(This)); }
static HRESULT STDMETHODCALLTYPE E_Unload(IDirectInputEffect *This) { return ER(This)->lpVtbl->Unload(ER(This)); }
static HRESULT STDMETHODCALLTYPE E_Escape(IDirectInputEffect *This, LPDIEFFESCAPE a) { return ER(This)->lpVtbl->Escape(ER(This), a); }

static const IDirectInputEffectVtbl g_effVtbl = {
    E_QueryInterface, E_AddRef, E_Release, E_Initialize, E_GetEffectGuid, E_GetParameters,
    E_SetParameters, E_Start, E_Stop, E_GetEffectStatus, E_Download, E_Unload, E_Escape,
};

/* ------------------------------------------------------ wrapped device */

/* Estado do volante, compartilhado entre as interfaces (Device, Device2, 7, 8). */
typedef struct Shared {
    LONG ref;
    int fmtReady, rangeValid;
    LONG tgtOfs[NAXES];   /* offset do eixo do volante no formato do jogo, ou -1 */
    LONG tgtMin[NAXES], tgtMax[NAXES];
    LONG btnOfs[NBTN];    /* offset do botao N do volante no formato do jogo, ou -1 */
} Shared;

/* O vtable de IDirectInputDevice8 contem os de Device/Device2/Device7 como
 * prefixo, entao o mesmo wrapper serve para todas as versoes. */
typedef struct WDev {
    const IDirectInputDevice8WVtbl *lpVtbl;
    IDirectInputDevice8W *real;
    LONG ref;
    int ansi;
    Shared *sh;
} WDev;

#define R(This) (((WDev *)(This))->real)
#define RV(This) (R(This)->lpVtbl)

static const IDirectInputDevice8WVtbl g_devVtbl;

static WDev *newWDev(IDirectInputDevice8W *real, int ansi, Shared *sh)
{
    WDev *w = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *w);
    w->lpVtbl = &g_devVtbl;
    w->real = real;
    w->ref = 1;
    w->ansi = ansi;
    if (!sh) {
        sh = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *sh);
        for (int i = 0; i < NAXES; i++) sh->tgtOfs[i] = -1;
        for (int i = 0; i < NBTN; i++) sh->btnOfs[i] = -1;
    }
    InterlockedIncrement(&sh->ref);
    w->sh = sh;
    return w;
}

static void mapGameFormat(WDev *w, LPCDIDATAFORMAT fmt)
{
    Shared *sh = w->sh;
    for (int i = 0; i < NAXES; i++) sh->tgtOfs[i] = -1;
    for (int i = 0; i < NBTN; i++) sh->btnOfs[i] = -1;
    int sliders = 0, buttons = 0;
    for (DWORD i = 0; i < fmt->dwNumObjs; i++) {
        /* guidType/dwType ficam nos mesmos offsets nas versoes A e W */
        union { DIDEVICEOBJECTINSTANCEA a; DIDEVICEOBJECTINSTANCEW w; } oi;
        oi.w.dwSize = w->ansi ? sizeof(DIDEVICEOBJECTINSTANCEA) : sizeof(DIDEVICEOBJECTINSTANCEW);
        DWORD ofs = fmt->rgodf[i].dwOfs;
        HRESULT hr = RV(w)->GetObjectInfo(w->real, &oi.w, ofs, DIPH_BYOFFSET);
        if (FAILED(hr)) continue;
        if (oi.w.dwType & DIDFT_BUTTON) {
            DWORD n = DIDFT_GETINSTANCE(oi.w.dwType);
            if (n < NBTN && sh->btnOfs[n] < 0) { sh->btnOfs[n] = ofs; buttons++; }
            continue;
        }
        if (!(oi.w.dwType & DIDFT_AXIS)) continue;
        int a = -1;
        if (IsEqualGUID(&oi.w.guidType, &GUID_XAxis)) a = 0;
        else if (IsEqualGUID(&oi.w.guidType, &GUID_YAxis)) a = 1;
        else if (IsEqualGUID(&oi.w.guidType, &GUID_ZAxis)) a = 2;
        else if (IsEqualGUID(&oi.w.guidType, &GUID_RxAxis)) a = 3;
        else if (IsEqualGUID(&oi.w.guidType, &GUID_RyAxis)) a = 4;
        else if (IsEqualGUID(&oi.w.guidType, &GUID_RzAxis)) a = 5;
        else if (IsEqualGUID(&oi.w.guidType, &GUID_Slider)) a = sliders++ ? 7 : 6;
        if (a >= 0 && sh->tgtOfs[a] < 0) {
            sh->tgtOfs[a] = ofs;
            logf("  formato do jogo: eixo %s do volante no offset %lu", AXIS_NAMES[a], ofs);
        }
    }
    logf("  formato do jogo: %d botoes do volante visiveis", buttons);
    sh->fmtReady = 1;
    sh->rangeValid = 0;
}

static void loadRanges(WDev *w)
{
    Shared *sh = w->sh;
    for (int a = 0; a < NAXES; a++) {
        sh->tgtMin[a] = 0;
        sh->tgtMax[a] = 65535;
        if (sh->tgtOfs[a] < 0) continue;
        DIPROPRANGE r;
        r.diph.dwSize = sizeof r;
        r.diph.dwHeaderSize = sizeof r.diph;
        r.diph.dwObj = sh->tgtOfs[a];
        r.diph.dwHow = DIPH_BYOFFSET;
        if (SUCCEEDED(RV(w)->GetProperty(w->real, DIPROP_RANGE, &r.diph))) {
            sh->tgtMin[a] = r.lMin;
            sh->tgtMax[a] = r.lMax;
        }
    }
    logf("  faixa do eixo X do volante no jogo: %ld..%ld", sh->tgtMin[0], sh->tgtMax[0]);
    sh->rangeValid = 1;
}

static void inject(WDev *w, DWORD cb, LPVOID data)
{
    Shared *sh = w->sh;
    if (!sh->fmtReady) return;
    BYTE *out = (BYTE *)data;
    SrcState st;

    reloadTuningIfChanged();
    if ((g_steerCenter != 32768 || g_steerGain != 1.0 || g_steerCurve != 1.0 || g_steerDeadzone > 0) &&
        sh->tgtOfs[0] >= 0 && (DWORD)sh->tgtOfs[0] + 4 <= cb) {
        if (!sh->rangeValid) loadRanges(w);
        LONG mn = sh->tgtMin[0], mx = sh->tgtMax[0];
        double mid = (mn + (double)mx) / 2, half = (mx - mn) / 2.0;
        double center = mn + (double)g_steerCenter * (mx - mn) / 65535;
        LONG *x = (LONG *)(out + sh->tgtOfs[0]);
        double deg = (*x - center) * g_steerRotation / (mx - mn);
        /* n: -1..1 depois do ganho; a curva e a zona morta agem sobre |n| */
        double n = (*x - center) * g_steerGain / half;
        double a = fabs(n) > 1 ? 1 : fabs(n);
        /* abaixo do joelho a curva vira reta: sem salto nem ruido amplificado no centro */
        const double knee = 0.08;
        double c = a >= knee ? pow(a, g_steerCurve) : a * pow(knee, g_steerCurve - 1);
        if (g_steerDeadzone > 0) a = a < 0.002 ? 0 : g_steerDeadzone + (1 - g_steerDeadzone) * c;
        else a = c;
        double v = mid + (n < 0 ? -a : a) * half;
        *x = v < mn ? mn : v > mx ? mx : (LONG)v;
        if (g_logSteering) {
            static DWORD lastLog;
            static double lastDeg;
            static int lines;
            DWORD now = GetTickCount();
            jitter(&g_jitSteer, deg, 0.1, 0.2);
            if (lines < 3000 && now - lastLog >= 250 && fabs(deg - lastDeg) >= 3) {
                lastLog = now;
                lastDeg = deg;
                lines++;
                logf("direcao: fisico %+6.1f graus -> jogo %+6ld (%+4.0f%%)", deg, *x, (*x - mid) * 100 / half);
            }
        }
    }

    /* valor de cada eixo do volante em 0..65535; meio eixo parte do centro */
    LONG acc[NAXES];
    int used[NAXES] = { 0 };
    for (int t = 0; t < NAXES; t++) acc[t] = 32768;
    for (int k = 0; k < NSRC; k++) {
        Src *s = &g_src[k];
        if (!s->id || !readSrc(s, &st)) continue;
        for (int p = 0; p < NAXES; p++) {
            int t = s->mapAxis[p];
            if (t < 0) continue;
            LONG v = st.ax[p];
            if (g_logSteering) jitter(&s->jit[p], v, 150, 400);
            if (s->invert[p]) v = 65535 - v;
            if (s->mapDir[p] == 0) acc[t] = v;
            else acc[t] += s->mapDir[p] * (v / 2);
            used[t] = 1;
        }
        if (s->buttons) {
            for (int b = 0; b < NBTN; b++) {
                int t = b + s->btnOffset;
                if (!(st.btn[b] & 0x80) || t < 0 || t >= NBTN || sh->btnOfs[t] < 0 || (DWORD)sh->btnOfs[t] >= cb) continue;
                out[sh->btnOfs[t]] |= 0x80;
            }
        }
        if (!s->loggedFirst) {
            s->loggedFirst = 1;
            logf("Primeira leitura de %s: X=%ld Y=%ld Z=%ld RX=%ld RY=%ld RZ=%ld S0=%ld S1=%ld", s->name,
                 st.ax[0], st.ax[1], st.ax[2], st.ax[3], st.ax[4], st.ax[5], st.ax[6], st.ax[7]);
        }
    }
    for (int t = 0; t < NAXES; t++) {
        if (!used[t] || sh->tgtOfs[t] < 0 || (DWORD)sh->tgtOfs[t] + 4 > cb) continue;
        if (!sh->rangeValid) loadRanges(w);
        LONG v = acc[t] < 0 ? 0 : acc[t] > 65535 ? 65535 : acc[t];
        *(LONG *)(out + sh->tgtOfs[t]) = sh->tgtMin[t] + (LONG)(((LONGLONG)v * (sh->tgtMax[t] - sh->tgtMin[t])) / 65535);
    }
}

static const IID *const DEV_IIDS[] = {
    &IID_IDirectInputDeviceA, &IID_IDirectInputDeviceW, &IID_IDirectInputDevice2A, &IID_IDirectInputDevice2W,
    &IID_IDirectInputDevice7A, &IID_IDirectInputDevice7W, &IID_IDirectInputDevice8A, &IID_IDirectInputDevice8W,
};

static HRESULT STDMETHODCALLTYPE D_QueryInterface(IDirectInputDevice8W *This, REFIID riid, LPVOID *ppv)
{
    WDev *w = (WDev *)This;
    if (IsEqualIID(riid, &IID_IUnknown)) {
        *ppv = This;
        InterlockedIncrement(&w->ref);
        return S_OK;
    }
    HRESULT hr = RV(This)->QueryInterface(R(This), riid, ppv);
    for (int i = 0; SUCCEEDED(hr) && i < 8; i++) {
        if (IsEqualIID(riid, DEV_IIDS[i])) {
            /* indices pares sao as versoes A */
            *ppv = newWDev((IDirectInputDevice8W *)*ppv, (i & 1) == 0, w->sh);
            logf("Jogo pediu outra interface do volante (versao %d) - envolvida", i);
            break;
        }
    }
    return hr;
}
static ULONG STDMETHODCALLTYPE D_AddRef(IDirectInputDevice8W *This)
{
    return InterlockedIncrement(&((WDev *)This)->ref);
}
static ULONG STDMETHODCALLTYPE D_Release(IDirectInputDevice8W *This)
{
    WDev *w = (WDev *)This;
    LONG n = InterlockedDecrement(&w->ref);
    if (n == 0) {
        w->real->lpVtbl->Release(w->real);
        if (InterlockedDecrement(&w->sh->ref) == 0) HeapFree(GetProcessHeap(), 0, w->sh);
        HeapFree(GetProcessHeap(), 0, w);
    }
    return n;
}
static HRESULT STDMETHODCALLTYPE D_GetCapabilities(IDirectInputDevice8W *This, LPDIDEVCAPS a) { return RV(This)->GetCapabilities(R(This), a); }
static HRESULT STDMETHODCALLTYPE D_EnumObjects(IDirectInputDevice8W *This, LPDIENUMDEVICEOBJECTSCALLBACKW a, LPVOID b, DWORD c) { return RV(This)->EnumObjects(R(This), a, b, c); }
static HRESULT STDMETHODCALLTYPE D_GetProperty(IDirectInputDevice8W *This, REFGUID a, LPDIPROPHEADER b) { return RV(This)->GetProperty(R(This), a, b); }
static HRESULT STDMETHODCALLTYPE D_SetProperty(IDirectInputDevice8W *This, REFGUID prop, LPCDIPROPHEADER ph)
{
    WDev *w = (WDev *)This;
    w->sh->rangeValid = 0;
    if (prop == DIPROP_AUTOCENTER) {
        DWORD v = ((LPCDIPROPDWORD)ph)->dwData;
        logf("Jogo definiu autocentro=%lu%s", v, g_ffAutocenterOff ? " -> forcado OFF" : "");
        if (g_ffAutocenterOff) {
            DIPROPDWORD off = *(LPCDIPROPDWORD)ph;
            off.dwData = DIPROPAUTOCENTER_OFF;
            return RV(This)->SetProperty(R(This), prop, &off.diph);
        }
    } else if (prop == DIPROP_FFGAIN) {
        DIPROPDWORD g = *(LPCDIPROPDWORD)ph;
        logf("Jogo definiu ganho FFB=%lu (x%d%%)", g.dwData, g_ffGain);
        g.dwData = (DWORD)pct((LONG)g.dwData, g_ffGain);
        if (g.dwData > 10000) g.dwData = 10000;
        return RV(This)->SetProperty(R(This), prop, &g.diph);
    } else if (prop == DIPROP_RANGE) {
        LPCDIPROPRANGE r = (LPCDIPROPRANGE)ph;
        logf("Jogo definiu faixa %ld..%ld (obj=%lu how=%lu)", r->lMin, r->lMax, ph->dwObj, ph->dwHow);
    } else if (prop == DIPROP_DEADZONE || prop == DIPROP_SATURATION) {
        logf("Jogo definiu %s=%lu (obj=%lu how=%lu)", prop == DIPROP_DEADZONE ? "zona morta" : "saturacao",
             ((LPCDIPROPDWORD)ph)->dwData, ph->dwObj, ph->dwHow);
    }
    return RV(This)->SetProperty(R(This), prop, ph);
}
static HRESULT STDMETHODCALLTYPE D_Acquire(IDirectInputDevice8W *This) { return RV(This)->Acquire(R(This)); }
static HRESULT STDMETHODCALLTYPE D_Unacquire(IDirectInputDevice8W *This) { return RV(This)->Unacquire(R(This)); }
static HRESULT STDMETHODCALLTYPE D_GetDeviceState(IDirectInputDevice8W *This, DWORD cb, LPVOID data)
{
    HRESULT hr = RV(This)->GetDeviceState(R(This), cb, data);
    if (SUCCEEDED(hr)) inject((WDev *)This, cb, data);
    return hr;
}
static HRESULT STDMETHODCALLTYPE D_GetDeviceData(IDirectInputDevice8W *This, DWORD a, LPDIDEVICEOBJECTDATA b, LPDWORD c, DWORD d) { return RV(This)->GetDeviceData(R(This), a, b, c, d); }
static HRESULT STDMETHODCALLTYPE D_SetDataFormat(IDirectInputDevice8W *This, LPCDIDATAFORMAT fmt)
{
    HRESULT hr = RV(This)->SetDataFormat(R(This), fmt);
    logf("Jogo definiu formato do volante: %lu bytes, %lu objetos, hr=0x%08lX", fmt->dwDataSize, fmt->dwNumObjs, hr);
    if (SUCCEEDED(hr)) mapGameFormat((WDev *)This, fmt);
    return hr;
}
static HRESULT STDMETHODCALLTYPE D_SetEventNotification(IDirectInputDevice8W *This, HANDLE a) { return RV(This)->SetEventNotification(R(This), a); }
static HRESULT STDMETHODCALLTYPE D_SetCooperativeLevel(IDirectInputDevice8W *This, HWND hwnd, DWORD flags)
{
    logf("Jogo definiu cooperative level do volante: hwnd=%p flags=0x%lX", hwnd, flags);
    openSources(hwnd);
    return RV(This)->SetCooperativeLevel(R(This), hwnd, flags);
}
static HRESULT STDMETHODCALLTYPE D_GetObjectInfo(IDirectInputDevice8W *This, LPDIDEVICEOBJECTINSTANCEW a, DWORD b, DWORD c) { return RV(This)->GetObjectInfo(R(This), a, b, c); }
static HRESULT STDMETHODCALLTYPE D_GetDeviceInfo(IDirectInputDevice8W *This, LPDIDEVICEINSTANCEW a) { return RV(This)->GetDeviceInfo(R(This), a); }
static HRESULT STDMETHODCALLTYPE D_RunControlPanel(IDirectInputDevice8W *This, HWND a, DWORD b) { return RV(This)->RunControlPanel(R(This), a, b); }
static HRESULT STDMETHODCALLTYPE D_Initialize(IDirectInputDevice8W *This, HINSTANCE a, DWORD b, REFGUID c) { return RV(This)->Initialize(R(This), a, b, c); }
static HRESULT STDMETHODCALLTYPE D_CreateEffect(IDirectInputDevice8W *This, REFGUID g, LPCDIEFFECT e, LPDIRECTINPUTEFFECT *out, LPUNKNOWN outer)
{
    int kind = effectKind(g);
    logEffect("CreateEffect", kind, e, e ? DIEP_ALLPARAMS : 0);
    DIEFFECT tmp;
    BYTE buf[256];
    IDirectInputEffect *eff;
    HRESULT hr = RV(This)->CreateEffect(R(This), g, adjustEffect(kind, e, DIEP_ALLPARAMS, &tmp, buf, sizeof buf), &eff, outer);
    if (FAILED(hr)) { logf("FFB CreateEffect falhou 0x%08lX", hr); return hr; }
    WEff *w = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *w);
    w->lpVtbl = &g_effVtbl;
    w->real = eff;
    w->ref = 1;
    w->kind = kind;
    w->duration = e ? e->dwDuration : INFINITE;
    *out = (LPDIRECTINPUTEFFECT)w;
    return hr;
}
static HRESULT STDMETHODCALLTYPE D_EnumEffects(IDirectInputDevice8W *This, LPDIENUMEFFECTSCALLBACKW a, LPVOID b, DWORD c) { return RV(This)->EnumEffects(R(This), a, b, c); }
static HRESULT STDMETHODCALLTYPE D_GetEffectInfo(IDirectInputDevice8W *This, LPDIEFFECTINFOW a, REFGUID b) { return RV(This)->GetEffectInfo(R(This), a, b); }
static HRESULT STDMETHODCALLTYPE D_GetForceFeedbackState(IDirectInputDevice8W *This, LPDWORD a) { return RV(This)->GetForceFeedbackState(R(This), a); }
static HRESULT STDMETHODCALLTYPE D_SendForceFeedbackCommand(IDirectInputDevice8W *This, DWORD a)
{
    logf("FFB comando 0x%lX", a);
    return RV(This)->SendForceFeedbackCommand(R(This), a);
}
static HRESULT STDMETHODCALLTYPE D_EnumCreatedEffectObjects(IDirectInputDevice8W *This, LPDIENUMCREATEDEFFECTOBJECTSCALLBACK a, LPVOID b, DWORD c) { return RV(This)->EnumCreatedEffectObjects(R(This), a, b, c); }
static HRESULT STDMETHODCALLTYPE D_Escape(IDirectInputDevice8W *This, LPDIEFFESCAPE a) { return RV(This)->Escape(R(This), a); }
static HRESULT STDMETHODCALLTYPE D_Poll(IDirectInputDevice8W *This) { return RV(This)->Poll(R(This)); }
static HRESULT STDMETHODCALLTYPE D_SendDeviceData(IDirectInputDevice8W *This, DWORD a, LPCDIDEVICEOBJECTDATA b, LPDWORD c, DWORD d) { return RV(This)->SendDeviceData(R(This), a, b, c, d); }
static HRESULT STDMETHODCALLTYPE D_EnumEffectsInFile(IDirectInputDevice8W *This, LPCWSTR a, LPDIENUMEFFECTSINFILECALLBACK b, LPVOID c, DWORD d) { return RV(This)->EnumEffectsInFile(R(This), a, b, c, d); }
static HRESULT STDMETHODCALLTYPE D_WriteEffectToFile(IDirectInputDevice8W *This, LPCWSTR a, DWORD b, LPDIFILEEFFECT c, DWORD d) { return RV(This)->WriteEffectToFile(R(This), a, b, c, d); }
static HRESULT STDMETHODCALLTYPE D_BuildActionMap(IDirectInputDevice8W *This, LPDIACTIONFORMATW a, LPCWSTR b, DWORD c) { return RV(This)->BuildActionMap(R(This), a, b, c); }
static HRESULT STDMETHODCALLTYPE D_SetActionMap(IDirectInputDevice8W *This, LPDIACTIONFORMATW a, LPCWSTR b, DWORD c) { return RV(This)->SetActionMap(R(This), a, b, c); }
static HRESULT STDMETHODCALLTYPE D_GetImageInfo(IDirectInputDevice8W *This, LPDIDEVICEIMAGEINFOHEADERW a) { return RV(This)->GetImageInfo(R(This), a); }

static const IDirectInputDevice8WVtbl g_devVtbl = {
    D_QueryInterface, D_AddRef, D_Release,
    D_GetCapabilities, D_EnumObjects, D_GetProperty, D_SetProperty,
    D_Acquire, D_Unacquire, D_GetDeviceState, D_GetDeviceData,
    D_SetDataFormat, D_SetEventNotification, D_SetCooperativeLevel,
    D_GetObjectInfo, D_GetDeviceInfo, D_RunControlPanel, D_Initialize,
    D_CreateEffect, D_EnumEffects, D_GetEffectInfo, D_GetForceFeedbackState,
    D_SendForceFeedbackCommand, D_EnumCreatedEffectObjects, D_Escape, D_Poll,
    D_SendDeviceData, D_EnumEffectsInFile, D_WriteEffectToFile,
    D_BuildActionMap, D_SetActionMap, D_GetImageInfo,
};

/* Envolve o dispositivo se for o volante. */
static void maybeWrapDevice(REFGUID g, void **dev, int ansi)
{
    IDirectInputDevice8W *d = (IDirectInputDevice8W *)*dev;
    DWORD id = productOf(g);
    if (!id) {
        DIPROPDWORD vp;
        vp.diph.dwSize = sizeof vp;
        vp.diph.dwHeaderSize = sizeof vp.diph;
        vp.diph.dwObj = 0;
        vp.diph.dwHow = DIPH_DEVICE;
        vp.dwData = 0;
        if (SUCCEEDED(d->lpVtbl->GetProperty(d, DIPROP_VIDPID, &vp.diph))) id = vp.dwData;
    }
    if (id && id == g_wheelId) {
        *dev = newWDev(d, ansi, NULL);
        logf("Jogo abriu o volante %08lX - pedais e aro serao injetados", id);
    } else if (id) {
        logf("Jogo abriu dispositivo %08lX (sem alteracao)", id);
    }
}

/* ------------------------------------------------------ enum filtering */

typedef struct { LPDIENUMDEVICESCALLBACKW cb; LPVOID ref; } EnumCtx;

static BOOL CALLBACK enumFilter(LPCDIDEVICEINSTANCEW inst, LPVOID p)
{
    EnumCtx *c = (EnumCtx *)p;
    /* guidInstance/guidProduct/dwDevType ficam nos mesmos offsets em A e W e em todas as versoes */
    DWORD id = inst->guidProduct.Data1;
    rememberDevice(&inst->guidInstance, id);
    for (int i = 0; i < NSRC; i++) {
        if (g_src[i].hide && g_src[i].id && id == g_src[i].id) {
            logf("  enumeracao: escondendo %s %08lX", g_src[i].name, id);
            return DIENUM_CONTINUE;
        }
    }
    logf("  enumeracao: mostrando %08lX (tipo 0x%08lX)", id, inst->dwDevType);
    return c->cb(inst, c->ref);
}

/* ------------------------------------------------- wrapped IDirectInput8 */

typedef struct WDI {
    const IDirectInput8WVtbl *lpVtbl;
    IDirectInput8W *real;
    LONG ref;
    int ansi;
} WDI;

static HRESULT STDMETHODCALLTYPE I_QueryInterface(IDirectInput8W *This, REFIID riid, LPVOID *ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDirectInput8W) || IsEqualIID(riid, &IID_IDirectInput8A)) {
        *ppv = This;
        InterlockedIncrement(&((WDI *)This)->ref);
        return S_OK;
    }
    return ((WDI *)This)->real->lpVtbl->QueryInterface(((WDI *)This)->real, riid, ppv);
}
static ULONG STDMETHODCALLTYPE I_AddRef(IDirectInput8W *This) { return InterlockedIncrement(&((WDI *)This)->ref); }
static ULONG STDMETHODCALLTYPE I_Release(IDirectInput8W *This)
{
    WDI *w = (WDI *)This;
    LONG n = InterlockedDecrement(&w->ref);
    if (n == 0) {
        w->real->lpVtbl->Release(w->real);
        HeapFree(GetProcessHeap(), 0, w);
    }
    return n;
}
static HRESULT STDMETHODCALLTYPE I_CreateDevice(IDirectInput8W *This, REFGUID g, LPDIRECTINPUTDEVICE8W *out, LPUNKNOWN outer)
{
    IDirectInput8W *real = ((WDI *)This)->real;
    HRESULT hr = real->lpVtbl->CreateDevice(real, g, out, outer);
    if (SUCCEEDED(hr)) maybeWrapDevice(g, (void **)out, ((WDI *)This)->ansi);
    return hr;
}
static HRESULT STDMETHODCALLTYPE I_EnumDevices(IDirectInput8W *This, DWORD type, LPDIENUMDEVICESCALLBACKW cb, LPVOID ref, DWORD flags)
{
    IDirectInput8W *real = ((WDI *)This)->real;
    EnumCtx c = { cb, ref };
    logf("Jogo enumerou dispositivos DI8 (classe %lu, flags 0x%lX)", type, flags);
    return real->lpVtbl->EnumDevices(real, type, enumFilter, &c, flags);
}
static HRESULT STDMETHODCALLTYPE I_GetDeviceStatus(IDirectInput8W *This, REFGUID a) { IDirectInput8W *r = ((WDI *)This)->real; return r->lpVtbl->GetDeviceStatus(r, a); }
static HRESULT STDMETHODCALLTYPE I_RunControlPanel(IDirectInput8W *This, HWND a, DWORD b) { IDirectInput8W *r = ((WDI *)This)->real; return r->lpVtbl->RunControlPanel(r, a, b); }
static HRESULT STDMETHODCALLTYPE I_Initialize(IDirectInput8W *This, HINSTANCE a, DWORD b) { IDirectInput8W *r = ((WDI *)This)->real; return r->lpVtbl->Initialize(r, a, b); }
static HRESULT STDMETHODCALLTYPE I_FindDevice(IDirectInput8W *This, REFGUID a, LPCWSTR b, LPGUID c) { IDirectInput8W *r = ((WDI *)This)->real; return r->lpVtbl->FindDevice(r, a, b, c); }
static HRESULT STDMETHODCALLTYPE I_EnumDevicesBySemantics(IDirectInput8W *This, LPCWSTR a, LPDIACTIONFORMATW b, LPDIENUMDEVICESBYSEMANTICSCBW c, LPVOID d, DWORD e) { IDirectInput8W *r = ((WDI *)This)->real; return r->lpVtbl->EnumDevicesBySemantics(r, a, b, c, d, e); }
static HRESULT STDMETHODCALLTYPE I_ConfigureDevices(IDirectInput8W *This, LPDICONFIGUREDEVICESCALLBACK a, LPDICONFIGUREDEVICESPARAMSW b, DWORD c, LPVOID d) { IDirectInput8W *r = ((WDI *)This)->real; return r->lpVtbl->ConfigureDevices(r, a, b, c, d); }

static const IDirectInput8WVtbl g_diVtbl = {
    I_QueryInterface, I_AddRef, I_Release,
    I_CreateDevice, I_EnumDevices, I_GetDeviceStatus, I_RunControlPanel,
    I_Initialize, I_FindDevice, I_EnumDevicesBySemantics, I_ConfigureDevices,
};

/* ------------------------------------------- wrapped IDirectInput (1-7) */

/* Vtable de IDirectInput7 (contem IDirectInput e IDirectInput2 como prefixo). */
typedef struct DI7Vtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void *, REFIID, void **);
    ULONG (STDMETHODCALLTYPE *AddRef)(void *);
    ULONG (STDMETHODCALLTYPE *Release)(void *);
    HRESULT (STDMETHODCALLTYPE *CreateDevice)(void *, REFGUID, void **, LPUNKNOWN);
    HRESULT (STDMETHODCALLTYPE *EnumDevices)(void *, DWORD, LPDIENUMDEVICESCALLBACKW, LPVOID, DWORD);
    HRESULT (STDMETHODCALLTYPE *GetDeviceStatus)(void *, REFGUID);
    HRESULT (STDMETHODCALLTYPE *RunControlPanel)(void *, HWND, DWORD);
    HRESULT (STDMETHODCALLTYPE *Initialize)(void *, HINSTANCE, DWORD);
    HRESULT (STDMETHODCALLTYPE *FindDevice)(void *, REFGUID, LPCWSTR, LPGUID);
    HRESULT (STDMETHODCALLTYPE *CreateDeviceEx)(void *, REFGUID, REFIID, void **, LPUNKNOWN);
} DI7Vtbl;
typedef struct DI7 { const DI7Vtbl *lpVtbl; } DI7;

typedef struct W7 {
    const DI7Vtbl *lpVtbl;
    DI7 *real;
    LONG ref;
    int ansi;
} W7;

static const DI7Vtbl g_di7Vtbl;

static W7 *newW7(DI7 *real, int ansi)
{
    W7 *w = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *w);
    w->lpVtbl = &g_di7Vtbl;
    w->real = real;
    w->ref = 1;
    w->ansi = ansi;
    return w;
}

#define W7R(This) (((W7 *)(This))->real)

static const IID *const DI_IIDS[] = {
    &IID_IDirectInputA, &IID_IDirectInputW, &IID_IDirectInput2A, &IID_IDirectInput2W,
    &IID_IDirectInput7A, &IID_IDirectInput7W,
};

static int isDeviceIIDAnsi(REFIID riid)
{
    for (int i = 0; i < 8; i++)
        if (IsEqualIID(riid, DEV_IIDS[i])) return (i & 1) == 0;
    return -1;
}

static HRESULT STDMETHODCALLTYPE J_QueryInterface(void *This, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown)) {
        *ppv = This;
        InterlockedIncrement(&((W7 *)This)->ref);
        return S_OK;
    }
    HRESULT hr = W7R(This)->lpVtbl->QueryInterface(W7R(This), riid, ppv);
    for (int i = 0; SUCCEEDED(hr) && i < 6; i++) {
        if (IsEqualIID(riid, DI_IIDS[i])) {
            *ppv = newW7((DI7 *)*ppv, (i & 1) == 0);
            break;
        }
    }
    return hr;
}
static ULONG STDMETHODCALLTYPE J_AddRef(void *This) { return InterlockedIncrement(&((W7 *)This)->ref); }
static ULONG STDMETHODCALLTYPE J_Release(void *This)
{
    W7 *w = (W7 *)This;
    LONG n = InterlockedDecrement(&w->ref);
    if (n == 0) {
        w->real->lpVtbl->Release(w->real);
        HeapFree(GetProcessHeap(), 0, w);
    }
    return n;
}
static HRESULT STDMETHODCALLTYPE J_CreateDevice(void *This, REFGUID g, void **out, LPUNKNOWN outer)
{
    HRESULT hr = W7R(This)->lpVtbl->CreateDevice(W7R(This), g, out, outer);
    if (SUCCEEDED(hr)) maybeWrapDevice(g, out, ((W7 *)This)->ansi);
    return hr;
}
static HRESULT STDMETHODCALLTYPE J_EnumDevices(void *This, DWORD type, LPDIENUMDEVICESCALLBACKW cb, LPVOID ref, DWORD flags)
{
    EnumCtx c = { cb, ref };
    logf("Jogo enumerou dispositivos DI7 (tipo %lu, flags 0x%lX)", type, flags);
    return W7R(This)->lpVtbl->EnumDevices(W7R(This), type, enumFilter, &c, flags);
}
static HRESULT STDMETHODCALLTYPE J_GetDeviceStatus(void *This, REFGUID a) { return W7R(This)->lpVtbl->GetDeviceStatus(W7R(This), a); }
static HRESULT STDMETHODCALLTYPE J_RunControlPanel(void *This, HWND a, DWORD b) { return W7R(This)->lpVtbl->RunControlPanel(W7R(This), a, b); }
static HRESULT STDMETHODCALLTYPE J_Initialize(void *This, HINSTANCE a, DWORD b) { return W7R(This)->lpVtbl->Initialize(W7R(This), a, b); }
static HRESULT STDMETHODCALLTYPE J_FindDevice(void *This, REFGUID a, LPCWSTR b, LPGUID c) { return W7R(This)->lpVtbl->FindDevice(W7R(This), a, b, c); }
static HRESULT STDMETHODCALLTYPE J_CreateDeviceEx(void *This, REFGUID g, REFIID riid, void **out, LPUNKNOWN outer)
{
    HRESULT hr = W7R(This)->lpVtbl->CreateDeviceEx(W7R(This), g, riid, out, outer);
    int ansi = isDeviceIIDAnsi(riid);
    if (SUCCEEDED(hr) && ansi >= 0) maybeWrapDevice(g, out, ansi);
    return hr;
}

static const DI7Vtbl g_di7Vtbl = {
    J_QueryInterface, J_AddRef, J_Release, J_CreateDevice, J_EnumDevices,
    J_GetDeviceStatus, J_RunControlPanel, J_Initialize, J_FindDevice, J_CreateDeviceEx,
};

/* --------------------------------------------------------------- exports */

HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD ver, REFIID riid, LPVOID *ppv, LPUNKNOWN outer)
{
    if (!loadReal8()) return DIERR_GENERIC;
    IDirectInput8W *real;
    HRESULT hr = g_create8(hinst, ver, riid, (void **)&real, outer);
    logf("DirectInput8Create(ver=0x%lX) hr=0x%08lX", ver, hr);
    if (FAILED(hr)) return hr;
    int ansi = IsEqualIID(riid, &IID_IDirectInput8A);
    if (!ansi && !IsEqualIID(riid, &IID_IDirectInput8W)) {
        *ppv = real;
        return hr;
    }
    WDI *w = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *w);
    w->lpVtbl = &g_diVtbl;
    w->real = real;
    w->ref = 1;
    w->ansi = ansi;
    *ppv = w;
    return hr;
}

static HRESULT createDI7(const char *fn, HINSTANCE hinst, DWORD ver, LPVOID *ppv, LPUNKNOWN outer, int ansi)
{
    if (!loadReal7()) return DIERR_GENERIC;
    PFN_DICreate f = (PFN_DICreate)GetProcAddress(g_real7, fn);
    if (!f) return DIERR_GENERIC;
    HRESULT hr = f(hinst, ver, ppv, outer);
    logf("%s(ver=0x%lX) hr=0x%08lX", fn, ver, hr);
    if (SUCCEEDED(hr) && !outer) *ppv = newW7((DI7 *)*ppv, ansi);
    return hr;
}

HRESULT WINAPI DirectInputCreateA(HINSTANCE hinst, DWORD ver, LPVOID *ppv, LPUNKNOWN outer)
{
    return createDI7("DirectInputCreateA", hinst, ver, ppv, outer, 1);
}

HRESULT WINAPI DirectInputCreateW(HINSTANCE hinst, DWORD ver, LPVOID *ppv, LPUNKNOWN outer)
{
    return createDI7("DirectInputCreateW", hinst, ver, ppv, outer, 0);
}

HRESULT WINAPI DirectInputCreateEx(HINSTANCE hinst, DWORD ver, REFIID riid, LPVOID *ppv, LPUNKNOWN outer)
{
    if (!loadReal7()) return DIERR_GENERIC;
    PFN_DICreateEx f = (PFN_DICreateEx)GetProcAddress(g_real7, "DirectInputCreateEx");
    if (!f) return DIERR_GENERIC;
    HRESULT hr = f(hinst, ver, riid, ppv, outer);
    logf("DirectInputCreateEx(ver=0x%lX) hr=0x%08lX", ver, hr);
    for (int i = 0; SUCCEEDED(hr) && !outer && i < 6; i++) {
        if (IsEqualIID(riid, DI_IIDS[i])) {
            *ppv = newW7((DI7 *)*ppv, (i & 1) == 0);
            break;
        }
    }
    return hr;
}

static HMODULE realForThisModule(void)
{
    return lstrcmpiA(g_modName, "dinput8") ? (loadReal7() ? g_real7 : NULL) : (loadReal8() ? g_real8 : NULL);
}

HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }

HRESULT WINAPI DllGetClassObject(REFCLSID c, REFIID r, LPVOID *p)
{
    typedef HRESULT (WINAPI *F)(REFCLSID, REFIID, LPVOID *);
    HMODULE m = realForThisModule();
    F f = m ? (F)GetProcAddress(m, "DllGetClassObject") : NULL;
    return f ? f(c, r, p) : CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllRegisterServer(void) { return E_NOTIMPL; }
HRESULT WINAPI DllUnregisterServer(void) { return E_NOTIMPL; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        InitializeCriticalSection(&g_cs);
        GetModuleFileNameA(h, g_dir, MAX_PATH);
        char *s = strrchr(g_dir, '\\');
        if (s) {
            lstrcpynA(g_modName, s + 1, sizeof g_modName);
            char *dot = strrchr(g_modName, '.');
            if (dot) *dot = 0;
            *s = 0;
        }
        for (int b = 0; b < NBTN; b++) {
            DIOBJECTDATAFORMAT *o = &g_srcObjs[NAXES + b];
            o->pguid = NULL;
            o->dwOfs = FIELD_OFFSET(SrcState, btn) + b;
            o->dwType = DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL;
            o->dwFlags = 0;
        }
        loadConfig();
        logf("=== LegacyWheel carregado (%d bits). volante=%08lX", (int)sizeof(void *) * 8, g_wheelId);
        for (int k = 0; k < NSRC; k++) {
            Src *s = &g_src[k];
            if (!s->id) continue;
            logf("  %s: %08lX esconder=%d botoes=%s (N -> N%+d)", s->name, s->id, s->hide, s->buttons ? "sim" : "nao", s->btnOffset);
            for (int i = 0; i < NAXES; i++)
                if (s->mapAxis[i] >= 0)
                    logf("    eixo %s -> volante %s%s%s", AXIS_NAMES[i], AXIS_NAMES[s->mapAxis[i]],
                         s->mapDir[i] > 0 ? "+" : s->mapDir[i] < 0 ? "-" : "", s->invert[i] ? " (invertido)" : "");
        }
        logf("  FFB: ganho=%d%% mola=%d%% (ganho max %d) amortecedor=%d%% atrito=%d%% inercia=%d%% constante=%d%% periodico=%d%% autocentro_off=%d",
             g_ffGain, g_ffSpring, g_ffSpringGainMax, g_ffDamper, g_ffFriction, g_ffInertia, g_ffConstant, g_ffPeriodic, g_ffAutocenterOff);
        logf("  FFB: parar efeitos no fim da duracao=%d", g_ffEnforceDuration);
        logf("  direcao: centro=%ld ganho=%.2f curva=%.2f zona_morta=%d%%", g_steerCenter, g_steerGain, g_steerCurve, (int)(g_steerDeadzone * 100 + 0.5));
    }
    return TRUE;
}
