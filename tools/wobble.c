/* wobble - amostra o eixo X do volante por N segundos e mede a oscilacao. */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#define INITGUID
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <stdlib.h>

static IDirectInput8W *di;
static IDirectInputDevice8W *dev;

static BOOL CALLBACK cb(LPCDIDEVICEINSTANCEW inst, LPVOID ctx)
{
    (void)ctx;
    if (inst->guidProduct.Data1 != 0x59D7045B) return DIENUM_CONTINUE;
    di->lpVtbl->CreateDevice(di, &inst->guidInstance, &dev, NULL);
    return DIENUM_STOP;
}

int main(int argc, char **argv)
{
    int secs = argc > 1 ? atoi(argv[1]) : 4;
    DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di, NULL);
    di->lpVtbl->EnumDevices(di, DI8DEVCLASS_GAMECTRL, cb, NULL, DIEDFL_ATTACHEDONLY);
    if (!dev) { puts("FFBeast nao encontrado"); return 1; }
    dev->lpVtbl->SetDataFormat(dev, &c_dfDIJoystick);
    dev->lpVtbl->Acquire(dev);

    /* graus: 900 graus = 65535 */
    int n = secs * 100, crossings = 0, prevSign = 0;
    double *x = malloc(sizeof(double) * n), sum = 0, mn = 1e9, mx = -1e9;
    for (int i = 0; i < n; i++) {
        DIJOYSTATE js;
        dev->lpVtbl->Poll(dev);
        dev->lpVtbl->GetDeviceState(dev, sizeof js, &js);
        x[i] = (js.lX - 32768) * 900.0 / 65535;
        sum += x[i];
        if (x[i] < mn) mn = x[i];
        if (x[i] > mx) mx = x[i];
        Sleep(10);
    }
    double avg = sum / n;
    for (int i = 0; i < n; i++) {
        double d = x[i] - avg;
        int s = d > 0.3 ? 1 : d < -0.3 ? -1 : 0;
        if (s && prevSign && s != prevSign) crossings++;
        if (s) prevSign = s;
    }
    printf("posicao media %.1f graus, min %.1f, max %.1f, amplitude %.1f graus\n", avg, mn, mx, mx - mn);
    printf("frequencia aproximada do vai e vem: %.1f Hz\n", crossings / 2.0 / secs);
    printf("amostras (graus, a cada 50 ms):\n");
    for (int i = 0; i < n && i < 200; i += 5) printf("%.1f ", x[i]);
    printf("\n");
    return 0;
}
