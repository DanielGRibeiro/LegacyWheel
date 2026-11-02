/* ffbtest - testa adjustEffect com os valores reais enviados pelo jogo (sem tocar no hardware). */
#include "../src/pedalmerge.c"

static int fails;

static void check(const char *what, long got, long want)
{
    printf("%-44s %8ld (esperado %ld) %s\n", what, got, want, got == want ? "OK" : "FALHOU");
    if (got != want) fails++;
}

/* efeito falso que conta Start/Stop/Release */
static LONG starts, stops, releases;
struct FakeVtbl { void *qi; void *addref; ULONG (STDMETHODCALLTYPE *release)(void *); void *init, *guid, *getp; HRESULT (STDMETHODCALLTYPE *setp)(void *, LPCDIEFFECT, DWORD);
                  HRESULT (STDMETHODCALLTYPE *start)(void *, DWORD, DWORD); HRESULT (STDMETHODCALLTYPE *stop)(void *); };
static ULONG STDMETHODCALLTYPE fRelease(void *t) { (void)t; InterlockedIncrement(&releases); return 0; }
static HRESULT STDMETHODCALLTYPE fStart(void *t, DWORD a, DWORD b) { (void)t; (void)a; (void)b; InterlockedIncrement(&starts); return S_OK; }
static LONG zeroed;
static HRESULT STDMETHODCALLTYPE fSetp(void *t, LPCDIEFFECT e, DWORD f) { (void)t; (void)f; if (e && e->lpvTypeSpecificParams && ((DIPERIODIC *)e->lpvTypeSpecificParams)->dwMagnitude == 0) InterlockedIncrement(&zeroed); return S_OK; }
static HRESULT STDMETHODCALLTYPE fStop(void *t) { (void)t; InterlockedIncrement(&stops); return S_OK; }

int main(void)
{
    InitializeCriticalSection(&g_cs);  /* normalmente feito no DllMain */
    g_log = 0;
    g_ffPeriodic = 30; g_ffConstant = 100; g_ffSpring = 100; g_ffSpringGainMax = 2000;
    DIEFFECT e = { sizeof(DIEFFECT) }, out;
    BYTE buf[256];

    /* batida na mureta: magnitude -1574 num campo DWORD */
    DIPERIODIC p = { (DWORD)-1574, 0, 0, 131488 };
    e.cbTypeSpecificParams = sizeof p;
    e.lpvTypeSpecificParams = &p;
    LPCDIEFFECT r = adjustEffect(FX_PERIODIC, &e, DIEP_TYPESPECIFICPARAMS, &out, buf, sizeof buf);
    DIPERIODIC *q = (DIPERIODIC *)r->lpvTypeSpecificParams;
    check("periodico -1574: magnitude", (long)q->dwMagnitude, 472);
    check("periodico -1574: fase invertida", (long)q->dwPhase, 18000);

    DIPERIODIC big = { 4000000000u, 0, 0, 100000 };
    e.lpvTypeSpecificParams = &big;
    r = adjustEffect(FX_PERIODIC, &e, DIEP_TYPESPECIFICPARAMS, &out, buf, sizeof buf);
    check("periodico gigante: limitado", (long)((DIPERIODIC *)r->lpvTypeSpecificParams)->dwMagnitude, 3000);

    DICONSTANTFORCE c = { 50000 };
    e.cbTypeSpecificParams = sizeof c;
    e.lpvTypeSpecificParams = &c;
    r = adjustEffect(FX_CONSTANT, &e, DIEP_TYPESPECIFICPARAMS, &out, buf, sizeof buf);
    check("constante 50000: limitado a 10000", ((DICONSTANTFORCE *)r->lpvTypeSpecificParams)->lMagnitude, 10000);

    e.dwGain = 5000;
    r = adjustEffect(FX_SPRING, &e, DIEP_GAIN, &out, buf, sizeof buf);
    check("mola ganho 5000: limitado", (long)r->dwGain, 2000);
    e.dwGain = 500;
    r = adjustEffect(FX_SPRING, &e, DIEP_GAIN, &out, buf, sizeof buf);
    check("mola ganho 500: mantido", (long)r->dwGain, 500);

    g_steerCenter = 15500;
    DICONDITION cond = { 0, 9000, 9000, 10000, 10000, 0 };
    e.cbTypeSpecificParams = sizeof cond;
    e.lpvTypeSpecificParams = &cond;
    r = adjustEffect(FX_SPRING, &e, DIEP_TYPESPECIFICPARAMS, &out, buf, sizeof buf);
    check("mola: centro no centro fisico (15500)", ((DICONDITION *)r->lpvTypeSpecificParams)->lOffset, -5269);

    /* auto-stop */
    static struct FakeVtbl fv;
    fv.setp = fSetp; fv.release = fRelease; fv.start = fStart; fv.stop = fStop;
    static struct { struct FakeVtbl *v; } fake = { &fv };

    g_ffEnforceDuration = 1;
    WEff *w = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *w);
    w->lpVtbl = &g_effVtbl;
    w->real = (IDirectInputEffect *)&fake;
    w->ref = 1;
    w->kind = FX_PERIODIC;
    w->duration = 100000; /* 0,1 s como o jogo */
    IDirectInputEffect *ie = (IDirectInputEffect *)w;
    ie->lpVtbl->Start(ie, 1, 0);
    Sleep(60);
    check("auto-stop: ainda tocando aos 60 ms", stops, 0);
    Sleep(150);
    check("auto-stop: parado depois de 0,1 s", stops, 1);
    check("auto-stop: intensidade zerada", zeroed, 1);
    ie->lpVtbl->Start(ie, 1, 0);
    ie->lpVtbl->Stop(ie);   /* jogo parou antes: timer nao deve parar de novo */
    Sleep(200);
    check("auto-stop: nao duplica se o jogo parou", stops, 2);
    w->duration = INFINITE;
    ie->lpVtbl->Start(ie, INFINITE, 0);
    Sleep(200);
    check("auto-stop: efeito infinito continua", stops, 2);
    ie->lpVtbl->Release(ie);
    check("auto-stop: efeito liberado uma vez", releases, 1);

    /* detector de oscilacao: 6 Hz, +-0,5 grau, amostrado a ~100 Hz por 1,2 s */
    Jit j1 = { "teste" }, j2 = { "parado" };
    for (int i = 0; i < 120; i++) {
        jitter(&j1, 0.5 * sin(i * 0.01 * 6 * 2 * 3.14159265), 0.1, 0.2);
        jitter(&j2, (i & 1) * 0.05, 0.1, 0.2);  /* ruido de 1 contagem: nao conta */
        Sleep(10);
    }
    check("oscilacao 6 Hz detectada", j1.lines, 1);
    check("ruido pequeno ignorado", j2.lines, 0);

    printf(fails ? "\n%d FALHA(S)\n" : "\nTODOS OK\n", fails);
    return fails != 0;
}
