/* axismon - registra min/max dos 8 eixos de cada controle durante N segundos. */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#define INITGUID
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <stdlib.h>

#define NAXES 8
#define MAXDEV 8
static const char *AXIS_NAMES[NAXES] = { "X", "Y", "Z", "RX", "RY", "RZ", "S0", "S1" };

static DIOBJECTDATAFORMAT objs[NAXES] = {
    { &GUID_XAxis,  0, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_YAxis,  4, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_ZAxis,  8, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_RxAxis, 12, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_RyAxis, 16, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_RzAxis, 20, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_Slider, 24, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
    { &GUID_Slider, 28, DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0 },
};
#define NBTN 32
static DIOBJECTDATAFORMAT allobjs[NAXES + NBTN];
static DIDATAFORMAT fmt = { sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT), DIDF_ABSAXIS, NAXES * 4 + NBTN, NAXES + NBTN, allobjs };
typedef struct { LONG ax[NAXES]; BYTE btn[NBTN]; } State;

static IDirectInput8W *di;
static IDirectInputDevice8W *devs[MAXDEV];
static WCHAR names[MAXDEV][260];
static DWORD ids[MAXDEV];
static int ndev;

static BOOL CALLBACK cb(LPCDIDEVICEINSTANCEW inst, LPVOID ctx)
{
    (void)ctx;
    if (ndev >= MAXDEV) return DIENUM_STOP;
    IDirectInputDevice8W *d;
    HRESULT hr = di->lpVtbl->CreateDevice(di, &inst->guidInstance, &d, NULL);
    printf("enum: %ls CreateDevice=0x%08lX\n", inst->tszProductName, hr);
    if (FAILED(hr)) return DIENUM_CONTINUE;
    hr = d->lpVtbl->SetDataFormat(d, &fmt);
    printf("      SetDataFormat=0x%08lX\n", hr);
    DIPROPRANGE r = { { sizeof r, sizeof r.diph, 0, DIPH_DEVICE }, 0, 65535 };
    d->lpVtbl->SetProperty(d, DIPROP_RANGE, &r.diph);
    hr = d->lpVtbl->Acquire(d);
    printf("      Acquire=0x%08lX\n", hr);
    lstrcpyW(names[ndev], inst->tszProductName);
    ids[ndev] = inst->guidProduct.Data1;
    devs[ndev++] = d;
    return DIENUM_CONTINUE;
}

int main(int argc, char **argv)
{
    int secs = argc > 1 ? atoi(argv[1]) : 30;
    memcpy(allobjs, objs, sizeof objs);
    for (int b = 0; b < NBTN; b++) {
        allobjs[NAXES + b].pguid = NULL;
        allobjs[NAXES + b].dwOfs = NAXES * 4 + b;
        allobjs[NAXES + b].dwType = DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL;
        allobjs[NAXES + b].dwFlags = 0;
    }
    if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di, NULL))) return 1;
    di->lpVtbl->EnumDevices(di, DI8DEVCLASS_GAMECTRL, cb, NULL, DIEDFL_ATTACHEDONLY);

    LONG mn[MAXDEV][NAXES], mx[MAXDEV][NAXES], first[MAXDEV][NAXES];
    int pressed[MAXDEV][NBTN] = { 0 };
    for (int i = 0; i < ndev; i++)
        for (int a = 0; a < NAXES; a++) { mn[i][a] = 1 << 30; mx[i][a] = -1; }

    DWORD end = GetTickCount() + secs * 1000;
    while (GetTickCount() < end) {
        for (int i = 0; i < ndev; i++) {
            State st;
            LONG *s = st.ax;
            devs[i]->lpVtbl->Poll(devs[i]);
            if (FAILED(devs[i]->lpVtbl->GetDeviceState(devs[i], sizeof st, &st))) { devs[i]->lpVtbl->Acquire(devs[i]); continue; }
            for (int b = 0; b < NBTN; b++) if (st.btn[b] & 0x80) pressed[i][b] = 1;
            for (int a = 0; a < NAXES; a++) {
                if (mx[i][a] < 0) first[i][a] = s[a];
                if (s[a] < mn[i][a]) mn[i][a] = s[a];
                if (s[a] > mx[i][a]) mx[i][a] = s[a];
            }
        }
        Sleep(10);
    }

    for (int i = 0; i < ndev; i++) {
        printf("== %ls (VID %04lX PID %04lX)\n", names[i], ids[i] & 0xFFFF, ids[i] >> 16);
        for (int a = 0; a < NAXES; a++)
            printf("   %-3s inicio=%6ld  min=%6ld  max=%6ld  %s\n", AXIS_NAMES[a], first[i][a], mn[i][a], mx[i][a],
                   mx[i][a] - mn[i][a] > 3000 ? "<-- MOVEU" : "");
        printf("   botoes apertados:");
        for (int b = 0; b < NBTN; b++) if (pressed[i][b]) printf(" %d", b + 1);
        printf("\n");
    }
    return 0;
}
