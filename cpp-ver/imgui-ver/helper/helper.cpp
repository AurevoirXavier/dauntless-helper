#include <helper.h>

#include <utility>

extern HWND dauntlessWindow;
extern DWORD64 processAddress;

extern Helper helper;

extern bool g_initialised;
extern bool g_activeMenu;
extern bool g_showMenu;

bool lButtonDown, rButtonDown, inHunt = false;
bool startHunt = false;
int huntingTimer = 0;

PDWORD64 readFromMemSecurity(DWORD64 baseAddress, const std::vector<DWORD64> &offsets) {
    auto ptr = (PDWORD64) baseAddress;
    for (auto &offset: offsets) {
        if (IsBadCodePtr((FARPROC) ptr)) return nullptr;
        ptr = (PDWORD64) (*ptr + offset);
    }

    if (IsBadCodePtr((FARPROC) ptr)) return nullptr;
    return ptr;
}

void autoClick() {
    if (lButtonDown) {
        PostMessage(dauntlessWindow, WM_LBUTTONDOWN, NULL, helper.cursor);
        PostMessage(dauntlessWindow, WM_LBUTTONUP, NULL, helper.cursor);
    }

    if (rButtonDown) {
        PostMessage(dauntlessWindow, WM_RBUTTONDOWN, NULL, helper.cursor);
        PostMessage(dauntlessWindow, WM_RBUTTONUP, NULL, helper.cursor);
    }

    if (lButtonDown || rButtonDown) helper.player.coordinate.lock(); else helper.player.coordinate.isLocked = false;
}

DWORD WINAPI TeleportBossToPlayer(LPVOID) {
    float x = helper.player.coordinate.x + 500;
    float y = helper.player.coordinate.y;
    float z = helper.player.coordinate.z;

    while (helper.huntingFeatures[2].status) helper.boss.coordinate.lockTo(x, y, z);

    helper.boss.coordinate.isLocked = false;

    return NULL;
}

void teleportBossToPlayer() {
    if (helper.boss.coordinate.isLocked) return;

    CreateThread(nullptr, NULL, TeleportBossToPlayer, nullptr, NULL, nullptr);

    Sleep(100);
}

Attribute::Attribute() {
    this->status = false;
    this->previousStatus = false;

    this->baseAddress = NULL;
    this->pointer = nullptr;
    this->value = 0;
    this->defaultValue = 0;
}

Attribute::Attribute(DWORD64 baseAddress, std::vector<DWORD64> offsets, float defaultValue) {
    this->status = false;
    this->previousStatus = false;

    this->baseAddress = baseAddress;
    this->offsets = std::move(offsets);
    this->pointer = nullptr;
    this->value = defaultValue;
    this->defaultValue = defaultValue;
}

Coordinate::Coordinate() {
    this->isLocked = false;

    this->baseAddress = NULL;
    this->pX = nullptr;
    this->pY = nullptr;
    this->pZ = nullptr;

    this->x = 0;
    this->lockX = 0;
    this->y = 0;
    this->lockY = 0;
    this->z = 0;
    this->lockZ = 0;
}

Coordinate::Coordinate(DWORD64 baseAddress, std::vector<DWORD64> offsets) {
    this->isLocked = false;

    this->baseAddress = baseAddress;
    this->offsets = std::move(offsets);
    this->pX = nullptr;
    this->pY = nullptr;
    this->pZ = nullptr;

    this->x = 0;
    this->lockX = 0;
    this->y = 0;
    this->lockY = 0;
    this->z = 0;
    this->lockZ = 0;
}

void Coordinate::lock() {
    this->isLocked = true;

    *this->pX = this->lockX;
    *this->pY = this->lockY;
    *this->pZ = this->lockZ;
}

void Coordinate::lockTo(float x, float y, float z) {
    this->isLocked = true;

    *this->pX = x;
    *this->pY = y;
    *this->pZ = z;
}

void Coordinate::update() {
    if (this->isLocked) return;

    if (inHunt) {
        this->x = *this->pX;
        this->y = *this->pY;
        this->z = *this->pZ;

        if (!lButtonDown && !rButtonDown) {
            this->lockX = this->x;
            this->lockY = this->y;
            this->lockZ = this->z;
        }
    } else {
        PDWORD64 pCoordinate = readFromMemSecurity(this->baseAddress, this->offsets);
        if (!pCoordinate) return;

        this->pX = (PFLOAT) (*pCoordinate + 0x190);
        this->pY = (PFLOAT) (*pCoordinate + 0x194);
        this->pZ = (PFLOAT) (*pCoordinate + 0x198);

        if (!IsBadCodePtr((FARPROC) this->pX)) this->x = *this->pX;
        if (!IsBadCodePtr((FARPROC) this->pY)) this->y = *this->pY;
        if (!IsBadCodePtr((FARPROC) this->pZ)) this->z = *this->pZ;
    }
}

void Coordinate::initInHunt() {
    PDWORD64 pCoordinate = readFromMemSecurity(this->baseAddress, this->offsets);

    this->pX = (PFLOAT) (*pCoordinate + 0x190);
    this->pY = (PFLOAT) (*pCoordinate + 0x194);
    this->pZ = (PFLOAT) (*pCoordinate + 0x198);
}

void Coordinate::clearData() {
    this->isLocked = false;

    this->x = 0;
    this->y = 0;
    this->z = 0;
}

Boss::Boss() {
    // 0 hp
    this->hp = Attribute(processAddress + 0x4091010, std::initializer_list<DWORD64>{0x148, 0x440, 0, 0x758, 0x190, 0, 0x30}, 0.f);

    this->coordinate = Coordinate(processAddress + 0x4091010, std::initializer_list<DWORD64>{0x148, 0x440, 0, 0x158});
}

DWORD WINAPI Boss::monitoring(LPVOID) {
    while (g_initialised) {
        helper.boss.hp.pointer = (PFLOAT) readFromMemSecurity(helper.boss.hp.baseAddress, helper.boss.hp.offsets);
        if (helper.boss.hp.pointer) helper.boss.hp.value = *helper.boss.hp.pointer;

        if (!helper.boss.hp.pointer || helper.boss.hp.value == 0) {
            inHunt = false;
            startHunt = true;
            lButtonDown = false;
            rButtonDown = false;

            helper.player.coordinate.isLocked = false;

            helper.huntingFeatures[2].status = false;
            helper.boss.coordinate.clearData();
        } else inHunt = true;

        Sleep(1);
    }

    return NULL;
}

void Boss::initInHunt() {
    this->coordinate.initInHunt();
}

Player::Player() {
    // 0 attack speed
    // 1 movement speed
    // 2 instant movement
    // 3 jump height
    // 4 stamina
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x758, 0x190, 0x8, 0xD4}, 10.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x758, 0x190, 0x10, 0x30}, 4.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x398, 0x214}, 100000.f);
    this->attributes.emplace_back(processAddress + 0x402A2C8, std::initializer_list<DWORD64>{0x18, 0x240, 0x398, 0x1AC}, 2000.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x758, 0x190, 0, 0xB4}, 100.f);

    this->coordinate = Coordinate(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x48, 0x8, 0xC8});
}

void Player::updateAttribute() {
    if (inHunt) {
        for (Attribute &attribute : this->attributes) {
            if (attribute.status) {
                attribute.previousStatus = true;
                *attribute.pointer = attribute.value;
            } else if (attribute.previousStatus) {
                attribute.previousStatus = false;
                if (attribute.status) *attribute.pointer = attribute.defaultValue;
            }
        }
    } else {
        for (Attribute &attribute : this->attributes) {
            attribute.pointer = (PFLOAT) readFromMemSecurity(attribute.baseAddress, attribute.offsets);
            if (!attribute.pointer) continue;

            if (attribute.status) {
                attribute.previousStatus = true;
                *attribute.pointer = attribute.value;
            } else if (attribute.previousStatus) {
                attribute.previousStatus = false;
                if (attribute.status) *attribute.pointer = attribute.defaultValue;
            }
        }
    }
}

void Player::initInHunt() {
    for (Attribute &attribute : this->attributes) attribute.pointer = (PFLOAT) readFromMemSecurity(attribute.baseAddress, attribute.offsets);

    this->coordinate.initInHunt();
}

Feature::Feature(void (*fn)()) {
    this->status = false;

    this->fn = fn;
}

Helper::Helper() {
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(dauntlessWindow, &p);
    this->cursor = MAKELONG(p.x, p.y);

    this->boss = Boss();
    this->player = Player();

    // 0 auto click
    // 1 lock player position on auto lock
    // 2 summon boss
    this->huntingFeatures.emplace_back(autoClick);
    this->huntingFeatures.emplace_back(nullptr);
    this->huntingFeatures.emplace_back(teleportBossToPlayer);
}

DWORD WINAPI Helper::run(LPVOID) {
    CreateThread(nullptr, NULL, Boss::monitoring, nullptr, NULL, nullptr);

    while (g_initialised) {
        helper.player.updateAttribute();
        helper.player.coordinate.update();

        Sleep(16);
        if (!inHunt) {
            Sleep(1000);
            continue;
        }

        if (startHunt) {
            startHunt = false;
            Sleep(18000);

            helper.player.initInHunt();
            helper.boss.initInHunt();
        }

        helper.boss.coordinate.update();

        if (dauntlessWindow == GetForegroundWindow()) {
            lButtonDown = GetKeyState(VK_LBUTTON) < 0;
            rButtonDown = GetKeyState(VK_RBUTTON) < 0;

            if (GetAsyncKeyState(VK_F2) & 1) helper.huntingFeatures[2].status = !helper.huntingFeatures[2].status;
        }

        for (Feature &feature : helper.huntingFeatures) if (feature.status && feature.fn) feature.fn();
    }

    return NULL;
}

void Helper::draw() {
//    ImGui::ShowDemoWindow(&g_showMenu);

    ImGui::Begin("Dauntless Helper");

    ImGui::Checkbox("Active menu", &g_activeMenu);

    if (ImGui::CollapsingHeader("Info")) {
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("Author: Xavier Lau - c.estlavie@icloud.com");
        ImGui::Text("Github: github.com/AurevoirXavier/dauntless-helper");
    }

    if (ImGui::CollapsingHeader("Hunting features")) {
        ImGui::Checkbox("Auto click", &helper.huntingFeatures[0].status);
        ImGui::Checkbox("Lock player position on auto click", &helper.huntingFeatures[1].status);
        ImGui::Checkbox("Summon Boss", &helper.huntingFeatures[2].status);
    }

    if (ImGui::CollapsingHeader("Player Attributes")) {
        ImGui::Checkbox("Modify attack speed", &helper.player.attributes[0].status);
        ImGui::SliderFloat("attack speed", &helper.player.attributes[0].value, 1, 1000);
        ImGui::Checkbox("Modify movement speed", &helper.player.attributes[1].status);
        ImGui::SliderFloat("Movement speed", &helper.player.attributes[1].value, 1, 1000);
        ImGui::Checkbox("Enable instant movement", &(bool &) helper.player.attributes[2].status);
        ImGui::SliderFloat("Instant movement", &helper.player.attributes[2].value, 500, 100000);
        ImGui::Checkbox("Modify jump height", &(bool &) helper.player.attributes[3].status);
        ImGui::SliderFloat("Jump height", &helper.player.attributes[3].value, 980, 100000);
        ImGui::Checkbox("Infinity stamina", &(bool &) helper.player.attributes[4].status);
    }

    if (ImGui::CollapsingHeader("Status")) {
        if (ImGui::TreeNode("Player")) {
            ImGui::Text("x %.2f | y %.2f | z %.2f", helper.player.coordinate.x, helper.player.coordinate.y, helper.player.coordinate.z);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Boss")) {
            ImGui::Text("Hp: %.2f", helper.boss.hp.value);
            ImGui::Text("x %.2f | y %.2f | z %.2f", helper.boss.coordinate.x, helper.boss.coordinate.y, helper.boss.coordinate.z);
            ImGui::TreePop();
        }
    }

    ImGui::End();
}
