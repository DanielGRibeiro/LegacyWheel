/* mergetest - age como um jogo: carrega a dinput8.dll da propria pasta (o adaptador)
 * e lista os controles que o jogo enxerga. */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#include <windows.h>
#include <dinput.h>
#include <stdio.h>

static int count;
static GUID wheel;
static int haveWheel;

static BOOL CALLBACK cb(LPCDIDEVICEINSTANCEW inst, LPVOID ctx)
{
    (void)ctx;
    printf("  visivel: %08lX %ls\n", inst->guidProduct.Data1, inst->tszProductName);
    if (!haveWheel) { wheel = inst->guidInstance; haveWheel = 1; }
    count++;
    return DIENUM_CONTINUE;
}

int main(void)
{
    IDirectInput8W *di;
    HRESULT hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di, NULL);
    printf("%d bits, DirectInput8Create hr=0x%08lX\n", (int)sizeof(void *) * 8, hr);
    if (FAILED(hr)) return 1;
    di->lpVtbl->EnumDevices(di, DI8DEVCLASS_GAMECTRL, cb, NULL, DIEDFL_ATTACHEDONLY);
    printf("%d controle(s) visivel(is)\n", count);
    /* le o primeiro controle como o jogo leria: os pedais devem aparecer nos eixos dele */
    IDirectInputDevice8W *d;
    if (haveWheel && SUCCEEDED(di->lpVtbl->CreateDevice(di, &wheel, &d, NULL))) {
        DIJOYSTATE js;
        d->lpVtbl->SetDataFormat(d, &c_dfDIJoystick);
        d->lpVtbl->Acquire(d);
        for (int i = 0; i < 5; i++) {
            Sleep(50);
            d->lpVtbl->Poll(d);
            d->lpVtbl->GetDeviceState(d, sizeof js, &js);
        }
        printf("  leitura: X=%ld Y=%ld Z=%ld\n", js.lX, js.lY, js.lZ);
        d->lpVtbl->Release(d);
    }
    di->lpVtbl->Release(di);
    return 0;
}
