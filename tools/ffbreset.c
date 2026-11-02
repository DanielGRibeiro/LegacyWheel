/* ffbreset - para e apaga todos os efeitos de Force Feedback do volante (DISFFC_RESET).
 * Use com o jogo fechado, se o volante ficar vibrando sozinho. */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#define INITGUID
#include <windows.h>
#include <dinput.h>
#include <stdio.h>

static IDirectInput8W *di;
static IDirectInputDevice8W *dev;

static BOOL CALLBACK cb(LPCDIDEVICEINSTANCEW inst, LPVOID ctx)
{
    (void)ctx;
    if (inst->guidProduct.Data1 != 0x59D7045B) return DIENUM_CONTINUE;
    di->lpVtbl->CreateDevice(di, &inst->guidInstance, &dev, NULL);
    return DIENUM_STOP;
}

int main(void)
{
    /* modo exclusivo exige uma janela; cria uma invisivel */
    HWND hwnd = CreateWindowExA(0, "STATIC", "ffbreset", WS_POPUP, 0, 0, 1, 1, NULL, NULL, GetModuleHandle(NULL), NULL);
    DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di, NULL);
    di->lpVtbl->EnumDevices(di, DI8DEVCLASS_GAMECTRL, cb, NULL, DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK);
    if (!dev) { puts("FFBeast nao encontrado"); return 1; }
    dev->lpVtbl->SetDataFormat(dev, &c_dfDIJoystick);
    HRESULT hr = dev->lpVtbl->SetCooperativeLevel(dev, hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
    printf("SetCooperativeLevel hr=0x%08lX\n", hr);
    hr = dev->lpVtbl->Acquire(dev);
    printf("Acquire hr=0x%08lX\n", hr);
    hr = dev->lpVtbl->SendForceFeedbackCommand(dev, DISFFC_STOPALL);
    printf("STOPALL hr=0x%08lX\n", hr);
    hr = dev->lpVtbl->SendForceFeedbackCommand(dev, DISFFC_RESET);
    printf("RESET hr=0x%08lX\n", hr);
    dev->lpVtbl->Unacquire(dev);
    dev->lpVtbl->Release(dev);
    di->lpVtbl->Release(di);
    DestroyWindow(hwnd);
    return 0;
}
