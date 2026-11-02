/* LegacyWheel - painel de ajustes (barras deslizantes) do adaptador.
 * Barras deslizantes que gravam o pedalmerge.ini; o adaptador relê o arquivo com o jogo aberto.
 * Le os controles pelo dinput8.dll do SISTEMA (nunca pelo proxy da pasta do jogo). */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <dinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------- textos */

#define STRINGS(X) \
    X(S_TITLE, "LegacyWheel \xe2\x80\x93 Ajustes do volante", "LegacyWheel \xe2\x80\x93 Wheel settings") \
    X(S_TAB_GENERAL, "Geral", "General") \
    X(S_TAB_DEVICES, "Dispositivos", "Devices") \
    X(S_TAB_STEER, "Dire\xc3\xa7\xc3\xa3o", "Steering") \
    X(S_TAB_PEDALS, "Pedais e eixos", "Pedals && axes") \
    X(S_TAB_FFB, "Force Feedback", "Force Feedback") \
    X(S_LANG, "Idioma:", "Language:") \
    X(S_GAME, "Jogo:", "Game:") \
    X(S_ADDGAME, "Adicionar jogo...", "Add game...") \
    X(S_PICK_EXE, "Escolha o .exe do jogo", "Choose the game's .exe") \
    X(S_INFO_32, "Jogo de 32 bits", "32-bit game") \
    X(S_INFO_64, "Jogo de 64 bits", "64-bit game") \
    X(S_INFO_DI8, "usa DirectInput 8 \xe2\x9c\x93", "uses DirectInput 8 \xe2\x9c\x93") \
    X(S_INFO_DI7, "usa DirectInput antigo \xe2\x9c\x93", "uses old DirectInput \xe2\x9c\x93") \
    X(S_INFO_XI, "\xe2\x9a\xa0 usa s\xc3\xb3 XInput: o adaptador n\xc3\xa3o tem efeito", "\xe2\x9a\xa0 uses XInput only: the adapter has no effect") \
    X(S_INFO_UNK, "entrada n\xc3\xa3o detectada (.exe protegido?): teste no jogo", "input not detected (protected .exe?): test in game") \
    X(S_INFO_BAD, "n\xc3\xa3o \xc3\xa9 um .exe v\xc3\xa1lido", "not a valid .exe") \
    X(S_INSTALL, "Instalar adaptador", "Install adapter") \
    X(S_REMOVE, "Remover adaptador", "Remove adapter") \
    X(S_ST_INSTALLED, "Adaptador instalado \xe2\x9c\x93", "Adapter installed \xe2\x9c\x93") \
    X(S_ST_OLD, "Adaptador desatualizado: clique em Instalar", "Adapter outdated: click Install") \
    X(S_ST_NOTINST, "Adaptador n\xc3\xa3o instalado", "Adapter not installed") \
    X(S_ST_NOGAME, "Nenhum jogo escolhido: use Adicionar jogo", "No game chosen: use Add game") \
    X(S_PROFILE, "Perfil pronto:", "Preset:") \
    X(S_APPLY_PROFILE, "Aplicar perfil", "Apply preset") \
    X(S_P_SAFE, "Padr\xc3\xa3o seguro (qualquer volante)", "Safe default (any wheel)") \
    X(S_P_DD, "Direct drive forte (8 Nm ou mais)", "Strong direct drive (8 Nm or more)") \
    X(S_P_DIY, "DIY hoverboard / FFBeast", "DIY hoverboard / FFBeast") \
    X(S_P_GEAR, "Engrenagem ou correia (Logitech, Thrustmaster)", "Gear or belt (Logitech, Thrustmaster)") \
    X(S_LOG, "Gravar log (pedalmerge.log)", "Write log (pedalmerge.log)") \
    X(S_LOGSTEER, "Log detalhado da dire\xc3\xa7\xc3\xa3o e de oscila\xc3\xa7\xc3\xb5""es", "Detailed steering and oscillation log") \
    X(S_OPENLOG, "Abrir log", "Open log") \
    X(S_OPENDIR, "Abrir pasta", "Open folder") \
    X(S_GENERAL_NOTE, "Dire\xc3\xa7\xc3\xa3o, Pedais e Force Feedback valem na hora, mesmo com o jogo aberto. " \
                      "Mudan\xc3\xa7""as em Dispositivos exigem reiniciar o jogo.", \
                      "Steering, Pedals and Force Feedback apply instantly, even with the game running. " \
                      "Changes in Devices need a game restart.") \
    X(S_WHEEL, "Volante (base):", "Wheel (base):") \
    X(S_EXTRAS, "Dispositivos extras (pedais, aro, c\xc3\xa2mbio, freio de m\xc3\xa3o, button box):", \
                "Extra devices (pedals, rim, shifter, handbrake, button box):") \
    X(S_EXTRA, "Extra", "Extra") \
    X(S_HIDE, "Esconder", "Hide") \
    X(S_BUTTONS, "Bot\xc3\xb5""es", "Buttons") \
    X(S_OFFSET, "+N", "+N") \
    X(S_NONE, "(nenhum)", "(none)") \
    X(S_DISCONNECTED, "(desconectado)", "(disconnected)") \
    X(S_REFRESH, "Atualizar lista", "Refresh list") \
    X(S_BTNS, "Bot\xc3\xb5""es apertados:", "Buttons pressed:") \
    X(S_DEV_NOTE, "O jogo enxerga s\xc3\xb3 o volante. Extras com Esconder somem do jogo e seus eixos e bot\xc3\xb5""es " \
                  "entram no volante (\"+N\" desloca os bot\xc3\xb5""es para n\xc3\xa3o baterem). Pedais ligados na base " \
                  "(Logitech, Thrustmaster) n\xc3\xa3o precisam de extra.", \
                  "The game sees only the wheel. Extras marked Hide disappear from the game and their axes and buttons " \
                  "go into the wheel (\"+N\" shifts buttons so they don't clash). Pedals plugged into the base " \
                  "(Logitech, Thrustmaster) need no extra.") \
    X(S_ROTATION, "Rota\xc3\xa7\xc3\xa3o do volante", "Wheel rotation") \
    X(S_GAIN, "Sensibilidade", "Sensitivity") \
    X(S_CURVE, "Resposta no centro", "Center response") \
    X(S_DZ, "Vencer zona morta do jogo", "Beat game dead zone") \
    X(S_CENTER, "Centro:", "Center:") \
    X(S_SETCENTER, "Definir centro agora", "Set center now") \
    X(S_RESETCENTER, "Padr\xc3\xa3o", "Default") \
    X(S_PHYS, "Volante f\xc3\xadsico", "Physical wheel") \
    X(S_INGAME, "Valor enviado ao jogo", "Value sent to game") \
    X(S_NOWHEEL, "volante n\xc3\xa3o encontrado", "wheel not found") \
    X(S_STEER_NOTE, "Ajuste a sensibilidade at\xc3\xa9 o volante do jogo acompanhar o f\xc3\xadsico. " \
                    "Rig'n'Roll: combine com a sensibilidade do menu do jogo.", \
                    "Adjust sensitivity until the game's wheel follows the physical one. " \
                    "Rig'n'Roll: combine with the game menu's sensitivity.") \
    X(S_AXSRC, "Dispositivo:", "Device:") \
    X(S_DETECT, "Detectar eixo", "Detect axis") \
    X(S_DETECTING, "Mova o pedal ou eixo at\xc3\xa9 o fim e solte... %d s", "Move the pedal or axis all the way and release... %d s") \
    X(S_DETECTED, "Detectado: %ls, eixo %hs. Escolha para onde ele vai.", "Detected: %ls, axis %hs. Choose where it goes.") \
    X(S_DETECT_NONE, "Nada se mexeu. Tente de novo.", "Nothing moved. Try again.") \
    X(S_WHEELAXES, "(volante, s\xc3\xb3 leitura)", "(wheel, read only)") \
    X(S_PED_NOTE, "Escolha o dispositivo, pise em cada pedal (ou use Detectar eixo) e escolha para onde ele vai no volante. " \
                  "Rig'n'Roll: acelerador = Y-, freio = Y+.", \
                  "Pick the device, press each pedal (or use Detect axis) and choose where it goes on the wheel. " \
                  "Rig'n'Roll: throttle = Y-, brake = Y+.") \
    X(S_INVERT, "Inverter", "Invert") \
    X(S_FFB_GAIN, "For\xc3\xa7""a geral", "Overall strength") \
    X(S_SPRING, "Mola (centraliza\xc3\xa7\xc3\xa3o)", "Spring (centering)") \
    X(S_SPRINGMAX, "Limite da mola", "Spring limit") \
    X(S_DAMPER, "Amortecedor", "Damper") \
    X(S_FRICTION, "Atrito", "Friction") \
    X(S_INERTIA, "In\xc3\xa9rcia", "Inertia") \
    X(S_CONSTANT, "For\xc3\xa7""as da estrada", "Road forces") \
    X(S_PERIODIC, "Batidas e vibra\xc3\xa7\xc3\xb5""es", "Crashes and vibration") \
    X(S_RAMP, "Rampa", "Ramp") \
    X(S_ENFORCE, "Parar vibra\xc3\xa7\xc3\xb5""es no tempo certo (recomendado)", "Stop vibrations on time (recommended)") \
    X(S_AUTOC, "Desligar o autocentro do driver", "Turn off the driver's autocenter") \
    X(S_SPROFS, "Mola sempre no centro f\xc3\xadsico", "Keep the spring on the physical center") \
    X(S_FFB_WARN, "\xe2\x9a\xa0 Direct drive tem muita for\xc3\xa7""a: suba aos poucos. " \
                  "Batidas acima de 50% podem sacudir a mesa.", \
                  "\xe2\x9a\xa0 Direct drive wheels are very strong: raise slowly. " \
                  "Crashes above 50% can shake the desk.") \
    X(H_ROTATION, "Use o mesmo \xc3\xa2ngulo configurado no software do volante (FFBeast, G HUB, Pit House...). " \
                  "Serve para calcular os graus.", \
                  "Use the same angle set in the wheel's software (FFBeast, G HUB, Pit House...). " \
                  "It is used to compute degrees.") \
    X(H_GAIN, "Quanto o volante do jogo gira em rela\xc3\xa7\xc3\xa3o ao f\xc3\xadsico. " \
              "Entre par\xc3\xaanteses: giro f\xc3\xadsico de batente a batente no jogo.", \
              "How much the game's wheel turns compared to the physical one. " \
              "In brackets: physical lock-to-lock rotation in the game.") \
    X(H_CURVE, "1.00 = linear. Menor = pequenos movimentos perto do centro ficam mais vivos, sem mudar o fim de curso.", \
               "1.00 = linear. Lower = small movements near center feel livelier, end of travel unchanged.") \
    X(H_DZ, "Faz o volante do jogo mexer logo no primeiro grau. Use s\xc3\xb3 se o jogo tiver um vazio no centro.", \
            "Makes the game's wheel move from the very first degree. Only if the game has a dead spot at center.") \
    X(H_FFB_GAIN, "Multiplica todos os efeitos.", "Scales every effect.") \
    X(H_SPRING, "Puxa o volante para o centro. Em jogos de caminh\xc3\xa3o costuma ser o efeito principal.", \
                "Pulls the wheel to center. In truck games it is usually the main effect.") \
    X(H_SPRINGMAX, "Teto da mola. Alguns jogos mandam mola m\xc3\xa1xima no carregamento; o limite evita o tranco. 10000 = sem limite.", \
                   "Spring ceiling. Some games send full spring while loading; the limit avoids the jolt. 10000 = no limit.") \
    X(H_DAMPER, "Deixa o volante pesado contra movimentos r\xc3\xa1pidos. Ajuda a disfar\xc3\xa7""ar a travadinha de motores de hoverboard.", \
                "Resists fast movements. Helps hide the cogging of hoverboard motors.") \
    X(H_FRICTION, "Resist\xc3\xaancia constante ao girar.", "Constant resistance while turning.") \
    X(H_INERTIA, "Sensa\xc3\xa7\xc3\xa3o de massa do volante.", "Feeling of wheel mass.") \
    X(H_CONSTANT, "Pux\xc3\xb5""es de curvas e da estrada.", "Pulls from corners and the road.") \
    X(H_PERIODIC, "Trancos de batidas, meio-fio e raspadas. Valores inv\xc3\xa1lidos do jogo s\xc3\xa3o corrigidos e limitados.", \
                  "Jolts from crashes, curbs and scrapes. Invalid values from the game are fixed and limited.") \
    X(H_RAMP, "For\xc3\xa7""as que crescem ou diminuem aos poucos (pouco usado).", "Forces that grow or fade gradually (rarely used).") \
    X(S_SAVED_LIVE, "Salvo \xe2\x9c\x93 \xe2\x80\x94 o jogo aplica em at\xc3\xa9 1 segundo.", "Saved \xe2\x9c\x93 \xe2\x80\x94 the game applies it within 1 second.") \
    X(S_SAVED_RESTART, "Salvo \xe2\x9c\x93 \xe2\x80\x94 reinicie o jogo para aplicar.", "Saved \xe2\x9c\x93 \xe2\x80\x94 restart the game to apply.") \
    X(S_SAVE_FAIL, "N\xc3\xa3o consegui gravar o pedalmerge.ini nessa pasta.", "Could not write pedalmerge.ini in that folder.") \
    X(S_INSTALL_OK, "Adaptador instalado. Abra o jogo.", "Adapter installed. Start the game.") \
    X(S_INSTALL_FAIL, "N\xc3\xa3o consegui copiar as DLLs. Feche o jogo e tente de novo.", "Could not copy the DLLs. Close the game and try again.") \
    X(S_INSTALL_NOSRC, "As pastas x86 e x64 com as DLLs precisam estar junto deste programa.", \
                       "The x86 and x64 folders with the DLLs must be next to this program.") \
    X(S_FOREIGN, "J\xc3\xa1 existe uma dinput.dll ou dinput8.dll de outro mod nessa pasta. " \
                 "Ela ser\xc3\xa1 renomeada para .bak. Continuar?", \
                 "A dinput.dll or dinput8.dll from another mod is already in that folder. " \
                 "It will be renamed to .bak. Continue?") \
    X(S_REMOVE_OK, "Adaptador removido.", "Adapter removed.") \
    X(S_REMOVE_FAIL, "N\xc3\xa3o consegui apagar as DLLs. Feche o jogo e tente de novo.", "Could not delete the DLLs. Close the game and try again.") \
    X(S_PROFILE_OK, "Perfil aplicado \xe2\x9c\x93 \xe2\x80\x94 o jogo aplica em at\xc3\xa9 1 segundo.", "Preset applied \xe2\x9c\x93 \xe2\x80\x94 the game applies it within 1 second.") \
    X(S_XINPUT_WARN, "Este jogo parece usar s\xc3\xb3 XInput. O adaptador funciona apenas com DirectInput. Instalar mesmo assim?", \
                     "This game seems to use XInput only. The adapter works only with DirectInput. Install anyway?")

#define X(id, pt, en) id,
enum { STRINGS(X) S_COUNT };
#undef X
#define X(id, pt, en) { pt, en },
static const char *g_str[][2] = { STRINGS(X) };
#undef X

static int g_lang;  /* 0 = pt, 1 = en */

static const WCHAR *W(const char *s)
{
    static WCHAR buf[8][1024];
    static int i;
    i = (i + 1) & 7;
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf[i], 1024);
    return buf[i];
}
#define T(id) W(g_str[id][g_lang])

/* ------------------------------------------------------------- estado */

#define NAXES 8
#define NSRC 6
static const char *AXIS_NAMES[NAXES] = { "X", "Y", "Z", "RX", "RY", "RZ", "S0", "S1" };

enum { PG_GENERAL, PG_DEVICES, PG_STEER, PG_PEDALS, PG_FFB, PG_COUNT };
/* dispositivo 0 = volante, 1..NSRC = extras */
#define NDEV (1 + NSRC)

enum { ID_TAB = 100, ID_LANG, ID_GAME, ID_ADDGAME, ID_INSTALL, ID_REMOVE, ID_PROFILE, ID_APPLY_PROFILE, ID_OPENLOG,
       ID_OPENDIR, ID_REFRESH, ID_SETCENTER, ID_RESETCENTER, ID_AXSRC, ID_DETECT,
       ID_DEV0 = 200, ID_LBL0 = 600, ID_HIDE0 = 220, ID_BTN0 = 240, ID_OFS0 = 260, ID_CHECK0 = 300, ID_MAP0 = 400, ID_INV0 = 500,
       ID_TIMER = 1 };

static HINSTANCE g_inst;
static HWND g_main, g_tab, g_page[PG_COUNT], g_help, g_status;
static HFONT g_font, g_fontBold;
static int g_dpi = 96;
static WCHAR g_exe[MAX_PATH], g_bin[MAX_PATH], g_ini[MAX_PATH], g_selfDir[MAX_PATH];
static int g_exeArch;  /* 32 ou 64; 0 = desconhecido */
static HBRUSH g_pageBrush;

#define S(v) MulDiv((v), g_dpi, 96)

typedef struct { HWND h; int sid; } TextCtl;
static TextCtl g_texts[160];
static int g_ntexts;

enum { K_PCT, K_INT, K_DEG, K_GAIN, K_CURVE };
typedef struct {
    int page;
    const char *sec, *key;
    int sid, hid;
    int min, max, step, def, kind;
    HWND tb, val;
} Slider;

enum { SL_ROT, SL_GAIN, SL_CURVE, SL_DZ, SL_FFGAIN, SL_SPRING, SL_SPRINGMAX, SL_DAMPER, SL_FRICTION, SL_INERTIA,
       SL_CONSTANT, SL_PERIODIC, SL_RAMP, SL_COUNT };
static Slider g_sl[SL_COUNT] = {
    { PG_STEER, "steering", "rotation", S_ROTATION, H_ROTATION, 180, 1440, 10, 900, K_DEG },
    { PG_STEER, "steering", "gain", S_GAIN, H_GAIN, 5, 60, 1, 20, K_GAIN },
    { PG_STEER, "steering", "curve", S_CURVE, H_CURVE, 30, 100, 1, 100, K_CURVE },
    { PG_STEER, "steering", "deadzone", S_DZ, H_DZ, 0, 30, 1, 0, K_PCT },
    { PG_FFB, "ffb", "gain", S_FFB_GAIN, H_FFB_GAIN, 0, 100, 1, 100, K_PCT },
    { PG_FFB, "ffb", "spring", S_SPRING, H_SPRING, 0, 150, 1, 100, K_PCT },
    { PG_FFB, "ffb", "spring_gain_max", S_SPRINGMAX, H_SPRINGMAX, 0, 10000, 100, 1500, K_INT },
    { PG_FFB, "ffb", "damper", S_DAMPER, H_DAMPER, 0, 150, 1, 50, K_PCT },
    { PG_FFB, "ffb", "friction", S_FRICTION, H_FRICTION, 0, 150, 1, 50, K_PCT },
    { PG_FFB, "ffb", "inertia", S_INERTIA, H_INERTIA, 0, 150, 1, 50, K_PCT },
    { PG_FFB, "ffb", "constant", S_CONSTANT, H_CONSTANT, 0, 150, 1, 100, K_PCT },
    { PG_FFB, "ffb", "periodic", S_PERIODIC, H_PERIODIC, 0, 100, 1, 25, K_PCT },
    { PG_FFB, "ffb", "ramp", S_RAMP, H_RAMP, 0, 150, 1, 50, K_PCT },
};

typedef struct {
    int page;
    const char *sec, *key;
    int sid, def, live;
    HWND h;
} Check;

enum { CK_ENFORCE, CK_AUTOC, CK_SPROFS, CK_LOG, CK_LOGSTEER, CK_COUNT };
static Check g_ck[CK_COUNT] = {
    { PG_FFB, "ffb", "enforce_duration", S_ENFORCE, 1, 1 },
    { PG_FFB, "ffb", "autocenter_off", S_AUTOC, 1, 1 },
    { PG_FFB, "ffb", "spring_offset_zero", S_SPROFS, 1, 1 },
    { PG_GENERAL, "options", "log", S_LOG, 1, 0 },
    { PG_GENERAL, "options", "log_steering", S_LOGSTEER, 0, 1 },
};

/* perfis prontos: lockDeg = giro fisico de batente a batente no jogo */
typedef struct {
    int sid;
    int ffGain, spring, springMax, damper, friction, inertia, constant, periodic, ramp;
    int lockDeg, curve, dz;
} Profile;
static const Profile g_profiles[] = {
    { S_P_SAFE, 100, 100, 1500, 50, 50, 50, 100, 25, 50, 450, 70, 0 },
    { S_P_DD, 80, 70, 1000, 40, 40, 40, 80, 15, 40, 450, 70, 0 },
    { S_P_DIY, 100, 100, 1500, 50, 50, 50, 100, 25, 50, 450, 50, 0 },
    { S_P_GEAR, 100, 100, 4000, 80, 80, 60, 100, 60, 80, 450, 80, 0 },
};
#define NPROFILES (int)(sizeof g_profiles / sizeof g_profiles[0])

/* dispositivos conectados */
typedef struct { WCHAR name[128]; GUID inst; DWORD id; } DevInfo;
static DevInfo g_list[32];
static int g_nlist;

typedef struct {
    DWORD id;                    /* VID | PID << 16 configurado */
    IDirectInputDevice8W *d;
    DIJOYSTATE st;
    int ok;
} Dev;
static Dev g_dev[NDEV];
static DWORD g_comboIds[NDEV][40];
static HWND g_devCombo[NDEV], g_hideCk[NDEV], g_btnCk[NDEV], g_ofsCombo[NDEV];
static HWND g_langCombo, g_gameCombo, g_gameInfo, g_profileCombo, g_installStatus, g_centerLbl;
static HWND g_steerBar[2], g_steerTxt[2], g_pedBar[NAXES], g_pedVal[NAXES], g_mapCombo[NAXES], g_invCk[NAXES];
static HWND g_axSrcCombo, g_btnTxt;
static int g_axSrcItems[NSRC + 1], g_naxSrcItems;  /* item do combo -> dispositivo (0 = volante) */
static int g_axDev;                                 /* dispositivo mostrado na aba de eixos */
static LONG g_center = 32768;

/* lista de jogos (caminhos dos .exe) */
static WCHAR g_games[16][MAX_PATH];
static int g_ngames;

/* assistente de deteccao */
static DWORD g_detectEnd;
static LONG g_detMin[NDEV][NAXES], g_detMax[NDEV][NAXES], g_detFirst[NDEV][NAXES];
static int g_detSeen[NDEV];

typedef HRESULT (WINAPI *PFN_DI8Create)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
static IDirectInput8W *g_di;
static DIOBJECTDATAFORMAT g_odf[8 + 4 + 32];
static DIDATAFORMAT g_df;

/* ------------------------------------------------------------- ini */

static void iniPath(void) { swprintf(g_ini, MAX_PATH, L"%ls\\pedalmerge.ini", g_bin); }

static void srcSec(int dev, char *sec) { sprintf(sec, "src%d", dev); }

static int iniInt(const char *sec, const char *key, int def)
{
    WCHAR s[64], k[64];
    MultiByteToWideChar(CP_UTF8, 0, sec, -1, s, 64);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, k, 64);
    return (int)GetPrivateProfileIntW(s, k, def, g_ini);
}

static void iniStr(const char *sec, const char *key, const char *def, char *out, int n)
{
    WCHAR s[64], k[64], d[64], o[128];
    MultiByteToWideChar(CP_UTF8, 0, sec, -1, s, 64);
    MultiByteToWideChar(CP_UTF8, 0, key, -1, k, 64);
    MultiByteToWideChar(CP_UTF8, 0, def, -1, d, 64);
    GetPrivateProfileStringW(s, k, d, o, 128, g_ini);
    WideCharToMultiByte(CP_UTF8, 0, o, -1, out, n, NULL, NULL);
}

/* Gravacoes vao para uma copia e entram de uma vez (MoveFileEx), para o adaptador
 * nunca ler um arquivo pela metade. Agrupe varias com iniBegin/iniCommit. */
static WCHAR g_iniTmp[MAX_PATH];
static int g_batch;

static void iniBegin(void)
{
    if (g_batch++) return;
    swprintf(g_iniTmp, MAX_PATH, L"%ls.tmp", g_ini);
    CopyFileW(g_ini, g_iniTmp, FALSE);
}

static BOOL iniCommit(void)
{
    if (--g_batch) return TRUE;
    return MoveFileExW(g_iniTmp, g_ini, MOVEFILE_REPLACE_EXISTING);
}

/* key NULL apaga a secao inteira; val NULL apaga a chave */
static void iniSet(const char *sec, const char *key, const char *val)
{
    WCHAR s[64], k[64], v[64];
    MultiByteToWideChar(CP_UTF8, 0, sec, -1, s, 64);
    if (key) MultiByteToWideChar(CP_UTF8, 0, key, -1, k, 64);
    if (val) MultiByteToWideChar(CP_UTF8, 0, val, -1, v, 64);
    iniBegin();
    WritePrivateProfileStringW(s, key ? k : NULL, val ? v : NULL, g_iniTmp);
    iniCommit();
}

static void iniSetInt(const char *sec, const char *key, int v)
{
    char b[16];
    snprintf(b, sizeof b, "%d", v);
    iniSet(sec, key, b);
}

static const char DEFAULT_INI[] =
    "; LegacyWheel - configuracao / settings\r\n"
    "; Edite pelo LegacyWheel.exe / Edit with LegacyWheel.exe\r\n"
    "; Eixos / axes: X Y Z RX RY RZ S0 S1\r\n"
    "\r\n[devices]\r\nwheel=\r\n"
    "\r\n[steering]\r\ncenter=32768\r\nrotation=900\r\ngain=2.0\r\ncurve=0.70\r\ndeadzone=0\r\n"
    "\r\n[ffb]\r\ngain=100\r\nspring=100\r\nspring_gain_max=1500\r\ndamper=50\r\nfriction=50\r\ninertia=50\r\n"
    "constant=100\r\nperiodic=25\r\nramp=50\r\nspring_offset_zero=1\r\nautocenter_off=1\r\nenforce_duration=1\r\n"
    "\r\n[options]\r\nlog=1\r\nlog_steering=0\r\n";

static BOOL ensureIni(void)
{
    if (GetFileAttributesW(g_ini) != INVALID_FILE_ATTRIBUTES) return TRUE;
    HANDLE f = CreateFileW(g_ini, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    DWORD n;
    WriteFile(f, DEFAULT_INI, sizeof DEFAULT_INI - 1, &n, NULL);
    CloseHandle(f);
    return TRUE;
}

/* ini antigo (pedals/rim, [map], [invert], [rim]) -> [src1] e [src2] */
static void migrateLegacy(void)
{
    char b[64], key[16];
    iniStr("src1", "device", "?", b, sizeof b);
    if (strcmp(b, "?")) return;
    char ped[32], rim[32];
    iniStr("devices", "pedals", "", ped, sizeof ped);
    iniStr("devices", "rim", "", rim, sizeof rim);
    if (!ped[0] && !rim[0]) return;
    iniBegin();
    if (ped[0]) {
        iniSet("src1", "device", ped);
        iniSetInt("src1", "hide", iniInt("devices", "hide_pedals", 1));
        iniSetInt("src1", "buttons", 0);
        for (int a = 0; a < NAXES; a++) {
            iniStr("map", AXIS_NAMES[a], "", b, sizeof b);
            if (b[0]) iniSet("src1", AXIS_NAMES[a], b);
            snprintf(key, sizeof key, "invert_%s", AXIS_NAMES[a]);
            if (iniInt("invert", AXIS_NAMES[a], 0)) iniSetInt("src1", key, 1);
        }
    } else {
        iniSet("src1", "device", "");
    }
    if (rim[0]) {
        iniSet("src2", "device", rim);
        iniSetInt("src2", "hide", iniInt("devices", "hide_rim", 1));
        iniSetInt("src2", "buttons", 1);
        iniSetInt("src2", "button_offset", iniInt("rim", "button_offset", 0));
    }
    iniSet("devices", "pedals", NULL);
    iniSet("devices", "rim", NULL);
    iniSet("devices", "hide_pedals", NULL);
    iniSet("devices", "hide_rim", NULL);
    iniSet("map", NULL, NULL);
    iniSet("invert", NULL, NULL);
    iniSet("rim", NULL, NULL);
    iniCommit();
}

/* ------------------------------------------------------------- registro */

static void regSave(const WCHAR *name, const void *data, DWORD type, DWORD size)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\LegacyWheel", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL)) return;
    RegSetValueExW(k, name, 0, type, data, size);
    RegCloseKey(k);
}

/* le da chave nova; se nao houver, da chave do nome antigo (RnR-PedalMerge) */
static BOOL regLoad(const WCHAR *name, void *data, DWORD size)
{
    static const WCHAR *keys[2] = { L"Software\\LegacyWheel", L"Software\\RnR-PedalMerge" };
    for (int i = 0; i < 2; i++) {
        HKEY k;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, keys[i], 0, KEY_READ, &k)) continue;
        DWORD sz = size;
        LONG r = RegQueryValueExW(k, name, NULL, NULL, data, &sz);
        RegCloseKey(k);
        if (r == 0) return TRUE;
    }
    return FALSE;
}

static void saveGames(void)
{
    static WCHAR multi[16 * MAX_PATH + 1];
    int n = 0;
    for (int i = 0; i < g_ngames; i++) {
        lstrcpyW(multi + n, g_games[i]);
        n += lstrlenW(g_games[i]) + 1;
    }
    multi[n++] = 0;
    regSave(L"Games", multi, REG_MULTI_SZ, (DWORD)n * sizeof(WCHAR));
    regSave(L"GameExe", g_exe, REG_SZ, (DWORD)(lstrlenW(g_exe) + 1) * sizeof(WCHAR));
}

static int addGame(const WCHAR *exe)
{
    for (int i = 0; i < g_ngames; i++)
        if (!lstrcmpiW(g_games[i], exe)) return i;
    if (g_ngames >= 16) return -1;
    lstrcpynW(g_games[g_ngames], exe, MAX_PATH);
    return g_ngames++;
}

static void loadGames(void)
{
    static WCHAR multi[16 * MAX_PATH + 1];
    memset(multi, 0, sizeof multi);
    if (regLoad(L"Games", multi, sizeof multi - sizeof(WCHAR)))
        for (WCHAR *p = multi; *p; p += lstrlenW(p) + 1)
            if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) addGame(p);
}

/* Rig'n'Roll pela Steam: biblioteca padrao e libraryfolders.vdf */
static BOOL tryLibrary(const WCHAR *lib, WCHAR *out)
{
    swprintf(out, MAX_PATH, L"%ls\\steamapps\\common\\Rig n Roll\\Bin\\rnr.exe", lib);
    for (WCHAR *c = out; *c; c++) if (*c == L'/') *c = L'\\';
    return GetFileAttributesW(out) != INVALID_FILE_ATTRIBUTES;
}

static BOOL findRigNRoll(WCHAR *out)
{
    WCHAR steam[MAX_PATH] = L"", p[MAX_PATH];
    HKEY k;
    if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &k)) {
        DWORD sz = sizeof steam;
        RegQueryValueExW(k, L"SteamPath", NULL, NULL, (BYTE *)steam, &sz);
        RegCloseKey(k);
    }
    if (!steam[0]) lstrcpyW(steam, L"C:\\Program Files (x86)\\Steam");
    if (tryLibrary(steam, out)) return TRUE;
    swprintf(p, MAX_PATH, L"%ls\\steamapps\\libraryfolders.vdf", steam);
    FILE *f = _wfopen(p, L"rb");
    if (!f) return FALSE;
    char line[1024];
    BOOL found = FALSE;
    while (!found && fgets(line, sizeof line, f)) {
        char *q = strstr(line, "\"path\"");
        if (!q || !(q = strchr(q + 6, '"'))) continue;
        char path[MAX_PATH];
        int n = 0;
        for (q++; *q && *q != '"' && n < MAX_PATH - 1; q++) {
            if (*q == '\\' && q[1] == '\\') q++;
            path[n++] = *q;
        }
        path[n] = 0;
        WCHAR wp[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, MAX_PATH);
        found = tryLibrary(wp, out);
    }
    fclose(f);
    return found;
}

/* ------------------------------------------------------------- analise do .exe */

enum { IN_UNKNOWN, IN_DI8, IN_DI7, IN_XINPUT, IN_BAD };

static DWORD rvaToOfs(BYTE *img, DWORD size, IMAGE_SECTION_HEADER *sec, int nsec, DWORD rva)
{
    (void)img;
    for (int i = 0; i < nsec; i++) {
        DWORD va = sec[i].VirtualAddress, len = sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
        if (rva >= va && rva < va + len) {
            DWORD o = rva - va + sec[i].PointerToRawData;
            return o < size ? o : 0;
        }
    }
    return 0;
}

/* arquitetura (32/64) e qual entrada o .exe importa */
static int analyzeExe(const WCHAR *path, int *arch)
{
    *arch = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return IN_BAD;
    DWORD size = GetFileSize(f, NULL), n = 0;
    if (size > 256 * 1024 * 1024) size = 256 * 1024 * 1024;
    BYTE *img = HeapAlloc(GetProcessHeap(), 0, size ? size : 1);
    BOOL rd = img && ReadFile(f, img, size, &n, NULL) && n == size;
    CloseHandle(f);
    int res = IN_BAD;
    if (!rd || size < 0x200) goto done;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)img;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || (DWORD)dos->e_lfanew + 0x200 > size) goto done;
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(img + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) goto done;
    IMAGE_DATA_DIRECTORY *dirs;
    if (nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
        *arch = 32;
        dirs = nt->OptionalHeader.DataDirectory;
    } else if (nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64) {
        *arch = 64;
        dirs = ((IMAGE_NT_HEADERS64 *)nt)->OptionalHeader.DataDirectory;
    } else {
        goto done;
    }
    IMAGE_SECTION_HEADER *sec = (IMAGE_SECTION_HEADER *)((BYTE *)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader);
    int nsec = nt->FileHeader.NumberOfSections;
    if ((BYTE *)(sec + nsec) > img + size) goto done;
    BOOL di8 = FALSE, di7 = FALSE, xi = FALSE;
    /* importacoes normais (descritor de 20 bytes) e atrasadas (32 bytes) */
    for (int d = 0; d < 2; d++) {
        IMAGE_DATA_DIRECTORY *dd = &dirs[d ? IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT : IMAGE_DIRECTORY_ENTRY_IMPORT];
        DWORD o = dd->VirtualAddress ? rvaToOfs(img, size, sec, nsec, dd->VirtualAddress) : 0;
        DWORD stride = d ? 32 : 20, nameAt = d ? 4 : 12;
        for (int i = 0; o && i < 512 && o + stride <= size; i++, o += stride) {
            DWORD nameRva = *(DWORD *)(img + o + nameAt);
            if (!nameRva) break;
            DWORD no = rvaToOfs(img, size, sec, nsec, nameRva);
            if (!no) continue;
            char name[64];
            lstrcpynA(name, (char *)img + no, sizeof name);
            if (!lstrcmpiA(name, "dinput8.dll")) di8 = TRUE;
            else if (!lstrcmpiA(name, "dinput.dll")) di7 = TRUE;
            else if (!_strnicmp(name, "xinput", 6)) xi = TRUE;
        }
    }
    res = di8 ? IN_DI8 : di7 ? IN_DI7 : xi ? IN_XINPUT : IN_UNKNOWN;
done:
    if (img) HeapFree(GetProcessHeap(), 0, img);
    return res;
}

/* nossas DLLs contem o nome do ini; as novas tambem "invert_%s" (varios dispositivos) */
static int dllVersion(const WCHAR *path)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD size = GetFileSize(f, NULL), n = 0;
    int ours = 0, multi = 0;
    if (size && size < 8 * 1024 * 1024) {
        char *buf = HeapAlloc(GetProcessHeap(), 0, size);
        if (buf && ReadFile(f, buf, size, &n, NULL))
            for (DWORD i = 0; i + 14 <= n; i++) {
                if (!memcmp(buf + i, "pedalmerge.ini", 14)) ours = 1;
                if (!memcmp(buf + i, "invert_%s", 9)) multi = 1;
            }
        if (buf) HeapFree(GetProcessHeap(), 0, buf);
    }
    CloseHandle(f);
    return ours ? (multi ? 2 : 1) : 0;
}

static const WCHAR *DLLS[2] = { L"dinput.dll", L"dinput8.dll" };

/* 0 = nao instalado, 1 = versao antiga, 2 = atual */
static int installState(void)
{
    WCHAR p[MAX_PATH];
    int v = 2;
    for (int i = 0; i < 2; i++) {
        swprintf(p, MAX_PATH, L"%ls\\%ls", g_bin, DLLS[i]);
        int d = dllVersion(p);
        if (d < v) v = d;
    }
    return v;
}

/* ------------------------------------------------------------- DirectInput */

static void buildFormat(void)
{
    static const GUID *axes[8] = { &GUID_XAxis, &GUID_YAxis, &GUID_ZAxis, &GUID_RxAxis, &GUID_RyAxis, &GUID_RzAxis,
                                   &GUID_Slider, &GUID_Slider };
    int n = 0;
    for (int i = 0; i < 8; i++) {
        g_odf[n].pguid = axes[i];
        g_odf[n].dwOfs = i < 6 ? (DWORD)(i * 4) : (DWORD)DIJOFS_SLIDER(i - 6);
        g_odf[n].dwType = DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL;
        g_odf[n++].dwFlags = DIDOI_ASPECTPOSITION;
    }
    for (int i = 0; i < 4; i++) {
        g_odf[n].pguid = &GUID_POV;
        g_odf[n].dwOfs = DIJOFS_POV(i);
        g_odf[n].dwType = DIDFT_POV | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL;
        g_odf[n++].dwFlags = 0;
    }
    for (int i = 0; i < 32; i++) {
        g_odf[n].pguid = NULL;
        g_odf[n].dwOfs = DIJOFS_BUTTON(i);
        g_odf[n].dwType = DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL;
        g_odf[n++].dwFlags = 0;
    }
    g_df.dwSize = sizeof g_df;
    g_df.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
    g_df.dwFlags = DIDF_ABSAXIS;
    g_df.dwDataSize = sizeof(DIJOYSTATE);
    g_df.dwNumObjs = n;
    g_df.rgodf = g_odf;
}

static BOOL initDI(void)
{
    WCHAR p[MAX_PATH];
    GetSystemDirectoryW(p, MAX_PATH);
    lstrcatW(p, L"\\dinput8.dll");
    HMODULE m = LoadLibraryW(p);
    PFN_DI8Create create = m ? (PFN_DI8Create)(void *)GetProcAddress(m, "DirectInput8Create") : NULL;
    if (!create) return FALSE;
    buildFormat();
    return SUCCEEDED(create(g_inst, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&g_di, NULL));
}

static BOOL CALLBACK enumCb(LPCDIDEVICEINSTANCEW inst, LPVOID ctx)
{
    (void)ctx;
    if (g_nlist >= 32) return DIENUM_STOP;
    DevInfo *d = &g_list[g_nlist++];
    lstrcpynW(d->name, inst->tszProductName, 128);
    d->inst = inst->guidInstance;
    d->id = inst->guidProduct.Data1;
    return DIENUM_CONTINUE;
}

static const WCHAR *devName(DWORD id)
{
    for (int i = 0; i < g_nlist; i++)
        if (g_list[i].id == id) return g_list[i].name;
    return T(S_DISCONNECTED);
}

static void closeDev(int r)
{
    if (g_dev[r].d) {
        g_dev[r].d->lpVtbl->Unacquire(g_dev[r].d);
        g_dev[r].d->lpVtbl->Release(g_dev[r].d);
    }
    g_dev[r].d = NULL;
    g_dev[r].ok = 0;
}

static void openDev(int r)
{
    closeDev(r);
    if (!g_di || !g_dev[r].id) return;
    for (int i = 0; i < g_nlist; i++) {
        if (g_list[i].id != g_dev[r].id) continue;
        IDirectInputDevice8W *d;
        if (FAILED(g_di->lpVtbl->CreateDevice(g_di, &g_list[i].inst, &d, NULL))) return;
        d->lpVtbl->SetDataFormat(d, &g_df);
        d->lpVtbl->SetCooperativeLevel(d, g_main, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
        DIPROPRANGE rg = { { sizeof rg, sizeof rg.diph, 0, DIPH_DEVICE }, 0, 65535 };
        d->lpVtbl->SetProperty(d, DIPROP_RANGE, &rg.diph);
        DIPROPDWORD dz = { { sizeof dz, sizeof dz.diph, 0, DIPH_DEVICE }, 0 };
        d->lpVtbl->SetProperty(d, DIPROP_DEADZONE, &dz.diph);
        d->lpVtbl->Acquire(d);
        g_dev[r].d = d;
        return;
    }
}

static void pollDev(int r)
{
    Dev *v = &g_dev[r];
    v->ok = 0;
    if (!v->d) return;
    v->d->lpVtbl->Poll(v->d);
    if (FAILED(v->d->lpVtbl->GetDeviceState(v->d, sizeof v->st, &v->st))) {
        v->d->lpVtbl->Acquire(v->d);
        return;
    }
    v->ok = 1;
}

static LONG axisVal(const DIJOYSTATE *s, int a)
{
    switch (a) {
    case 0: return s->lX;
    case 1: return s->lY;
    case 2: return s->lZ;
    case 3: return s->lRx;
    case 4: return s->lRy;
    case 5: return s->lRz;
    default: return s->rglSlider[a - 6];
    }
}

static DWORD parseVidPid(const char *s)
{
    unsigned vid = 0, pid = 0;
    if (sscanf(s, "%x:%x", &vid, &pid) != 2) return 0;
    return (vid & 0xFFFF) | ((pid & 0xFFFF) << 16);
}

/* ------------------------------------------------------------- barra ao vivo */

/* valor 0..1000; bipolar: -1000..1000 a partir do centro */
static void setBar(HWND h, int v)
{
    if (GetWindowLongPtrW(h, 0) == v) return;
    SetWindowLongPtrW(h, 0, v);
    InvalidateRect(h, NULL, FALSE);
}

static LRESULT CALLBACK barProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    if (m == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(225, 228, 232)), fg = CreateSolidBrush(RGB(0, 120, 215));
        FillRect(dc, &rc, bg);
        int v = (int)GetWindowLongPtrW(h, 0), bip = (int)GetWindowLongPtrW(h, sizeof(LONG_PTR));
        int w = rc.right - rc.left;
        RECT f = rc;
        if (bip) {
            int mid = w / 2, x = mid + v * mid / 1000;
            f.left = x < mid ? x : mid;
            f.right = x < mid ? mid : x;
            FillRect(dc, &f, fg);
            RECT c = { mid - 1, rc.top, mid + 1, rc.bottom };
            FillRect(dc, &c, (HBRUSH)GetStockObject(BLACK_BRUSH));
        } else {
            f.right = v * w / 1000;
            FillRect(dc, &f, fg);
        }
        DeleteObject(bg);
        DeleteObject(fg);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    return DefWindowProcW(h, m, wp, lp);
}

/* ------------------------------------------------------------- criacao de controles */

static HWND mk(HWND parent, const WCHAR *cls, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExW(0, cls, L"", WS_CHILD | WS_VISIBLE | style, S(x), S(y), S(w), S(h), parent,
                             (HMENU)(INT_PTR)id, g_inst, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_font, TRUE);
    return c;
}

static HWND txt(HWND parent, int sid, int x, int y, int w, int h, DWORD style)
{
    HWND c = mk(parent, L"STATIC", style, x, y, w, h, 0);
    if (g_ntexts < 160) { g_texts[g_ntexts].h = c; g_texts[g_ntexts++].sid = sid; }
    SetWindowTextW(c, T(sid));
    return c;
}

static HWND btn(HWND parent, int sid, int x, int y, int w, int h, int id, DWORD style)
{
    HWND c = mk(parent, L"BUTTON", WS_TABSTOP | style, x, y, w, h, id);
    if (g_ntexts < 160) { g_texts[g_ntexts].h = c; g_texts[g_ntexts++].sid = sid; }
    SetWindowTextW(c, T(sid));
    return c;
}

static HWND combo(HWND parent, int x, int y, int w, int id)
{
    return mk(parent, WC_COMBOBOXW, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, x, y, w, 300, id);
}

static HWND bar(HWND parent, int x, int y, int w, int h, int bipolar)
{
    HWND c = mk(parent, L"LegacyWheelBar", 0, x, y, w, h, 0);
    SetWindowLongPtrW(c, sizeof(LONG_PTR), bipolar);
    return c;
}

static int sliderY(int idx, int y)
{
    Slider *s = &g_sl[idx];
    HWND p = g_page[s->page];
    txt(p, s->sid, 12, y + 4, 210, 20, 0);
    s->tb = mk(p, TRACKBAR_CLASSW, WS_TABSTOP | TBS_HORZ | TBS_NOTICKS, 225, y, 270, 28, 0);
    SendMessageW(s->tb, TBM_SETRANGEMIN, FALSE, s->min);
    SendMessageW(s->tb, TBM_SETRANGEMAX, FALSE, s->max);
    SendMessageW(s->tb, TBM_SETLINESIZE, 0, s->step);
    SendMessageW(s->tb, TBM_SETPAGESIZE, 0, s->step * 10);
    SetWindowLongPtrW(s->tb, GWLP_USERDATA, idx + 1);
    s->val = mk(p, L"STATIC", 0, 500, y + 4, 140, 20, 0);
    return y + 32;
}

static int checkY(int idx, int x, int y, int w)
{
    Check *c = &g_ck[idx];
    c->h = btn(g_page[c->page], c->sid, x, y, w, 22, ID_CHECK0 + idx, BS_AUTOCHECKBOX);
    return y + 26;
}

static void setCheck(HWND h, int on) { SendMessageW(h, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0); }
static int getCheck(HWND h) { return SendMessageW(h, BM_GETCHECK, 0, 0) == BST_CHECKED; }

/* ------------------------------------------------------------- valores */

static int sliderPos(int idx) { return (int)SendMessageW(g_sl[idx].tb, TBM_GETPOS, 0, 0); }

static void sliderText(int idx)
{
    Slider *s = &g_sl[idx];
    int v = sliderPos(idx);
    char b[64];
    switch (s->kind) {
    case K_PCT: snprintf(b, sizeof b, "%d%%", v); break;
    case K_DEG: snprintf(b, sizeof b, "%d\xc2\xb0", v); break;
    case K_GAIN: snprintf(b, sizeof b, "%.1fx  (%d\xc2\xb0)", v / 10.0, (int)(sliderPos(SL_ROT) * 10.0 / v + 0.5)); break;
    case K_CURVE: snprintf(b, sizeof b, "%.2f", v / 100.0); break;
    default: snprintf(b, sizeof b, "%d", v); break;
    }
    SetWindowTextW(s->val, W(b));
}

static void sliderSave(int idx)
{
    Slider *s = &g_sl[idx];
    int v = sliderPos(idx);
    char b[32];
    if (s->kind == K_GAIN) snprintf(b, sizeof b, "%.1f", v / 10.0);
    else if (s->kind == K_CURVE) snprintf(b, sizeof b, "%.2f", v / 100.0);
    else snprintf(b, sizeof b, "%d", v);
    iniSet(s->sec, s->key, b);
}

static void loadSlider(int idx)
{
    Slider *s = &g_sl[idx];
    int v;
    char b[32];
    if (s->kind == K_GAIN || s->kind == K_CURVE) {
        iniStr(s->sec, s->key, "", b, sizeof b);
        v = b[0] ? (int)(atof(b) * (s->kind == K_GAIN ? 10 : 100) + 0.5) : s->def;
    } else {
        v = iniInt(s->sec, s->key, s->def);
    }
    if (v < s->min) v = s->min;
    if (v > s->max) v = s->max;
    SendMessageW(s->tb, TBM_SETPOS, TRUE, v);
    sliderText(idx);
}

static void setStatusW(const WCHAR *s) { SetWindowTextW(g_status, s); }
static void setStatus(int sid) { setStatusW(sid >= 0 ? T(sid) : L""); }
static void setHelp(int sid) { SetWindowTextW(g_help, sid >= 0 ? T(sid) : L""); }
static void savedMsg(BOOL ok, BOOL live) { setStatus(!ok ? S_SAVE_FAIL : live ? S_SAVED_LIVE : S_SAVED_RESTART); }

static void updateGameInfo(void)
{
    if (!g_exe[0]) {
        SetWindowTextW(g_gameInfo, L"");
        SetWindowTextW(g_installStatus, T(S_ST_NOGAME));
        return;
    }
    int in = analyzeExe(g_exe, &g_exeArch);
    WCHAR b[300];
    if (in == IN_BAD) {
        lstrcpyW(b, T(S_INFO_BAD));
    } else {
        int sid = in == IN_DI8 ? S_INFO_DI8 : in == IN_DI7 ? S_INFO_DI7 : in == IN_XINPUT ? S_INFO_XI : S_INFO_UNK;
        swprintf(b, 300, L"%ls \xb7 %ls", T(g_exeArch == 64 ? S_INFO_64 : S_INFO_32), T(sid));
    }
    SetWindowTextW(g_gameInfo, b);
    int st = installState();
    SetWindowTextW(g_installStatus, T(st == 2 ? S_ST_INSTALLED : st == 1 ? S_ST_OLD : S_ST_NOTINST));
}

static void fillGameCombo(void)
{
    SendMessageW(g_gameCombo, CB_RESETCONTENT, 0, 0);
    int sel = -1;
    for (int i = 0; i < g_ngames; i++) {
        WCHAR b[MAX_PATH + 40], dir[MAX_PATH];
        lstrcpyW(dir, g_games[i]);
        WCHAR *file = wcsrchr(dir, L'\\');
        if (file) *file++ = 0;
        WCHAR *parent = wcsrchr(dir, L'\\');
        /* "Rig n Roll\Bin\rnr.exe": mostra a pasta do jogo e o .exe */
        if (parent && !lstrcmpiW(parent + 1, L"Bin")) { *parent = 0; parent = wcsrchr(dir, L'\\'); }
        swprintf(b, MAX_PATH + 40, L"%ls  (%ls)", parent ? parent + 1 : dir, file ? file : g_games[i]);
        SendMessageW(g_gameCombo, CB_ADDSTRING, 0, (LPARAM)b);
        if (!lstrcmpiW(g_games[i], g_exe)) sel = i;
    }
    SendMessageW(g_gameCombo, CB_SETCURSEL, sel, 0);
}

static void fillDevCombos(void)
{
    for (int r = 0; r < NDEV; r++) {
        HWND c = g_devCombo[r];
        SendMessageW(c, CB_RESETCONTENT, 0, 0);
        int n = 0, sel = 0;
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)T(S_NONE));
        g_comboIds[r][n++] = 0;
        for (int i = 0; i < g_nlist && n < 39; i++) {
            WCHAR b[200];
            swprintf(b, 200, L"%ls  [%04X:%04X]", g_list[i].name, g_list[i].id & 0xFFFF, g_list[i].id >> 16);
            SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)b);
            if (g_list[i].id == g_dev[r].id) sel = n;
            g_comboIds[r][n++] = g_list[i].id;
        }
        if (g_dev[r].id && !sel) {
            WCHAR b[200];
            swprintf(b, 200, L"%ls  [%04X:%04X]", T(S_DISCONNECTED), g_dev[r].id & 0xFFFF, g_dev[r].id >> 16);
            SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)b);
            sel = n;
            g_comboIds[r][n++] = g_dev[r].id;
        }
        SendMessageW(c, CB_SETCURSEL, sel, 0);
    }
}

/* aba de eixos: linhas do dispositivo escolhido */
static void loadAxisRows(void)
{
    BOOL edit = g_axDev > 0;
    char sec[8], b[32], key[16];
    srcSec(g_axDev, sec);
    for (int a = 0; a < NAXES; a++) {
        int sel = 0;
        if (edit) {
            iniStr(sec, AXIS_NAMES[a], "", b, sizeof b);
            size_t n = strlen(b);
            int k = 0;
            if (n && (b[n - 1] == '+' || b[n - 1] == '-')) { k = b[n - 1] == '+' ? 1 : 2; b[n - 1] = 0; }
            for (int t = 0; t < NAXES; t++)
                if (b[0] && !lstrcmpiA(b, AXIS_NAMES[t])) sel = 1 + t * 3 + k;
        }
        SendMessageW(g_mapCombo[a], CB_SETCURSEL, sel, 0);
        snprintf(key, sizeof key, "invert_%s", AXIS_NAMES[a]);
        setCheck(g_invCk[a], edit && iniInt(sec, key, 0));
        EnableWindow(g_mapCombo[a], edit);
        EnableWindow(g_invCk[a], edit);
    }
}

static void fillAxSrcCombo(void)
{
    SendMessageW(g_axSrcCombo, CB_RESETCONTENT, 0, 0);
    g_naxSrcItems = 0;
    int sel = 0;
    for (int r = 1; r < NDEV; r++) {
        if (!g_dev[r].id) continue;
        WCHAR b[200];
        swprintf(b, 200, L"%ls %d: %ls", T(S_EXTRA), r, devName(g_dev[r].id));
        SendMessageW(g_axSrcCombo, CB_ADDSTRING, 0, (LPARAM)b);
        if (r == g_axDev) sel = g_naxSrcItems;
        g_axSrcItems[g_naxSrcItems++] = r;
    }
    if (!g_naxSrcItems) {
        WCHAR b[200];
        swprintf(b, 200, L"%ls %ls", devName(g_dev[0].id), T(S_WHEELAXES));
        SendMessageW(g_axSrcCombo, CB_ADDSTRING, 0, (LPARAM)b);
        g_axSrcItems[g_naxSrcItems++] = 0;
    }
    SendMessageW(g_axSrcCombo, CB_SETCURSEL, sel, 0);
    g_axDev = g_axSrcItems[sel];
    loadAxisRows();
}

static void refreshDevices(void)
{
    g_nlist = 0;
    if (g_di) g_di->lpVtbl->EnumDevices(g_di, DI8DEVCLASS_GAMECTRL, enumCb, NULL, DIEDFL_ATTACHEDONLY);
    fillDevCombos();
    for (int r = 0; r < NDEV; r++) openDev(r);
    fillAxSrcCombo();
}

static void loadAll(void)
{
    char b[64], sec[8];
    for (int i = 0; i < SL_COUNT; i++) loadSlider(i);
    for (int i = 0; i < CK_COUNT; i++) setCheck(g_ck[i].h, iniInt(g_ck[i].sec, g_ck[i].key, g_ck[i].def));
    g_center = iniInt("steering", "center", 32768);
    WCHAR c[32];
    swprintf(c, 32, L"%ld", g_center);
    SetWindowTextW(g_centerLbl, c);
    iniStr("devices", "wheel", "", b, sizeof b);
    g_dev[0].id = parseVidPid(b);
    for (int r = 1; r < NDEV; r++) {
        srcSec(r, sec);
        iniStr(sec, "device", "", b, sizeof b);
        g_dev[r].id = parseVidPid(b);
        setCheck(g_hideCk[r], iniInt(sec, "hide", 1));
        setCheck(g_btnCk[r], iniInt(sec, "buttons", 1));
        SendMessageW(g_ofsCombo[r], CB_SETCURSEL, iniInt(sec, "button_offset", 0), 0);
    }
    refreshDevices();
}

/* troca de jogo: pasta, ini (cria/migra) e tudo da tela */
static void selectGame(const WCHAR *exe)
{
    lstrcpynW(g_exe, exe, MAX_PATH);
    lstrcpyW(g_bin, exe);
    WCHAR *s = wcsrchr(g_bin, L'\\');
    if (s) *s = 0;
    iniPath();
    ensureIni();
    if (installState() != 1) migrateLegacy();  /* DLL antiga instalada so entende o formato antigo */
    saveGames();
    fillGameCombo();
    loadAll();
    updateGameInfo();
}

static void applyLang(void)
{
    SetWindowTextW(g_main, T(S_TITLE));
    static const int tabs[PG_COUNT] = { S_TAB_GENERAL, S_TAB_DEVICES, S_TAB_STEER, S_TAB_PEDALS, S_TAB_FFB };
    for (int i = 0; i < PG_COUNT; i++) {
        TCITEMW it = { TCIF_TEXT };
        it.pszText = (WCHAR *)T(tabs[i]);
        SendMessageW(g_tab, TCM_SETITEMW, i, (LPARAM)&it);
    }
    for (int i = 0; i < g_ntexts; i++) SetWindowTextW(g_texts[i].h, T(g_texts[i].sid));
    for (int r = 1; r < NDEV; r++) {
        WCHAR b[32];
        swprintf(b, 32, L"%ls %d", T(S_EXTRA), r);
        SetWindowTextW(GetDlgItem(g_page[PG_DEVICES], ID_LBL0 + r), b);
    }
    int sel = (int)SendMessageW(g_profileCombo, CB_GETCURSEL, 0, 0);
    SendMessageW(g_profileCombo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < NPROFILES; i++) SendMessageW(g_profileCombo, CB_ADDSTRING, 0, (LPARAM)T(g_profiles[i].sid));
    SendMessageW(g_profileCombo, CB_SETCURSEL, sel < 0 ? 0 : sel, 0);
    fillDevCombos();
    fillAxSrcCombo();
    for (int i = 0; i < SL_COUNT; i++) sliderText(i);
    updateGameInfo();
    setHelp(-1);
    setStatus(-1);
}

/* ------------------------------------------------------------- acoes */

static void doInstall(void)
{
    WCHAR src[MAX_PATH], dst[MAX_PATH], bak[MAX_PATH], from[MAX_PATH];
    if (!g_exe[0]) { setStatus(S_ST_NOGAME); return; }
    int in = analyzeExe(g_exe, &g_exeArch);
    if (in == IN_XINPUT && MessageBoxW(g_main, T(S_XINPUT_WARN), T(S_TITLE), MB_ICONWARNING | MB_YESNO) != IDYES) return;
    /* DLLs em x86\ e x64\ ao lado do programa; aceita tambem as de 32 bits soltas */
    swprintf(from, MAX_PATH, L"%ls\\%ls", g_selfDir, g_exeArch == 64 ? L"x64" : L"x86");
    swprintf(src, MAX_PATH, L"%ls\\dinput8.dll", from);
    if (GetFileAttributesW(src) == INVALID_FILE_ATTRIBUTES && g_exeArch != 64) lstrcpyW(from, g_selfDir);
    for (int i = 0; i < 2; i++) {
        swprintf(src, MAX_PATH, L"%ls\\%ls", from, DLLS[i]);
        if (GetFileAttributesW(src) == INVALID_FILE_ATTRIBUTES) {
            MessageBoxW(g_main, T(S_INSTALL_NOSRC), T(S_TITLE), MB_ICONWARNING);
            return;
        }
    }
    if (!lstrcmpiW(from, g_bin)) { updateGameInfo(); return; }
    BOOL asked = FALSE;
    for (int i = 0; i < 2; i++) {
        swprintf(dst, MAX_PATH, L"%ls\\%ls", g_bin, DLLS[i]);
        if (GetFileAttributesW(dst) == INVALID_FILE_ATTRIBUTES || dllVersion(dst)) continue;
        if (!asked && MessageBoxW(g_main, T(S_FOREIGN), T(S_TITLE), MB_ICONWARNING | MB_YESNO) != IDYES) return;
        asked = TRUE;
        swprintf(bak, MAX_PATH, L"%ls.bak", dst);
        MoveFileExW(dst, bak, MOVEFILE_REPLACE_EXISTING);
    }
    for (int i = 0; i < 2; i++) {
        swprintf(src, MAX_PATH, L"%ls\\%ls", from, DLLS[i]);
        swprintf(dst, MAX_PATH, L"%ls\\%ls", g_bin, DLLS[i]);
        if (!CopyFileW(src, dst, FALSE)) {
            MessageBoxW(g_main, T(S_INSTALL_FAIL), T(S_TITLE), MB_ICONWARNING);
            updateGameInfo();
            return;
        }
    }
    ensureIni();
    migrateLegacy();
    loadAll();
    updateGameInfo();
    MessageBoxW(g_main, T(S_INSTALL_OK), T(S_TITLE), MB_ICONINFORMATION);
}

static void doRemove(void)
{
    WCHAR dst[MAX_PATH], bak[MAX_PATH];
    if (!g_exe[0]) return;
    for (int i = 0; i < 2; i++) {
        swprintf(dst, MAX_PATH, L"%ls\\%ls", g_bin, DLLS[i]);
        if (!dllVersion(dst)) continue;
        if (!DeleteFileW(dst)) {
            MessageBoxW(g_main, T(S_REMOVE_FAIL), T(S_TITLE), MB_ICONWARNING);
            updateGameInfo();
            return;
        }
        swprintf(bak, MAX_PATH, L"%ls.bak", dst);
        MoveFileW(bak, dst);  /* devolve a DLL de outro mod, se havia */
    }
    updateGameInfo();
    MessageBoxW(g_main, T(S_REMOVE_OK), T(S_TITLE), MB_ICONINFORMATION);
}

static void doAddGame(void)
{
    WCHAR file[MAX_PATH] = L"";
    OPENFILENAMEW of = { sizeof of };
    of.hwndOwner = g_main;
    of.lpstrFilter = L"*.exe\0*.exe\0";
    of.lpstrFile = file;
    of.nMaxFile = MAX_PATH;
    of.lpstrTitle = T(S_PICK_EXE);
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&of)) return;
    if (addGame(file) < 0) return;
    selectGame(file);
}

static void applyProfile(int i)
{
    if (i < 0 || i >= NPROFILES) return;
    const Profile *p = &g_profiles[i];
    char b[32];
    iniBegin();
    iniSetInt("ffb", "gain", p->ffGain);
    iniSetInt("ffb", "spring", p->spring);
    iniSetInt("ffb", "spring_gain_max", p->springMax);
    iniSetInt("ffb", "damper", p->damper);
    iniSetInt("ffb", "friction", p->friction);
    iniSetInt("ffb", "inertia", p->inertia);
    iniSetInt("ffb", "constant", p->constant);
    iniSetInt("ffb", "periodic", p->periodic);
    iniSetInt("ffb", "ramp", p->ramp);
    iniSetInt("ffb", "enforce_duration", 1);
    iniSetInt("ffb", "autocenter_off", 1);
    iniSetInt("ffb", "spring_offset_zero", 1);
    iniSetInt("steering", "deadzone", p->dz);
    double gain = sliderPos(SL_ROT) / (double)p->lockDeg;
    if (gain < 0.5) gain = 0.5;
    if (gain > 6) gain = 6;
    snprintf(b, sizeof b, "%.1f", gain);
    iniSet("steering", "gain", b);
    snprintf(b, sizeof b, "%.2f", p->curve / 100.0);
    iniSet("steering", "curve", b);
    BOOL ok = iniCommit();
    for (int k = 0; k < SL_COUNT; k++) loadSlider(k);
    for (int k = 0; k < CK_COUNT; k++) setCheck(g_ck[k].h, iniInt(g_ck[k].sec, g_ck[k].key, g_ck[k].def));
    setStatus(ok ? S_PROFILE_OK : S_SAVE_FAIL);
}

static void saveMap(int a)
{
    if (g_axDev <= 0) return;
    char sec[8], b[8];
    srcSec(g_axDev, sec);
    int sel = (int)SendMessageW(g_mapCombo[a], CB_GETCURSEL, 0, 0);
    if (sel <= 0) {
        iniSet(sec, AXIS_NAMES[a], NULL);
    } else {
        int t = (sel - 1) / 3, k = (sel - 1) % 3;
        snprintf(b, sizeof b, "%s%s", AXIS_NAMES[t], k == 1 ? "+" : k == 2 ? "-" : "");
        iniSet(sec, AXIS_NAMES[a], b);
    }
    savedMsg(TRUE, TRUE);
}

static void saveInvert(int a)
{
    if (g_axDev <= 0) return;
    char sec[8], key[16];
    srcSec(g_axDev, sec);
    snprintf(key, sizeof key, "invert_%s", AXIS_NAMES[a]);
    iniSet(sec, key, getCheck(g_invCk[a]) ? "1" : "0");
    savedMsg(TRUE, TRUE);
}

static void startDetect(void)
{
    g_detectEnd = GetTickCount() + 4000;
    memset(g_detSeen, 0, sizeof g_detSeen);
}

/* fim da deteccao: o eixo que mais andou vira a linha escolhida; repouso alto = invertido */
static void finishDetect(void)
{
    g_detectEnd = 0;
    int bestDev = -1, bestAx = -1;
    LONG best = 8000;
    for (int r = 1; r < NDEV; r++) {
        if (!g_detSeen[r]) continue;
        for (int a = 0; a < NAXES; a++) {
            LONG range = g_detMax[r][a] - g_detMin[r][a];
            if (range > best) { best = range; bestDev = r; bestAx = a; }
        }
    }
    if (bestDev < 0) { setStatus(S_DETECT_NONE); return; }
    g_axDev = bestDev;
    fillAxSrcCombo();
    BOOL inv = g_detFirst[bestDev][bestAx] > (g_detMin[bestDev][bestAx] + g_detMax[bestDev][bestAx]) / 2;
    setCheck(g_invCk[bestAx], inv);
    saveInvert(bestAx);
    SetFocus(g_mapCombo[bestAx]);
    WCHAR b[300];
    swprintf(b, 300, T(S_DETECTED), devName(g_dev[bestDev].id), AXIS_NAMES[bestAx]);
    setStatusW(b);
}

/* mesma conta do adaptador: ganho, curva com joelho e zona morta */
static double steerOut(LONG raw)
{
    double gain = sliderPos(SL_GAIN) / 10.0, curve = sliderPos(SL_CURVE) / 100.0, dz = sliderPos(SL_DZ) / 100.0;
    double n = (raw - g_center) * gain / 32767.5;
    double a = fabs(n) > 1 ? 1 : fabs(n);
    const double knee = 0.08;
    double c = a >= knee ? pow(a, curve) : a * pow(knee, curve - 1);
    a = dz > 0 ? (a < 0.002 ? 0 : dz + (1 - dz) * c) : c;
    return n < 0 ? -a : a;
}

static void onTimer(void)
{
    for (int r = 0; r < NDEV; r++) pollDev(r);
    int page = (int)SendMessageW(g_tab, TCM_GETCURSEL, 0, 0);

    if (g_detectEnd) {
        for (int r = 1; r < NDEV; r++) {
            if (!g_dev[r].ok) continue;
            for (int a = 0; a < NAXES; a++) {
                LONG v = axisVal(&g_dev[r].st, a);
                if (!g_detSeen[r]) g_detFirst[r][a] = g_detMin[r][a] = g_detMax[r][a] = v;
                if (v < g_detMin[r][a]) g_detMin[r][a] = v;
                if (v > g_detMax[r][a]) g_detMax[r][a] = v;
            }
            g_detSeen[r] = 1;
        }
        LONG left = (LONG)(g_detectEnd - GetTickCount());
        if (left <= 0) {
            finishDetect();
        } else {
            WCHAR b[200];
            swprintf(b, 200, T(S_DETECTING), (int)(left / 1000) + 1);
            setStatusW(b);
        }
    }

    if (page == PG_STEER) {
        Dev *w = &g_dev[0];
        if (w->ok) {
            LONG raw = w->st.lX;
            double deg = (raw - g_center) * sliderPos(SL_ROT) / 65535.0, out = steerOut(raw);
            double half = sliderPos(SL_ROT) / 2.0;
            setBar(g_steerBar[0], (int)(deg / half * 1000));
            setBar(g_steerBar[1], (int)(out * 1000));
            char b[64];
            snprintf(b, sizeof b, "%+.1f\xc2\xb0", deg);
            SetWindowTextW(g_steerTxt[0], W(b));
            snprintf(b, sizeof b, "%+.0f%%", out * 100);
            SetWindowTextW(g_steerTxt[1], W(b));
        } else {
            setBar(g_steerBar[0], 0);
            setBar(g_steerBar[1], 0);
            SetWindowTextW(g_steerTxt[0], T(S_NOWHEEL));
            SetWindowTextW(g_steerTxt[1], L"");
        }
    } else if (page == PG_PEDALS) {
        Dev *src = &g_dev[g_axDev];
        for (int a = 0; a < NAXES; a++) {
            LONG v = src->ok ? axisVal(&src->st, a) : 0;
            setBar(g_pedBar[a], (int)(v * 1000 / 65535));
            WCHAR b[16];
            swprintf(b, 16, L"%ld", v);
            SetWindowTextW(g_pedVal[a], b);
        }
    } else if (page == PG_DEVICES) {
        WCHAR b[400];
        int n = swprintf(b, 400, L"%ls ", T(S_BTNS));
        for (int r = 1; r < NDEV && n < 360; r++) {
            if (!g_dev[r].ok) continue;
            BOOL any = FALSE;
            for (int i = 0; i < 32 && n < 380; i++) {
                if (!(g_dev[r].st.rgbButtons[i] & 0x80)) continue;
                if (!any) n += swprintf(b + n, 400 - n, L" %ls %d:", T(S_EXTRA), r);
                any = TRUE;
                n += swprintf(b + n, 400 - n, L" %d", i + 1);
            }
        }
        SetWindowTextW(g_btnTxt, b);
    }
}

static void onHScroll(HWND tb, int code)
{
    int idx = (int)GetWindowLongPtrW(tb, GWLP_USERDATA) - 1;
    if (idx < 0 || idx >= SL_COUNT) return;
    sliderText(idx);
    if (idx == SL_ROT) sliderText(SL_GAIN);
    setHelp(g_sl[idx].hid);
    if (code == TB_ENDTRACK) {
        sliderSave(idx);
        savedMsg(TRUE, TRUE);
    }
}

static void onCommand(int id, int code)
{
    char sec[8];
    if (id == ID_LANG && code == CBN_SELCHANGE) {
        g_lang = (int)SendMessageW(g_langCombo, CB_GETCURSEL, 0, 0) == 1;
        DWORD l = g_lang;
        regSave(L"Lang", &l, REG_DWORD, sizeof l);
        applyLang();
    } else if (id == ID_GAME && code == CBN_SELCHANGE) {
        int i = (int)SendMessageW(g_gameCombo, CB_GETCURSEL, 0, 0);
        if (i >= 0 && i < g_ngames) selectGame(g_games[i]);
    } else if (id == ID_ADDGAME) {
        doAddGame();
    } else if (id == ID_INSTALL) {
        doInstall();
    } else if (id == ID_REMOVE) {
        doRemove();
    } else if (id == ID_APPLY_PROFILE) {
        applyProfile((int)SendMessageW(g_profileCombo, CB_GETCURSEL, 0, 0));
    } else if (id == ID_OPENLOG) {
        WCHAR p[MAX_PATH];
        swprintf(p, MAX_PATH, L"%ls\\pedalmerge.log", g_bin);
        ShellExecuteW(g_main, L"open", p, NULL, NULL, SW_SHOWNORMAL);
    } else if (id == ID_OPENDIR) {
        ShellExecuteW(g_main, L"open", g_bin, NULL, NULL, SW_SHOWNORMAL);
    } else if (id == ID_REFRESH) {
        refreshDevices();
    } else if (id == ID_DETECT) {
        startDetect();
    } else if (id == ID_AXSRC && code == CBN_SELCHANGE) {
        int i = (int)SendMessageW(g_axSrcCombo, CB_GETCURSEL, 0, 0);
        if (i >= 0 && i < g_naxSrcItems) g_axDev = g_axSrcItems[i];
        loadAxisRows();
    } else if (id == ID_SETCENTER || id == ID_RESETCENTER) {
        if (id == ID_SETCENTER && !g_dev[0].ok) { setStatus(S_NOWHEEL); return; }
        g_center = id == ID_SETCENTER ? g_dev[0].st.lX : 32768;
        iniSetInt("steering", "center", (int)g_center);
        WCHAR w[32];
        swprintf(w, 32, L"%ld", g_center);
        SetWindowTextW(g_centerLbl, w);
        savedMsg(TRUE, TRUE);
    } else if (id >= ID_DEV0 && id < ID_DEV0 + NDEV && code == CBN_SELCHANGE) {
        int r = id - ID_DEV0, sel = (int)SendMessageW(g_devCombo[r], CB_GETCURSEL, 0, 0);
        g_dev[r].id = sel >= 0 ? g_comboIds[r][sel] : 0;
        char b[16] = "";
        if (g_dev[r].id) snprintf(b, sizeof b, "%04lX:%04lX", g_dev[r].id & 0xFFFF, g_dev[r].id >> 16);
        if (r == 0) {
            iniSet("devices", "wheel", b);
        } else {
            srcSec(r, sec);
            iniBegin();
            iniSet(sec, "device", b);
            iniSetInt(sec, "hide", getCheck(g_hideCk[r]));
            iniSetInt(sec, "buttons", getCheck(g_btnCk[r]));
            iniCommit();
        }
        openDev(r);
        fillAxSrcCombo();
        savedMsg(TRUE, FALSE);
    } else if (id >= ID_HIDE0 && id < ID_HIDE0 + NDEV) {
        srcSec(id - ID_HIDE0, sec);
        iniSetInt(sec, "hide", getCheck(g_hideCk[id - ID_HIDE0]));
        savedMsg(TRUE, FALSE);
    } else if (id >= ID_BTN0 && id < ID_BTN0 + NDEV) {
        srcSec(id - ID_BTN0, sec);
        iniSetInt(sec, "buttons", getCheck(g_btnCk[id - ID_BTN0]));
        savedMsg(TRUE, TRUE);
    } else if (id >= ID_OFS0 && id < ID_OFS0 + NDEV && code == CBN_SELCHANGE) {
        srcSec(id - ID_OFS0, sec);
        iniSetInt(sec, "button_offset", (int)SendMessageW(g_ofsCombo[id - ID_OFS0], CB_GETCURSEL, 0, 0));
        savedMsg(TRUE, TRUE);
    } else if (id >= ID_CHECK0 && id < ID_CHECK0 + CK_COUNT) {
        Check *c = &g_ck[id - ID_CHECK0];
        iniSet(c->sec, c->key, getCheck(c->h) ? "1" : "0");
        savedMsg(TRUE, c->live);
    } else if (id >= ID_MAP0 && id < ID_MAP0 + NAXES && code == CBN_SELCHANGE) {
        saveMap(id - ID_MAP0);
    } else if (id >= ID_INV0 && id < ID_INV0 + NAXES) {
        saveInvert(id - ID_INV0);
    }
}

/* ------------------------------------------------------------- paginas */

static LRESULT CALLBACK pageProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_COMMAND:
    case WM_HSCROLL:
    case WM_NOTIFY:
        return SendMessageW(g_main, m, wp, lp);
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)g_pageBrush;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static void buildGeneral(HWND p)
{
    int y = 12;
    txt(p, S_LANG, 12, y + 4, 150, 20, 0);
    g_langCombo = combo(p, 170, y, 200, ID_LANG);
    SendMessageW(g_langCombo, CB_ADDSTRING, 0, (LPARAM)L"Portugu\x00eas");
    SendMessageW(g_langCombo, CB_ADDSTRING, 0, (LPARAM)L"English");
    SendMessageW(g_langCombo, CB_SETCURSEL, g_lang, 0);
    y += 44;
    txt(p, S_GAME, 12, y + 4, 150, 20, 0);
    g_gameCombo = combo(p, 170, y, 320, ID_GAME);
    btn(p, S_ADDGAME, 500, y - 1, 140, 28, ID_ADDGAME, 0);
    y += 32;
    g_gameInfo = mk(p, L"STATIC", 0, 170, y, 470, 20, 0);
    y += 34;
    g_installStatus = mk(p, L"STATIC", 0, 12, y + 5, 260, 20, 0);
    SendMessageW(g_installStatus, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
    btn(p, S_INSTALL, 280, y, 175, 30, ID_INSTALL, 0);
    btn(p, S_REMOVE, 465, y, 175, 30, ID_REMOVE, 0);
    y += 52;
    txt(p, S_PROFILE, 12, y + 4, 150, 20, 0);
    g_profileCombo = combo(p, 170, y, 330, ID_PROFILE);
    btn(p, S_APPLY_PROFILE, 510, y - 1, 130, 28, ID_APPLY_PROFILE, 0);
    y += 52;
    y = checkY(CK_LOG, 12, y, 400);
    y = checkY(CK_LOGSTEER, 12, y, 400);
    btn(p, S_OPENLOG, 12, y + 6, 130, 28, ID_OPENLOG, 0);
    btn(p, S_OPENDIR, 150, y + 6, 130, 28, ID_OPENDIR, 0);
    y += 56;
    txt(p, S_GENERAL_NOTE, 12, y, 628, 60, 0);
}

static void buildDevices(HWND p)
{
    int y = 12;
    txt(p, S_WHEEL, 12, y + 4, 120, 20, 0);
    g_devCombo[0] = combo(p, 135, y, 400, ID_DEV0);
    y += 44;
    txt(p, S_EXTRAS, 12, y, 628, 20, 0);
    y += 26;
    for (int r = 1; r < NDEV; r++) {
        mk(p, L"STATIC", 0, 12, y + 4, 60, 20, ID_LBL0 + r);
        g_devCombo[r] = combo(p, 75, y, 300, ID_DEV0 + r);
        g_hideCk[r] = btn(p, S_HIDE, 385, y + 2, 95, 22, ID_HIDE0 + r, BS_AUTOCHECKBOX);
        g_btnCk[r] = btn(p, S_BUTTONS, 482, y + 2, 85, 22, ID_BTN0 + r, BS_AUTOCHECKBOX);
        g_ofsCombo[r] = combo(p, 570, y, 70, ID_OFS0 + r);
        for (int k = 0; k < 32; k++) {
            WCHAR b[8];
            swprintf(b, 8, L"+%d", k);
            SendMessageW(g_ofsCombo[r], CB_ADDSTRING, 0, (LPARAM)b);
        }
        y += 34;
    }
    btn(p, S_REFRESH, 75, y + 4, 160, 28, ID_REFRESH, 0);
    y += 44;
    g_btnTxt = mk(p, L"STATIC", 0, 12, y, 628, 20, 0);
    y += 30;
    txt(p, S_DEV_NOTE, 12, y, 628, 80, 0);
}

static void buildSteer(HWND p)
{
    int y = 12;
    y = sliderY(SL_ROT, y);
    y = sliderY(SL_GAIN, y);
    y = sliderY(SL_CURVE, y);
    y = sliderY(SL_DZ, y);
    y += 10;
    txt(p, S_CENTER, 12, y + 5, 80, 20, 0);
    g_centerLbl = mk(p, L"STATIC", 0, 95, y + 5, 80, 20, 0);
    btn(p, S_SETCENTER, 225, y, 190, 28, ID_SETCENTER, 0);
    btn(p, S_RESETCENTER, 425, y, 100, 28, ID_RESETCENTER, 0);
    y += 46;
    static const int labels[2] = { S_PHYS, S_INGAME };
    for (int i = 0; i < 2; i++) {
        txt(p, labels[i], 12, y + 2, 210, 20, 0);
        g_steerBar[i] = bar(p, 225, y, 270, 22, 1);
        g_steerTxt[i] = mk(p, L"STATIC", 0, 505, y + 2, 140, 20, 0);
        y += 32;
    }
    y += 12;
    txt(p, S_STEER_NOTE, 12, y, 628, 40, 0);
}

static void buildPedals(HWND p)
{
    int y = 8;
    txt(p, S_PED_NOTE, 12, y, 628, 38, 0);
    y += 44;
    txt(p, S_AXSRC, 12, y + 4, 100, 20, 0);
    g_axSrcCombo = combo(p, 115, y, 360, ID_AXSRC);
    btn(p, S_DETECT, 485, y - 1, 155, 28, ID_DETECT, 0);
    y += 40;
    for (int a = 0; a < NAXES; a++) {
        WCHAR b[16];
        swprintf(b, 16, L"%hs", AXIS_NAMES[a]);
        HWND l = mk(p, L"STATIC", 0, 12, y + 4, 40, 20, 0);
        SetWindowTextW(l, b);
        g_pedBar[a] = bar(p, 55, y + 2, 250, 20, 0);
        g_pedVal[a] = mk(p, L"STATIC", 0, 312, y + 4, 60, 20, 0);
        g_mapCombo[a] = combo(p, 380, y, 110, ID_MAP0 + a);
        SendMessageW(g_mapCombo[a], CB_ADDSTRING, 0, (LPARAM)L"\x2014");
        for (int t = 0; t < NAXES; t++)
            for (int k = 0; k < 3; k++) {
                swprintf(b, 16, L"\x2192 %hs%hs", AXIS_NAMES[t], k == 1 ? "+" : k == 2 ? "-" : "");
                SendMessageW(g_mapCombo[a], CB_ADDSTRING, 0, (LPARAM)b);
            }
        g_invCk[a] = btn(p, S_INVERT, 505, y + 2, 130, 22, ID_INV0 + a, BS_AUTOCHECKBOX);
        y += 32;
    }
}

static void buildFfb(HWND p)
{
    int y = 8;
    for (int i = SL_FFGAIN; i <= SL_RAMP; i++) y = sliderY(i, y);
    y += 2;
    y = checkY(CK_ENFORCE, 12, y, 600);
    y = checkY(CK_AUTOC, 12, y, 600);
    y = checkY(CK_SPROFS, 12, y, 600);
    y += 4;
    HWND w = txt(p, S_FFB_WARN, 12, y, 628, 40, 0);
    SendMessageW(w, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
}

static void showPage(int i)
{
    for (int k = 0; k < PG_COUNT; k++) ShowWindow(g_page[k], k == i ? SW_SHOW : SW_HIDE);
    setHelp(-1);
}

/* ------------------------------------------------------------- janela principal */

static LRESULT CALLBACK mainProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_COMMAND:
        onCommand(LOWORD(wp), HIWORD(wp));
        return 0;
    case WM_HSCROLL:
        if (lp) onHScroll((HWND)lp, LOWORD(wp));
        return 0;
    case WM_NOTIFY:
        if (((NMHDR *)lp)->hwndFrom == g_tab && ((NMHDR *)lp)->code == TCN_SELCHANGE)
            showPage((int)SendMessageW(g_tab, TCM_GETCURSEL, 0, 0));
        return 0;
    case WM_TIMER:
        onTimer();
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    case WM_DESTROY:
        for (int r = 0; r < NDEV; r++) closeDev(r);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    (void)prev;
    g_inst = inst;
    CoInitialize(NULL);
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_TAB_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    GetModuleFileNameW(NULL, g_selfDir, MAX_PATH);
    *wcsrchr(g_selfDir, L'\\') = 0;

    DWORD lang;
    if (regLoad(L"Lang", &lang, sizeof lang)) g_lang = lang == 1;
    else g_lang = PRIMARYLANGID(GetUserDefaultUILanguage()) != LANG_PORTUGUESE;

    HDC dc = GetDC(NULL);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    g_font = CreateFontW(-MulDiv(9, g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBold = CreateFontW(-MulDiv(9, g_dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_pageBrush = GetSysColorBrush(COLOR_WINDOW);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = mainProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = L"LegacyWheelMain";
    RegisterClassW(&wc);
    wc.lpfnWndProc = pageProc;
    wc.hbrBackground = g_pageBrush;
    wc.hIcon = NULL;
    wc.lpszClassName = L"LegacyWheelPage";
    RegisterClassW(&wc);
    wc.lpfnWndProc = barProc;
    wc.cbWndExtra = 2 * sizeof(LONG_PTR);
    wc.lpszClassName = L"LegacyWheelBar";
    RegisterClassW(&wc);

    int cw = 680, ch = 620;
    RECT wr = { 0, 0, S(cw), S(ch) };
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&wr, style, FALSE);
    g_main = CreateWindowExW(0, L"LegacyWheelMain", L"", style, CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left,
                             wr.bottom - wr.top, NULL, NULL, inst, NULL);

    g_tab = mk(g_main, WC_TABCONTROLW, WS_CLIPSIBLINGS | WS_TABSTOP, 8, 8, cw - 16, ch - 100, ID_TAB);
    static const int tabs[PG_COUNT] = { S_TAB_GENERAL, S_TAB_DEVICES, S_TAB_STEER, S_TAB_PEDALS, S_TAB_FFB };
    for (int i = 0; i < PG_COUNT; i++) {
        TCITEMW it = { TCIF_TEXT };
        it.pszText = (WCHAR *)T(tabs[i]);
        SendMessageW(g_tab, TCM_INSERTITEMW, i, (LPARAM)&it);
    }
    RECT pr = { S(8), S(8), S(8) + S(cw - 16), S(8) + S(ch - 100) };
    SendMessageW(g_tab, TCM_ADJUSTRECT, FALSE, (LPARAM)&pr);
    for (int i = 0; i < PG_COUNT; i++) {
        g_page[i] = CreateWindowExW(WS_EX_CONTROLPARENT, L"LegacyWheelPage", L"", WS_CHILD, pr.left, pr.top, pr.right - pr.left,
                                    pr.bottom - pr.top, g_main, NULL, inst, NULL);
        SetWindowPos(g_page[i], HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    g_help = mk(g_main, L"STATIC", 0, 12, ch - 86, cw - 24, 44, 0);
    g_status = mk(g_main, L"STATIC", 0, 12, ch - 36, cw - 24, 22, 0);
    SendMessageW(g_status, WM_SETFONT, (WPARAM)g_fontBold, TRUE);

    buildGeneral(g_page[PG_GENERAL]);
    buildDevices(g_page[PG_DEVICES]);
    buildSteer(g_page[PG_STEER]);
    buildPedals(g_page[PG_PEDALS]);
    buildFfb(g_page[PG_FFB]);

    initDI();

    /* jogos: os ja usados, o .exe ao lado deste programa e o Rig'n'Roll da Steam */
    loadGames();
    WCHAR p[MAX_PATH];
    swprintf(p, MAX_PATH, L"%ls\\rnr.exe", g_selfDir);
    if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES) addGame(p);
    if (findRigNRoll(p)) addGame(p);
    WCHAR cur[MAX_PATH] = L"";
    regLoad(L"GameExe", cur, sizeof cur);
    int pick = -1;
    for (int i = 0; i < g_ngames; i++)
        if (!lstrcmpiW(g_games[i], cur)) pick = i;
    if (pick < 0 && g_ngames) pick = 0;
    if (pick >= 0) selectGame(g_games[pick]);
    applyLang();

    /* --tab N abre direto numa aba (usado nos testes de tela) */
    int first = 0;
    const WCHAR *t = wcsstr(cmd, L"--tab ");
    if (t) first = _wtoi(t + 6);
    if (first < 0 || first >= PG_COUNT) first = 0;
    SendMessageW(g_tab, TCM_SETCURSEL, first, 0);
    showPage(first);
    ShowWindow(g_main, show);
    SetTimer(g_main, ID_TIMER, 30, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (IsDialogMessageW(g_main, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
