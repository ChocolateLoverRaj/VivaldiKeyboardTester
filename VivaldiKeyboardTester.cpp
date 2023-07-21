// VivaldiKeyboardTester.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <cstdint>

#define UINT8 uint8_t
#define UINT16 uint16_t
#define UINT32 uint32_t
#define INT32 int32_t
#define USHORT uint16_t
#define ULONG uint32_t
#define PULONG ULONG *

#include <cstdbool>
#define BOOLEAN bool
#define TRUE true
#define FALSE false

#undef bool
#undef true
#undef false

#include <cstring>

#define RtlZeroMemory(Destination,Length) memset((Destination),0,(Length))
#define RtlCopyMemory memcpy

#undef memcpy
#undef memset

#include <iostream>
#include <cassert>
#include "keyboard.h"

const UINT8 fnKeys_set1[] = {
    0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58,

    //F13 - F16
    0x64, 0x64, 0x66, 0x67
};

void ReceiveKeys_Guarded(PKEYBOARD_INPUT_DATA startPtr, PKEYBOARD_INPUT_DATA endPtr, PULONG InputDataConsumed);

#define INTFLAG_NEW 0x1
#define INTFLAG_REMOVED 0x2

#include <pshpack1.h>

typedef struct RemapCfgKey {
    USHORT MakeCode;
    USHORT Flags;
} RemapCfgKey, * PRemapCfgKey;

typedef enum RemapCfgOverride {
    RemapCfgOverrideAutoDetect,
    RemapCfgOverrideEnable,
    RemapCfgOverrideDisable
} RemapCfgOverride, * PRemapCfgOverride;

typedef enum RemapCfgKeyState {
    RemapCfgKeyStateNoDetect,
    RemapCfgKeyStateEnforce,
    RemapCfgKeyStateEnforceNot
} RemapCfgKeyState, * PRemapCfgKeyState;

typedef struct RemapCfg {
    RemapCfgKeyState LeftCtrl;
    RemapCfgKeyState LeftAlt;
    RemapCfgKeyState Search;
    RemapCfgKeyState Assistant;
    RemapCfgKeyState LeftShift;
    RemapCfgKeyState RightCtrl;
    RemapCfgKeyState RightAlt;
    RemapCfgKeyState RightShift;
    RemapCfgKey originalKey;
    BOOLEAN remapVivaldiToFnKeys;
    RemapCfgKey remappedKey;
    RemapCfgKey additionalKeys[8];
} RemapCfg, * PRemapCfg;

typedef struct RemapCfgs {
    UINT32 magic;
    UINT32 remappings;
    BOOLEAN FlipSearchAndAssistantOnPixelbook;
    RemapCfgOverride HasAssistantKey;
    RemapCfgOverride IsNonChromeEC;
    RemapCfg cfg[1];
} RemapCfgs, * PRemapCfgs;
#include <poppack.h>

typedef struct KeyStruct {
    USHORT MakeCode;
    USHORT Flags;
    USHORT InternalFlags;
} KeyStruct, * PKeyStruct;

typedef struct RemappedKeyStruct {
    struct KeyStruct origKey;
    struct KeyStruct remappedKey;
} RemappedKeyStruct, * PRemappedKeyStruct;

#define MAX_CURRENT_KEYS 20

class VivaldiTester {
    UINT8 legacyTopRowKeys[10];
    UINT8 legacyVivaldi[10];

    UINT8 functionRowCount;
    KeyStruct functionRowKeys[16];

    PRemapCfgs remapCfgs;

    BOOLEAN LeftCtrlPressed;
    BOOLEAN LeftAltPressed;
    BOOLEAN LeftShiftPressed;
    BOOLEAN AssistantPressed;
    BOOLEAN SearchPressed;

    BOOLEAN RightCtrlPressed;
    BOOLEAN RightAltPressed;
    BOOLEAN RightShiftPressed;

    KeyStruct currentKeys[MAX_CURRENT_KEYS];
    KeyStruct lastKeyPressed;
    int numKeysPressed = 0;

    RemappedKeyStruct remappedKeys[MAX_CURRENT_KEYS];
    int numRemaps;

    void updateKey(KeyStruct key);

    BOOLEAN addRemap(RemappedKeyStruct remap);
    void garbageCollect();

    BOOLEAN checkKey(KEYBOARD_INPUT_DATA key, KeyStruct report[MAX_CURRENT_KEYS]);
    BOOLEAN addKey(KEYBOARD_INPUT_DATA key, KEYBOARD_INPUT_DATA data[MAX_CURRENT_KEYS]);

    INT32 IdxOfFnKey(RemapCfgKey originalKey);

    void RemapLoaded(KEYBOARD_INPUT_DATA report[MAX_CURRENT_KEYS], KEYBOARD_INPUT_DATA dataBefore[MAX_CURRENT_KEYS], KEYBOARD_INPUT_DATA dataAfter[MAX_CURRENT_KEYS]);

public:
    VivaldiTester();
    void ServiceCallback(PKEYBOARD_INPUT_DATA InputDataStart, PKEYBOARD_INPUT_DATA InputDataEnd, PULONG InputDataConsumed);
};

#define filterExt this
#define DbgPrint printf

VivaldiTester::VivaldiTester() {
    const UINT8 legacyVivaldi[] = {
        VIVALDI_BACK, VIVALDI_FWD, VIVALDI_REFRESH, VIVALDI_FULLSCREEN, VIVALDI_OVERVIEW, VIVALDI_BRIGHTNESSDN, VIVALDI_BRIGHTNESSUP, VIVALDI_MUTE, VIVALDI_VOLDN, VIVALDI_VOLUP
    };

    const UINT8 legacyVivaldiPixelbook[] = {
        VIVALDI_BACK, VIVALDI_REFRESH, VIVALDI_FULLSCREEN, VIVALDI_OVERVIEW, VIVALDI_BRIGHTNESSDN, VIVALDI_BRIGHTNESSUP, VIVALDI_PLAYPAUSE, VIVALDI_MUTE, VIVALDI_VOLDN, VIVALDI_VOLUP
    };

    filterExt->numKeysPressed = 0;
    RtlZeroMemory(&filterExt->currentKeys, sizeof(filterExt->currentKeys));
    RtlZeroMemory(&filterExt->lastKeyPressed, sizeof(filterExt->lastKeyPressed));

    RtlZeroMemory(&filterExt->remappedKeys, sizeof(filterExt->remappedKeys));
    filterExt->numRemaps = 0;

    filterExt->functionRowCount = 0;
    RtlZeroMemory(&filterExt->functionRowKeys, sizeof(filterExt->functionRowKeys));

    RtlCopyMemory(&filterExt->legacyTopRowKeys, &fnKeys_set1, sizeof(filterExt->legacyTopRowKeys));
    RtlCopyMemory(&filterExt->legacyVivaldi, &legacyVivaldi, sizeof(filterExt->legacyVivaldi));

    /*filterExt->functionRowCount = sizeof(filterExt->legacyVivaldi);
    for (int i = 0; i < sizeof(filterExt->legacyVivaldi); i++) {
        filterExt->functionRowKeys[i].MakeCode = filterExt->legacyVivaldi[i];
        filterExt->functionRowKeys[i].Flags |= KEY_E0;
    }*/

    filterExt->functionRowCount = 13;
    UINT8 jinlon_keys[] = { VIVALDI_BACK, VIVALDI_REFRESH, VIVALDI_FULLSCREEN, VIVALDI_OVERVIEW, VIVALDI_SNAPSHOT,
        VIVALDI_BRIGHTNESSDN, VIVALDI_BRIGHTNESSUP, VIVALDI_KBD_BKLIGHT_DOWN, VIVALDI_KBD_BKLIGHT_UP, VIVALDI_PLAYPAUSE,
        VIVALDI_MUTE, VIVALDI_VOLDN, VIVALDI_VOLUP };
    for (int i = 0; i < sizeof(jinlon_keys); i++) {
        filterExt->functionRowKeys[i].MakeCode = jinlon_keys[i];
        filterExt->functionRowKeys[i].Flags |= KEY_E0;
    }

    { //Generate wilco keys
        size_t cfgSizeWilco = offsetof(RemapCfgs, cfg) + sizeof(RemapCfg) * 12;
        PRemapCfgs wilcoCfgs = (PRemapCfgs)malloc(cfgSizeWilco);
        RtlZeroMemory(wilcoCfgs, cfgSizeWilco);

        //Begin map wilco keys (Fn key and delete keys present, so minimal mappings)

        wilcoCfgs->magic = REMAP_CFG_MAGIC;
        wilcoCfgs->FlipSearchAndAssistantOnPixelbook = TRUE;
        wilcoCfgs->HasAssistantKey = RemapCfgOverrideAutoDetect;
        wilcoCfgs->IsNonChromeEC = RemapCfgOverrideAutoDetect;
        wilcoCfgs->remappings = 12;

        //Map Fullscreen -> F11

        wilcoCfgs->cfg[0].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[0].LeftShift = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[0].originalKey.MakeCode = WILCO_FULLSCREEN;
        wilcoCfgs->cfg[0].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[0].remappedKey.MakeCode = fnKeys_set1[10];

        //Map Overview -> Windows + Tab

        wilcoCfgs->cfg[1].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[1].LeftShift = RemapCfgKeyStateEnforceNot;
        wilcoCfgs->cfg[1].Search = RemapCfgKeyStateEnforceNot;
        wilcoCfgs->cfg[1].originalKey.MakeCode = WILCO_OVERVIEW;
        wilcoCfgs->cfg[1].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[1].remappedKey.MakeCode = 0x0F;
        wilcoCfgs->cfg[1].additionalKeys[0].MakeCode = K_LWIN;
        wilcoCfgs->cfg[1].additionalKeys[0].Flags = KEY_E0;

        wilcoCfgs->cfg[2].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[2].LeftShift = RemapCfgKeyStateEnforceNot;
        wilcoCfgs->cfg[2].Search = RemapCfgKeyStateEnforce;
        wilcoCfgs->cfg[2].originalKey.MakeCode = WILCO_OVERVIEW;
        wilcoCfgs->cfg[2].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[2].remappedKey.MakeCode = 0x0F;

        //Map Shift + Overview -> Windows + Shift + S

        wilcoCfgs->cfg[3].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[3].LeftShift = RemapCfgKeyStateEnforce;
        wilcoCfgs->cfg[3].Search = RemapCfgKeyStateEnforceNot;
        wilcoCfgs->cfg[3].originalKey.MakeCode = WILCO_OVERVIEW;
        wilcoCfgs->cfg[3].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[3].remappedKey.MakeCode = 0x1F;
        wilcoCfgs->cfg[3].additionalKeys[0].MakeCode = K_LWIN;
        wilcoCfgs->cfg[3].additionalKeys[0].Flags = KEY_E0;

        wilcoCfgs->cfg[4].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[4].LeftShift = RemapCfgKeyStateEnforce;
        wilcoCfgs->cfg[4].Search = RemapCfgKeyStateEnforce;
        wilcoCfgs->cfg[4].originalKey.MakeCode = WILCO_OVERVIEW;
        wilcoCfgs->cfg[4].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[4].remappedKey.MakeCode = 0x1F;

        //Map Wilco Brightness -> Vivaldi Brightness
        wilcoCfgs->cfg[5].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[5].LeftShift = RemapCfgKeyStateEnforceNot;
        wilcoCfgs->cfg[5].Search = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[5].originalKey.MakeCode = WILCO_BRIGHTNESSDN;
        wilcoCfgs->cfg[5].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[5].remappedKey.MakeCode = VIVALDI_BRIGHTNESSDN;
        wilcoCfgs->cfg[5].remappedKey.Flags = KEY_E0;

        wilcoCfgs->cfg[6].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[6].LeftShift = RemapCfgKeyStateEnforceNot;
        wilcoCfgs->cfg[6].Search = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[6].originalKey.MakeCode = WILCO_BRIGHTNESSUP;
        wilcoCfgs->cfg[6].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[6].remappedKey.MakeCode = VIVALDI_BRIGHTNESSUP;
        wilcoCfgs->cfg[6].remappedKey.Flags = KEY_E0;

        //Map Shift + Wilco Brightness -> Vivaldi Keyboard Brightness

        wilcoCfgs->cfg[7].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[7].LeftShift = RemapCfgKeyStateEnforce;
        wilcoCfgs->cfg[7].Search = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[7].originalKey.MakeCode = WILCO_BRIGHTNESSDN;
        wilcoCfgs->cfg[7].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[7].remappedKey.MakeCode = VIVALDI_KBD_BKLIGHT_DOWN;
        wilcoCfgs->cfg[7].remappedKey.Flags = KEY_E0;

        wilcoCfgs->cfg[8].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[8].LeftShift = RemapCfgKeyStateEnforce;
        wilcoCfgs->cfg[8].Search = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[8].originalKey.MakeCode = WILCO_BRIGHTNESSUP;
        wilcoCfgs->cfg[8].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[8].remappedKey.MakeCode = VIVALDI_KBD_BKLIGHT_UP;
        wilcoCfgs->cfg[8].remappedKey.Flags = KEY_E0;

        //Map Project -> Windows + P

        wilcoCfgs->cfg[9].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[9].LeftShift = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[9].Search = RemapCfgKeyStateEnforceNot;
        wilcoCfgs->cfg[9].originalKey.MakeCode = WILCO_PROJECT;
        wilcoCfgs->cfg[9].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[9].remappedKey.MakeCode = 0x19;
        wilcoCfgs->cfg[9].additionalKeys[0].MakeCode = K_LWIN;
        wilcoCfgs->cfg[9].additionalKeys[0].Flags = KEY_E0;

        wilcoCfgs->cfg[10].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[10].LeftShift = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[10].Search = RemapCfgKeyStateEnforce;
        wilcoCfgs->cfg[10].originalKey.MakeCode = WILCO_PROJECT;
        wilcoCfgs->cfg[10].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[10].remappedKey.MakeCode = 0x19;

        //Map Assistant -> Caps Lock (Search was remapped to assistant)
        wilcoCfgs->cfg[11].LeftCtrl = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[11].LeftShift = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[11].Search = RemapCfgKeyStateNoDetect;
        wilcoCfgs->cfg[11].originalKey.MakeCode = K_ASSISTANT;
        wilcoCfgs->cfg[11].originalKey.Flags = KEY_E0;
        wilcoCfgs->cfg[11].remappedKey.MakeCode = 0x3A;

        FILE* dumpedSettingsFile;
        if (fopen_s(&dumpedSettingsFile, "croskbsettings-wilco.bin", "wb") == 0) {
            fwrite(wilcoCfgs, 1, cfgSizeWilco, dumpedSettingsFile);
            fclose(dumpedSettingsFile);

            DbgPrint("Wrote wilco settings to croskbsettings-wilco.bin!\n");
        }
        else {
            DbgPrint("Warning: Failed to write settings for croskeyboard4 (wilco)! Check that your permissions are correct!");
        }

        free(wilcoCfgs);
    }


    size_t cfgSize = offsetof(RemapCfgs, cfg) + sizeof(RemapCfg) * 40;

    if (offsetof(RemapCfgs, cfg) != 17) {
        DbgPrint("Warning: RemapCfgs prefix size is incorrect. Your settings file may not work in croskeyboard4!\n");
    }
    if (sizeof(RemapCfg) != 73) {
        DbgPrint("Warning: RemapCfg size is incorrect. Your settings file may not work in croskeyboard4!\n");
    }

    PRemapCfgs remapCfgs = (PRemapCfgs)malloc(cfgSize);
    RtlZeroMemory(remapCfgs, cfgSize);

    //Begin map vivalid keys (without Ctrl) to F# keys

    remapCfgs->magic = REMAP_CFG_MAGIC;
    remapCfgs->FlipSearchAndAssistantOnPixelbook = TRUE;
    remapCfgs->HasAssistantKey = RemapCfgOverrideAutoDetect;
    remapCfgs->IsNonChromeEC = RemapCfgOverrideAutoDetect;
    remapCfgs->remappings = 40;

    remapCfgs->cfg[6].Search = RemapCfgKeyStateEnforce;
    remapCfgs->cfg[6].originalKey.MakeCode = VIVALDI_BRIGHTNESSDN;
    remapCfgs->cfg[6].originalKey.Flags = KEY_E0;
    remapCfgs->cfg[6].remapVivaldiToFnKeys = TRUE;

    remapCfgs->cfg[7].Search = RemapCfgKeyStateEnforce;
    remapCfgs->cfg[7].originalKey.MakeCode = VIVALDI_BRIGHTNESSUP;
    remapCfgs->cfg[7].originalKey.Flags = KEY_E0;
    remapCfgs->cfg[7].remapVivaldiToFnKeys = TRUE;

    remapCfgs->cfg[32].LeftCtrl = RemapCfgKeyStateNoDetect;
    remapCfgs->cfg[32].LeftAlt = RemapCfgKeyStateEnforce;
    remapCfgs->cfg[32].originalKey.MakeCode = VIVALDI_BRIGHTNESSDN;
    remapCfgs->cfg[32].originalKey.Flags = KEY_E0;
    remapCfgs->cfg[32].remappedKey.MakeCode = VIVALDI_KBD_BKLIGHT_DOWN;
    remapCfgs->cfg[32].remappedKey.Flags = KEY_E0;

    remapCfgs->cfg[33].LeftCtrl = RemapCfgKeyStateNoDetect;
    remapCfgs->cfg[33].LeftAlt = RemapCfgKeyStateEnforce;
    remapCfgs->cfg[33].originalKey.MakeCode = VIVALDI_BRIGHTNESSUP;
    remapCfgs->cfg[33].originalKey.Flags = KEY_E0;
    remapCfgs->cfg[33].remappedKey.MakeCode = VIVALDI_KBD_BKLIGHT_UP;
    remapCfgs->cfg[33].remappedKey.Flags = KEY_E0;

    filterExt->remapCfgs = remapCfgs;

    DbgPrint("Initialized\n");

    FILE* dumpedSettingsFile;
    if (fopen_s(&dumpedSettingsFile, "croskbsettings.bin", "wb") == 0) {
        fwrite(remapCfgs, 1, cfgSize, dumpedSettingsFile);
        fclose(dumpedSettingsFile);

        DbgPrint("Wrote active settings to croskbsettings.bin!\n");
    }
    else {
        DbgPrint("Warning: Failed to write settings for croskeyboard4! Check that your permissions are correct!");
    }
}

#undef filterExt
#define devExt this

void VivaldiTester::updateKey(KeyStruct data) {
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (devExt->currentKeys[i].InternalFlags & INTFLAG_REMOVED) {
            RtlZeroMemory(&devExt->currentKeys[i], sizeof(devExt->currentKeys[0])); //Remove any keys marked to be removed
        }
    }

    KeyStruct origData = data;
    //Apply any remaps if they were done
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (devExt->remappedKeys[i].origKey.MakeCode == data.MakeCode &&
            devExt->remappedKeys[i].origKey.Flags == (data.Flags & KEY_TYPES)) {
            data.MakeCode = devExt->remappedKeys[i].remappedKey.MakeCode;
            data.Flags = devExt->remappedKeys[i].remappedKey.Flags | (data.Flags & ~KEY_TYPES);
            break;
        }
    }

    garbageCollect();

    data.Flags = data.Flags & (KEY_TYPES | KEY_BREAK);
    if (data.Flags & KEY_BREAK) { //remove
        data.Flags = data.Flags & KEY_TYPES;
        origData.Flags = origData.Flags & KEY_TYPES;
        if (devExt->lastKeyPressed.MakeCode == data.MakeCode &&
            devExt->lastKeyPressed.Flags == data.Flags) {
            RtlZeroMemory(&devExt->lastKeyPressed, sizeof(devExt->lastKeyPressed));
        }

        for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
            if (devExt->currentKeys[i].MakeCode == data.MakeCode &&
                devExt->currentKeys[i].Flags == data.Flags) {
                for (int j = 0; j < MAX_CURRENT_KEYS; j++) { //Remove any remaps if the original key is to be removed
                    if (devExt->remappedKeys[j].origKey.MakeCode == origData.MakeCode &&
                        devExt->remappedKeys[j].origKey.Flags == origData.Flags) {
                        RtlZeroMemory(&devExt->remappedKeys[j], sizeof(devExt->remappedKeys[0]));
                    }
                }

                devExt->currentKeys[i].InternalFlags |= INTFLAG_REMOVED;
            }
        }
    }
    else {
        for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
            if (devExt->currentKeys[i].Flags == 0x00 && devExt->currentKeys[i].MakeCode == 0x00) {
                devExt->currentKeys[i] = data;
                devExt->currentKeys[i].InternalFlags |= INTFLAG_NEW;
                devExt->numKeysPressed++;
                devExt->lastKeyPressed = data;
                break;
            }
            else if (devExt->currentKeys[i].Flags == data.Flags && devExt->currentKeys[i].MakeCode == data.MakeCode) {
                break;
            }
        }
    }
}

BOOLEAN VivaldiTester::addRemap(RemappedKeyStruct remap) {
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (devExt->remappedKeys[i].origKey.MakeCode == remap.origKey.MakeCode &&
            devExt->remappedKeys[i].origKey.Flags == remap.remappedKey.Flags) {
            if (memcmp(&devExt->remappedKeys[i], &remap, sizeof(remap)) == 0) {
                return TRUE; //already exists
            }
            else {
                return FALSE; //existing remap exists but not the same
            }
        }
    }

    garbageCollect();

    const RemappedKeyStruct emptyStruct = { 0 };
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (memcmp(&devExt->remappedKeys[i], &emptyStruct, sizeof(emptyStruct)) == 0) {
            devExt->remappedKeys[i] = remap;


            //Now apply remap
            for (int j = 0; j < MAX_CURRENT_KEYS; j++) {
                if (devExt->currentKeys[j].MakeCode == remap.origKey.MakeCode &&
                    devExt->currentKeys[j].Flags == remap.origKey.Flags) {
                    devExt->currentKeys[j].MakeCode = remap.remappedKey.MakeCode;
                    devExt->currentKeys[j].Flags = remap.remappedKey.Flags;
                    break;
                }
            }

            return TRUE;
        }
    }
    return FALSE; //no slot found
}

void VivaldiTester::garbageCollect() {
    //Clear out any empty remap slots
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        RemappedKeyStruct keyRemaps[MAX_CURRENT_KEYS] = { 0 };
        const RemappedKeyStruct emptyStruct = { 0 };
        int j = 0;
        for (int k = 0; k < MAX_CURRENT_KEYS; k++) {
            if (memcmp(&devExt->remappedKeys[k], &emptyStruct, sizeof(emptyStruct)) != 0) {
                keyRemaps[j] = devExt->remappedKeys[k];
                j++;
            }
        }
        devExt->numRemaps = j;
        RtlCopyMemory(&devExt->remappedKeys, keyRemaps, sizeof(keyRemaps));
    }

    //Clear out any empty key slots
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        KeyStruct keyCodes[MAX_CURRENT_KEYS] = { 0 };
        int j = 0;
        for (int k = 0; k < MAX_CURRENT_KEYS; k++) {
            if (devExt->currentKeys[k].Flags != 0 ||
                devExt->currentKeys[k].MakeCode != 0) {
                keyCodes[j] = devExt->currentKeys[k];
                j++;
            }
        }
        devExt->numKeysPressed = j;
        RtlCopyMemory(&devExt->currentKeys, keyCodes, sizeof(keyCodes));
    }
}

UINT8 MapHIDKeys(KEYBOARD_INPUT_DATA report[MAX_CURRENT_KEYS], int* reportSize) {
    UINT8 flag = 0;
    for (int i = 0; i < *reportSize; i++) {
        if ((report[i].Flags & KEY_TYPES) == KEY_E0) {
            switch (report->MakeCode) {
            case VIVALDI_BRIGHTNESSDN:
                if (!(report[i].Flags & KEY_BREAK))
                    flag |= CROSKBHID_BRIGHTNESS_DN;
                break;
            case VIVALDI_BRIGHTNESSUP:
                if (!(report[i].Flags & KEY_BREAK))
                    flag |= CROSKBHID_BRIGHTNESS_UP;
                break;
            case VIVALDI_KBD_BKLIGHT_DOWN:
                if (!(report[i].Flags & KEY_BREAK))
                    flag |= CROSKBHID_KBLT_DN;
                break;
            case VIVALDI_KBD_BKLIGHT_UP:
                if (!(report[i].Flags & KEY_BREAK))
                    flag |= CROSKBHID_KBLT_UP;
                break;
            case VIVALDI_KBD_BKLIGHT_TOGGLE:
                if (!(report[i].Flags & KEY_BREAK))
                    flag |= CROSKBHID_KBLT_TOGGLE;
                break;
            default:
                continue;
            }
            report[i].MakeCode = 0;
            report[i].Flags = 0;
        }
    }

    //GC the new Report
    KEYBOARD_INPUT_DATA newReport[MAX_CURRENT_KEYS];
    int newSize = 0;
    for (int i = 0; i < *reportSize; i++) {
        if (report[i].Flags != 0 || report[i].MakeCode != 0) {
            newReport[newSize] = report[i];
            newSize++;
        }
    }

    RtlCopyMemory(report, newReport, sizeof(newReport[0]) * newSize);
    *reportSize = newSize;

    return flag;
}

BOOLEAN VivaldiTester::checkKey(KEYBOARD_INPUT_DATA key, KeyStruct report[MAX_CURRENT_KEYS]) {
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (report[i].MakeCode == key.MakeCode &&
            report[i].Flags == (key.Flags & KEY_TYPES)) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOLEAN VivaldiTester::addKey(KEYBOARD_INPUT_DATA key, KEYBOARD_INPUT_DATA data[MAX_CURRENT_KEYS]) {
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (data[i].MakeCode == key.MakeCode &&
            data[i].Flags == (key.Flags & KEY_TYPES)) {
            return data[i].Flags == key.Flags; //If both contain the same bit value of BREAK, we're ok. Otherwise we're not
        }
        else if (data[i].MakeCode == 0 && data[i].Flags == 0) {
            data[i] = key;
            return TRUE;
        }
    }
    return FALSE;
}

BOOLEAN validateBool(RemapCfgKeyState keyState, BOOLEAN containerBOOL) {
    if (keyState == RemapCfgKeyStateNoDetect) {
        return TRUE;
    }

    if ((keyState == RemapCfgKeyStateEnforce && containerBOOL) ||
        (keyState == RemapCfgKeyStateEnforceNot && !containerBOOL)) {
        return TRUE;
    }

    return FALSE;
}

INT32 VivaldiTester::IdxOfFnKey(RemapCfgKey originalKey) {
    if (originalKey.Flags != KEY_E0) {
        return -1;
    }

    for (int i = 0; i < devExt->functionRowCount; i++) {
        if (devExt->functionRowKeys[i].MakeCode == originalKey.MakeCode) {
            return i;
        }
    }

    return -1;
}

void VivaldiTester::RemapLoaded(KEYBOARD_INPUT_DATA data[MAX_CURRENT_KEYS], KEYBOARD_INPUT_DATA dataBefore[MAX_CURRENT_KEYS], KEYBOARD_INPUT_DATA dataAfter[MAX_CURRENT_KEYS]) {
    if (!devExt->remapCfgs || devExt->remapCfgs->magic != REMAP_CFG_MAGIC)
        return;

    for (int i = 0; i < devExt->numKeysPressed; i++) {
        for (UINT32 j = 0; j < devExt->remapCfgs->remappings; j++) {
            RemapCfg cfg = devExt->remapCfgs->cfg[j];

            if (!validateBool(cfg.LeftCtrl, devExt->LeftCtrlPressed))
                continue;
            if (!validateBool(cfg.LeftAlt, devExt->LeftAltPressed))
                continue;
            if (!validateBool(cfg.LeftShift, devExt->LeftShiftPressed))
                continue;
            if (!validateBool(cfg.Assistant, devExt->AssistantPressed))
                continue;
            if (!validateBool(cfg.Search, devExt->SearchPressed))
                continue;
            if (!validateBool(cfg.RightCtrl, devExt->RightCtrlPressed))
                continue;
            if (!validateBool(cfg.RightAlt, devExt->RightAltPressed))
                continue;
            if (!validateBool(cfg.RightShift, devExt->RightShiftPressed))
                continue;

            if (data[i].MakeCode == cfg.originalKey.MakeCode &&
                (cfg.originalKey.Flags & KEY_TYPES) == (data[i].Flags & KEY_TYPES)) {

                RemappedKeyStruct remappedStruct = { 0 };
                remappedStruct.origKey.MakeCode = data[i].MakeCode;
                remappedStruct.origKey.Flags = data[i].Flags;

                INT32 fnKeyIdx = IdxOfFnKey(cfg.originalKey);
                if (cfg.remapVivaldiToFnKeys && fnKeyIdx != -1) {
                    remappedStruct.remappedKey.MakeCode = fnKeys_set1[fnKeyIdx];
                    remappedStruct.remappedKey.Flags = 0;
                    if (addRemap(remappedStruct)) {
                        data[i].Flags &= ~KEY_TYPES;
                        data[i].MakeCode = fnKeys_set1[fnKeyIdx];
                    }
                }
                else {
                    remappedStruct.remappedKey.MakeCode = cfg.remappedKey.MakeCode;
                    remappedStruct.remappedKey.Flags = (cfg.remappedKey.Flags & KEY_TYPES);
                    if (addRemap(remappedStruct)) {
                        data[i].Flags = (cfg.remappedKey.Flags & KEY_TYPES);
                        data[i].MakeCode = cfg.remappedKey.MakeCode;
                    }

                    for (int k = 0; k < sizeof(cfg.additionalKeys) / sizeof(cfg.additionalKeys[0]); k++) {
                        if ((cfg.additionalKeys[k].Flags & (KEY_TYPES | KEY_BREAK)) == 0 && cfg.additionalKeys[k].MakeCode == 0) {
                            break;
                        }

                        KEYBOARD_INPUT_DATA addData = { 0 };
                        addData.MakeCode = cfg.additionalKeys[k].MakeCode;
                        addData.Flags = cfg.additionalKeys[k].Flags & (KEY_TYPES | KEY_BREAK);
                        addKey(addData, dataBefore);

                        KEYBOARD_INPUT_DATA removeData = { 0 };
                        removeData.MakeCode = addData.MakeCode;
                        removeData.Flags = cfg.additionalKeys[k].Flags & KEY_TYPES;
                        if ((addData.Flags & KEY_BREAK) == 0) {
                            removeData.Flags |= KEY_BREAK;
                        }
                        addKey(removeData, dataAfter);
                    }
                }

                break;
            }
        }
    }
}

void VivaldiTester::ServiceCallback(PKEYBOARD_INPUT_DATA InputDataStart, PKEYBOARD_INPUT_DATA InputDataEnd, PULONG InputDataConsumed) {
    PKEYBOARD_INPUT_DATA pData;
    for (pData = InputDataStart; pData != InputDataEnd; pData++) { //First loop -> Refresh Modifier Keys and Change Legacy Keys to vivaldi bindings
        if ((pData->Flags & KEY_TYPES) == 0) {
            switch (pData->MakeCode)
            {
            case K_LCTRL: //L CTRL
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->LeftCtrlPressed = TRUE;
                }
                else {
                    devExt->LeftCtrlPressed = FALSE;
                }
                break;
            case K_LALT: //L Alt
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->LeftAltPressed = TRUE;
                }
                else {
                    devExt->LeftAltPressed = FALSE;
                }
                break;
            case K_LSHFT: //L Shift
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->LeftShiftPressed = TRUE;
                }
                else {
                    devExt->LeftShiftPressed = FALSE;
                }
                break;
            case K_RSHFT: //R Shift
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->RightShiftPressed = TRUE;
                }
                else {
                    devExt->RightShiftPressed = FALSE;
                }
                break;
            default:
                for (int i = 0; i < sizeof(devExt->legacyTopRowKeys); i++) {
                    if (pData->MakeCode == devExt->legacyTopRowKeys[i]) {
                        pData->MakeCode = devExt->legacyVivaldi[i];
                        pData->Flags |= KEY_E0; //All legacy vivaldi upgrades use E0 modifier
                    }
                }

                break;
            }
        }
        if ((pData->Flags & KEY_TYPES) == KEY_E0) {
            switch (pData->MakeCode)
            {
            case K_ASSISTANT: //Assistant Key
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->AssistantPressed = TRUE;
                }
                else {
                    devExt->AssistantPressed = FALSE;
                }
                break;
            case K_LWIN: //Search Key
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->SearchPressed = TRUE;
                }
                else {
                    devExt->SearchPressed = FALSE;
                }
                break;
            case K_RCTRL: //R CTRL
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->RightCtrlPressed = TRUE;
                }
                else {
                    devExt->RightCtrlPressed = FALSE;
                }
                break;
            case K_RALT: //R Alt
                if ((pData->Flags & KEY_BREAK) == 0) {
                    devExt->RightAltPressed = TRUE;
                }
                else {
                    devExt->RightAltPressed = FALSE;
                }
                break;

            }
        }
    }

    {
        //Now make the data HID-like for easier handling
        ULONG i = 0;
        for (i = 0; i < (InputDataEnd - InputDataStart); i++) {
            KeyStruct key = { 0 };
            key.MakeCode = InputDataStart[i].MakeCode;
            key.Flags = InputDataStart[i].Flags;
            updateKey(key);
        }
        *InputDataConsumed = i;
    }

    KEYBOARD_INPUT_DATA newReport[MAX_CURRENT_KEYS] = { 0 };
    //Add new keys
    for (int i = 0, j = 0; i < devExt->numKeysPressed; i++) { //Prepare new report for remapper to sort through
        if (devExt->currentKeys[i].InternalFlags & INTFLAG_NEW) {
            newReport[j].MakeCode = devExt->currentKeys[i].MakeCode;
            newReport[j].Flags = devExt->currentKeys[i].Flags;
            devExt->currentKeys[i].InternalFlags &= ~INTFLAG_NEW;
            j++;
        }
    }

    KEYBOARD_INPUT_DATA preReport[MAX_CURRENT_KEYS] = { 0 };
    KEYBOARD_INPUT_DATA postReport[MAX_CURRENT_KEYS] = { 0 };

    //Do whichever remap was chosen
    //RemapPassthrough(newReport, preReport, postReport);
    //RemapLegacy(newReport, preReport, postReport);
    RemapLoaded(newReport, preReport, postReport);

    //Remove any empty keys
    int newReportKeysPresent = 0;
    for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
        if (newReport[i].Flags != 0 ||
            newReport[i].MakeCode != 0) {
            newReport[newReportKeysPresent] = newReport[i];
            newReportKeysPresent++;
        }
    }

    for (int i = newReportKeysPresent; i < MAX_CURRENT_KEYS; i++) {
        RtlZeroMemory(&newReport[i], sizeof(newReport[i]));
    }

    //Now add all the removed keys
    int reportSize = newReportKeysPresent;
    for (int i = 0; i < devExt->numKeysPressed; i++) { //Prepare new report for remapper to sort through
        if (devExt->currentKeys[i].InternalFlags & INTFLAG_REMOVED) {
            newReport[reportSize].MakeCode = devExt->currentKeys[i].MakeCode;
            newReport[reportSize].Flags = devExt->currentKeys[i].Flags | KEY_BREAK;
            reportSize++;
        }
    }

    //If empty report keys, add the last key (if present)
    if (reportSize == 0 && (devExt->lastKeyPressed.MakeCode != 0 || devExt->lastKeyPressed.Flags != 0)) {
        newReport[reportSize].MakeCode = devExt->lastKeyPressed.MakeCode;
        newReport[reportSize].Flags = devExt->lastKeyPressed.Flags;

        for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
            if (devExt->remappedKeys[i].origKey.MakeCode == devExt->lastKeyPressed.MakeCode &&
                devExt->remappedKeys[i].origKey.Flags == (devExt->lastKeyPressed.Flags & KEY_TYPES)) {
                newReport[reportSize].MakeCode = devExt->remappedKeys[i].remappedKey.MakeCode;
                newReport[reportSize].Flags = devExt->remappedKeys[i].remappedKey.Flags | (newReport[reportSize].Flags & ~KEY_TYPES);
                break;
            }
        }

        reportSize++;
    }

    //Now prepare the report
    for (int i = 0; i < reportSize; i++) {
        newReport[i].UnitId = InputDataStart[0].UnitId;

        //Always override Vivaldi Play/Pause to Windows native equivalent
        if (newReport[i].MakeCode == VIVALDI_PLAYPAUSE &&
            (pData->Flags & (KEY_E0 | KEY_E1)) == KEY_E0) {
            pData->MakeCode = 0x22; //Windows native Play / Pause Code
        }
    }

    UINT8 HIDFlag = MapHIDKeys(newReport, &reportSize);

    ULONG DataConsumed;
    DbgPrint("\tLegacy Keys\n");
    if (InputDataEnd > InputDataStart) {
        ReceiveKeys_Guarded(InputDataStart, InputDataEnd, &DataConsumed);
    }

    DbgPrint("\tHID Consumer Keys: 0x%x\n", HIDFlag);

    DbgPrint("\tHID translated Keys\n");

    {
        int preReportSize = 0;
        for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
            if (preReport[i].Flags != 0 || preReport[i].MakeCode != 0) {
                preReportSize++;
            }
        }

        if (preReportSize > 0) {
            ReceiveKeys_Guarded(preReport, preReport + preReportSize, &DataConsumed);
        }
    }

    if (reportSize > 0) {
        ReceiveKeys_Guarded(newReport, newReport + reportSize, &DataConsumed);
    }

    {
        int postReportSize = 0;
        for (int i = 0; i < MAX_CURRENT_KEYS; i++) {
            if (postReport[i].Flags != 0 || postReport[i].MakeCode != 0) {
                postReportSize++;
            }
        }

        if (postReportSize > 0) {
            ReceiveKeys_Guarded(postReport, postReport + postReportSize, &DataConsumed);
        }
    }
}

int CompareKeys(const void* raw1, const void* raw2) {
    PKEYBOARD_INPUT_DATA data1 = (PKEYBOARD_INPUT_DATA)raw1;
    PKEYBOARD_INPUT_DATA data2 = (PKEYBOARD_INPUT_DATA)raw2;
    return ((data1->MakeCode - data2->MakeCode) << 4) +
        ((data2->Flags & KEY_TYPES - (data1->Flags & KEY_TYPES)));
}

void ReceiveKeys_Guarded(PKEYBOARD_INPUT_DATA startPtr, PKEYBOARD_INPUT_DATA endPtr, PULONG InputDataConsumed) {
    qsort(startPtr, endPtr - startPtr, sizeof(*startPtr), CompareKeys);

    ULONG consumedCount = 0;

    printf("\t\t==Frame Start==\n");
    for (ULONG i = 0; i < endPtr - startPtr; i++) {
        printf("\t\t\tKey %d: Code 0x%x, Flags 0x%x\n", i, startPtr[i].MakeCode, startPtr[i].Flags);
        consumedCount++;
    }
    printf("\t\t==Frame End==\n");

    for (ULONG i = 0; i < (endPtr - 1) - startPtr; i++) {
        assert(startPtr[i].MakeCode != startPtr[i + 1].MakeCode ||
            (startPtr[i].Flags & KEY_TYPES) != (startPtr[i + 1].Flags & KEY_TYPES));
    }

    *InputDataConsumed = consumedCount;
}

void SubmitKeys_Guarded(VivaldiTester* test, PKEYBOARD_INPUT_DATA start, UINT32 count) {
    PKEYBOARD_INPUT_DATA newData = (PKEYBOARD_INPUT_DATA)malloc(sizeof(KEYBOARD_INPUT_DATA) * count);
    assert(newData != 0);

    RtlCopyMemory(newData, start, sizeof(KEYBOARD_INPUT_DATA) * count);

    ULONG consumedCount = 0;
    test->ServiceCallback(newData, newData + count, &consumedCount);

    assert(consumedCount == count); //Weird breakage can happen if this isn't equal
    free(newData);
}

int main()
{
    VivaldiTester test;

    KEYBOARD_INPUT_DATA testData[2];
    RtlZeroMemory(testData, sizeof(testData)); //Reset test data

    /*testData[0].MakeCode = K_LCTRL;
    printf("Ctrl\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("Ctrl Repeat\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = VIVALDI_MUTE;
    testData[0].Flags = KEY_E0;
    printf("Mute\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].Flags |= KEY_BREAK;
    printf("Mute Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LCTRL;
    testData[0].Flags = 0;
    printf("Ctrl Repeat\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].Flags |= KEY_BREAK;
    printf("Ctrl Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = VIVALDI_MUTE;
    testData[0].Flags = KEY_E0;
    printf("Mute\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].Flags |= KEY_BREAK;
    printf("Mute Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    RtlZeroMemory(testData, sizeof(testData)); //Reset test data

    testData[0].MakeCode = 0x1E;
    testData[0].Flags = 0;
    printf("A Press\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = 0x1F;
    testData[0].Flags = 0;
    printf("S Press + A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("S + A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("S + A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("S + A Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = 0x1E;
    testData[0].Flags = KEY_BREAK;
    printf("A Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = 0x1F;
    testData[0].Flags = 0;
    printf("S Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    printf("S Hold\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = 0x1F;
    testData[0].Flags = KEY_BREAK;
    printf("S Release\n");
    SubmitKeys_Guarded(&test, testData, 1);

    RtlZeroMemory(testData, sizeof(testData)); //Reset test data

    testData[0].MakeCode = K_LCTRL;
    printf("Ctrl\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LALT;
    printf("Alt\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_BACKSP;
    printf("Backspace\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LCTRL;
    testData[0].Flags = KEY_BREAK;
    printf("Release Ctrl\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LALT;
    testData[0].Flags = KEY_BREAK;
    printf("Release Alt\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_BACKSP;
    testData[0].Flags = KEY_BREAK;
    printf("Release Backspace\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_BACKSP;
    testData[0].Flags = 0;
    printf("Backspace\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_BACKSP;
    testData[0].Flags = KEY_BREAK;
    printf("Release Backspace\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LCTRL;
    testData[0].Flags = 0;
    printf("Ctrl\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = VIVALDI_BRIGHTNESSUP;
    testData[0].Flags = KEY_E0;
    printf("Brightness Up\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = VIVALDI_BRIGHTNESSUP;
    testData[0].Flags = KEY_E0 | KEY_BREAK;
    printf("Release Brightness Up\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = K_LCTRL;
    testData[0].Flags = KEY_BREAK;
    printf("Release Ctrl\n");
    SubmitKeys_Guarded(&test, testData, 1);*/

    testData[0].MakeCode = VIVALDI_VOLUP;
    testData[0].Flags = KEY_E0;
    printf("Volume Up\n");
    SubmitKeys_Guarded(&test, testData, 1);

    testData[0].MakeCode = VIVALDI_VOLUP;
    testData[0].Flags = KEY_E0 | KEY_BREAK;
    printf("Release Volume Up\n");
    SubmitKeys_Guarded(&test, testData, 1);
}