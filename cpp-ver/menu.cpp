#include "menu.h"

HWND dauntlessWindow = FindWindow(nullptr, "Dauntless  ");
Menu menu;

bool init = true;
bool lButtonDown, rButtonDown, startTimer;
int duration = 0;

DWORD timer(LPVOID) {
    while (menu.boss.hp != 0) {
        Sleep(1000);
        duration += 1;
    }

    return NULL;
}

PDWORD64 readFromMem(DWORD64 baseAddress, const vector<DWORD64> &offsets) {
    auto ptr = (PDWORD64) baseAddress;
    for (auto &offset: offsets) ptr = (PDWORD64) (*ptr + offset);

    return ptr;
}

PDWORD64 readFromMemSecurity(DWORD64 baseAddress, const vector<DWORD64> &offsets) {
    auto ptr = (PDWORD64) baseAddress;
    for (auto &offset: offsets) {
        if (IsBadReadPtr((PVOID) ptr, 8)) return nullptr;
        ptr = (PDWORD64) (*ptr + offset);
    }

    return ptr;
}

void move(PFLOAT from, float to, PFLOAT pZ, float speed) {
    float fromStamp = *from;
    int d = int(to - fromStamp);
    float offset = d < 0 ? speed : -speed;
    float z = pZ ? *pZ : 0;
    int range = int(speed * 2);

    while (abs(d) > range) {
        d = int(to - fromStamp);
        fromStamp -= offset;
        *from = fromStamp;
        if (z != 0) *pZ = z;
        Sleep(1);
    }
}

Player::Player() {
    this->pAttackSpeed = nullptr;
    this->pMovementSpeed = nullptr;
    this->pInstantMovement = nullptr;
    this->pStamina = nullptr;

    this->pX = nullptr;
    this->x = 0;
    this->pY = nullptr;
    this->y = 0;
    this->pZ = nullptr;
    this->z = 0;
}

bool Player::updateCoordinate() {
    PDWORD64 pPlayerCoordinate = readFromMemSecurity(menu.processAddress + 0x3E323B0, {0x10, 0x48, 0x8, 0xC8});
    if (!pPlayerCoordinate) return false;

    this->pX = (PFLOAT) (*pPlayerCoordinate + 0x190);
    this->x = *this->pX;
    this->pY = (PFLOAT) (*pPlayerCoordinate + 0x194);
    this->y = *this->pY;
    this->pZ = (PFLOAT) (*pPlayerCoordinate + 0x198);
    this->z = *this->pZ;

    return true;
}

bool Player::updateAttribute() {
    PDWORD64 pPlayerAttribute = readFromMemSecurity(menu.processAddress + 0x3E323B0, {0x10, 0x758, 0x190});
    if (!pPlayerAttribute) return false;

    this->pAttackSpeed = (PFLOAT) readFromMem(*pPlayerAttribute + 0x8, {0xD4});
    this->pMovementSpeed = (PFLOAT) readFromMem(*pPlayerAttribute + 0x10, {0x30});
    this->pInstantMovement = (PFLOAT) readFromMem(menu.processAddress + 0x3E323B0, {0x10, 0x398, 0x214});
    this->pStamina = (PFLOAT) readFromMem(*pPlayerAttribute + 0, {0xB4});

    return true;
}

void Player::lockPosition() {
    *this->pX = this->x;
    *this->pY = this->y;
    *this->pZ = this->z;
}

DWORD Player::teleportToBoss(LPVOID) {
    float dZ = *menu.player.pZ - *menu.boss.pZ;

    if (dZ < 0) move(menu.player.pZ, *menu.boss.pZ + 100, nullptr, 15);
    move(menu.player.pY, *menu.boss.pY, menu.player.pZ, 15);
    move(menu.player.pX, *menu.boss.pZ, menu.player.pZ, 15);
    if (dZ > 2000) move(menu.player.pZ, *menu.boss.pZ + 100, nullptr, 15);

    return NULL;
}

Boss::Boss() {
    this->pHp = nullptr;
    this->hp = 0;

    this->pX = nullptr;
    this->pY = nullptr;
    this->pZ = nullptr;
}

void Boss::updateAttribute() {
    this->pHp = (PFLOAT) readFromMemSecurity(menu.processAddress + 0x4091010, {0x148, 0x440, 0, 0x758, 0x190, 0, 0x30});
    this->hp = this->pHp ? *pHp : 0;
}

void Boss::updateCoordinate() {
    PDWORD64 pBossCoordinate = readFromMem(menu.processAddress + 0x4091010, {0x148, 0x440, 0, 0x3A0});

    this->pX = (PFLOAT) (*pBossCoordinate + 0x190);
    this->pY = (PFLOAT) (*pBossCoordinate + 0x194);
    this->pZ = (PFLOAT) (*pBossCoordinate + 0x198);
}

DWORD Boss::teleportToPlayer(LPVOID) {
    float x = *menu.player.pX;
    float y = *menu.player.pY + 100;
    float z = *menu.player.pZ;

    while (menu.boss.positionLocked) {
        *menu.boss.pX = x;
        *menu.boss.pY = y;
        *menu.boss.pZ = z;
    }

    return NULL;
}

void autoClick(Player *player, bool status) {
    if (status) {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(dauntlessWindow, &p);
        LPARAM lParam = MAKELONG(p.x, p.y);

        if (lButtonDown) {
            PostMessage(dauntlessWindow, WM_LBUTTONDOWN, NULL, lParam);
            PostMessage(dauntlessWindow, WM_LBUTTONUP, NULL, lParam);
        }

        if (rButtonDown) {
            PostMessage(dauntlessWindow, WM_RBUTTONDOWN, NULL, lParam);
            PostMessage(dauntlessWindow, WM_RBUTTONUP, NULL, lParam);
        }

        if (lButtonDown || rButtonDown) (*player).lockPosition();
    }
}

void speedUpAttack(Player *player, bool status) { if (status) *(*player).pAttackSpeed = 10; else *(*player).pAttackSpeed = 1; }

void speedUpMovement(Player *player, bool status) { if (status) *(*player).pMovementSpeed = 4; else *(*player).pMovementSpeed = 1; }

void instantMovement(Player *player, bool status) { if (status) *(*player).pInstantMovement = 100000; else *(*player).pInstantMovement = 768; }

void infinityStamina(Player *player, bool status) { if (status) *(*player).pStamina = 100; }

Option::Option(LPCWSTR text, hack feature) {
    this->status = false;
    this->prevStatus = false;
    this->text = text;
    this->feature = feature;
}

Menu::Menu() {
    this->visible = true;

    RECT rect;
    GetWindowRect(dauntlessWindow, &rect);
    this->y = (float) (rect.bottom - rect.top) / 2;
    this->present = nullptr;
    this->processAddress = NULL;

    this->pContext = nullptr;
    this->pTargetView = nullptr;
    this->pFontWrapper = nullptr;

    this->selectedOption = 0;

    this->boss = Boss();
    this->player = Player();
}

Menu::~Menu() {
    for (auto &option : this->options) option.status = false;

    this->player.~Player();
    this->boss.~Boss();
}

void Menu::draw() {
    if (GetAsyncKeyState(VK_INSERT) & 1) this->visible = !this->visible;
    if (!this->visible) return;

    if (GetAsyncKeyState(VK_UP) & 1) this->selectedOption == 0 ? (this->selectedOption = (uint8_t) this->options.size() - 1) : (this->selectedOption -= 1);
    if (GetAsyncKeyState(VK_DOWN) & 1) this->selectedOption == this->options.size() - 1 ? (this->selectedOption = 0) : (this->selectedOption += 1);
    if (GetAsyncKeyState(VK_LEFT) & 1) this->options[this->selectedOption].status = false;
    if (GetAsyncKeyState(VK_RIGHT) & 1) this->options[this->selectedOption].status = true;

    this->pContext->OMSetRenderTargets(1, &this->pTargetView, nullptr);

    wstringstream wss;
    wss << L"Boss Hp: " << menu.boss.hp << L" Duration: " << duration;
    this->pFontWrapper->DrawString(this->pContext, wss.str().c_str(), 24, 10, this->y, 0xFFFFFFFF, FW1_RESTORESTATE);

    int index = 0;
    float x = 10;
    float y = this->y + 30;
    for (auto &option : this->options) {
        DWORD color;
        LPCWSTR status;

        if (option.status) {
            color = 0xFFFFFFFF;
            status = L" [On]";
        } else {
            color = 0x66666666;
            status = L" [Off]";
        }

        if (this->selectedOption == index) color = 0xFFFF6666;

        wstringstream wss;
        wss << option.text << status;

        this->pFontWrapper->DrawString(this->pContext, wss.str().c_str(), 24, x, y, color, FW1_RESTORESTATE);

        index += 1;
        y += 30;
    }
}

DWORD Menu::run(LPVOID) {
    while (!init) {
        menu.boss.updateAttribute();

        if (menu.boss.hp == 0) {
            startTimer = true;
            Sleep(1000);
            continue;
        }

        if (startTimer) {
            startTimer = false;
            duration = 0;
            CreateThread(nullptr, NULL, timer, nullptr, NULL, nullptr);
        }
        if (!menu.player.updateAttribute()) continue;
        if (GetForegroundWindow() != dauntlessWindow) {
            Sleep(1000);
            continue;
        }

        lButtonDown = GetKeyState(VK_LBUTTON) < 0;
        rButtonDown = GetKeyState(VK_RBUTTON) < 0;

        if (!lButtonDown && !rButtonDown) {
            if (menu.player.updateCoordinate()) {
                menu.boss.updateCoordinate();

                if (GetAsyncKeyState(VK_HOME) & 1) CreateThread(nullptr, NULL, Player::teleportToBoss, nullptr, NULL, nullptr);
                if (GetAsyncKeyState(VK_DELETE) & 1) {
                    menu.boss.positionLocked = !menu.boss.positionLocked;
                    if (menu.boss.positionLocked) CreateThread(nullptr, NULL, Boss::teleportToPlayer, nullptr, NULL, nullptr);
                }
            }
        }

        for (auto &option : menu.options) {
            if (option.status) {
                option.prevStatus = true;
                option.feature(&menu.player, true);
            } else {
                if (option.prevStatus) {
                    option.prevStatus = false;
                    option.feature(&menu.player, false);
                }
            }
        }

        Sleep(1);
    }

    return NULL;
}
