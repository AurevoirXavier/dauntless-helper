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

//PDWORD64 readFromMem(DWORD64 baseAddress, const std::vector<DWORD64> &offsets) {
//    auto ptr = (PDWORD64) baseAddress;
//    for (auto &offset: offsets) ptr = (PDWORD64) (*ptr + offset);
//
//    return ptr;
//}

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
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(dauntlessWindow, &p);
    LPARAM lParam = MAKELONG(p.x, p.y);

    if (lButtonDown) {
        SendMessage(dauntlessWindow, WM_LBUTTONDOWN, NULL, lParam);
        SendMessage(dauntlessWindow, WM_LBUTTONUP, NULL, lParam);
    }

    if (rButtonDown) {
        SendMessage(dauntlessWindow, WM_RBUTTONDOWN, NULL, lParam);
        SendMessage(dauntlessWindow, WM_RBUTTONUP, NULL, lParam);
    }

    if (helper.huntingFeatures[1].status && (lButtonDown || rButtonDown)) helper.player.coordinate.lock();
}

DWORD WINAPI TeleportBossToPlayer(LPVOID) {
    float x = helper.player.coordinate.x;
    float y = helper.player.coordinate.y + 500;
    float z = helper.player.coordinate.z;

    while (inHunt && helper.huntingFeatures[2].status) {
        helper.boss.coordinate.lockTo(x, y, z);
        Sleep(1);
    }

    helper.boss.positionLocked = false;

    return NULL;
}

void teleportBossToPlayer() {
    if (helper.boss.positionLocked) return;
    helper.boss.positionLocked = true;

    CreateThread(nullptr, NULL, TeleportBossToPlayer, nullptr, NULL, nullptr);
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
    if (!IsBadCodePtr((FARPROC) this->pX)) *this->pX = this->lockX;
    if (!IsBadCodePtr((FARPROC) this->pY)) *this->pY = this->lockY;
    if (!IsBadCodePtr((FARPROC) this->pZ)) *this->pZ = this->lockZ;
}

void Coordinate::lockTo(float x, float y, float z) {
    if (!IsBadCodePtr((FARPROC) this->pX)) *this->pX = x;
    if (!IsBadCodePtr((FARPROC) this->pY)) *this->pY = y;
    if (!IsBadCodePtr((FARPROC) this->pZ)) *this->pZ = z;
}

void Coordinate::update() {
    PDWORD64 pCoordinate = readFromMemSecurity(this->baseAddress, this->offsets);
    if (!pCoordinate) return;

    this->pX = (PFLOAT) (*pCoordinate + 0x190);
    this->pY = (PFLOAT) (*pCoordinate + 0x194);
    this->pZ = (PFLOAT) (*pCoordinate + 0x198);

    if (!IsBadCodePtr((FARPROC) this->pX)) this->x = *this->pX;
    if (!IsBadCodePtr((FARPROC) this->pY)) this->y = *this->pY;
    if (!IsBadCodePtr((FARPROC) this->pZ)) this->z = *this->pZ;

    if (inHunt && !lButtonDown && !rButtonDown) {
        this->lockX = this->x;
        this->lockY = this->y;
        this->lockZ = this->z;
    }
}

Boss::Boss() {
    this->positionLocked = false;

    // 0 hp
    this->attributes.emplace_back(processAddress + 0x4091010, std::initializer_list<DWORD64>{0x148, 0x440, 0, 0x758, 0x190, 0, 0x30}, 0.f);

    this->coordinate = Coordinate(processAddress + 0x3EACBE0, std::initializer_list<DWORD64>{0x2B8, 0x130, 0x100});
}

void Boss::updateAttribute() {
    for (Attribute &attribute : this->attributes) {
        attribute.pointer = (PFLOAT) readFromMemSecurity(attribute.baseAddress, attribute.offsets);
        if (attribute.pointer) attribute.value = *attribute.pointer;
    }

    if (!this->attributes[0].pointer || this->attributes[0].value == 0) {
        inHunt = false;
        startHunt = true;
    } else inHunt = true;
}

Player::Player() {
    // 0 attack speed
    // 1 movement speed
    // 2 instant movement
    // 3 stamina
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x758, 0x190, 0x8, 0xD4}, 10.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x758, 0x190, 0x10, 0x30}, 4.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x398, 0x214}, 100000.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x758, 0x190, 0, 0xB4}, 100.f);

    this->coordinate = Coordinate(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x48, 0x8, 0xC8});
}

void Player::updateAttribute() {
    for (Attribute &attribute : this->attributes) {
        attribute.pointer = (PFLOAT) readFromMemSecurity(attribute.baseAddress, attribute.offsets);
        if (!attribute.pointer) return;
        if (attribute.status) *attribute.pointer = attribute.value;
        else if (attribute.previousStatus) {
            attribute.previousStatus = false;
            if (attribute.status) *attribute.pointer = attribute.defaultValue;
        }
    }
}

Feature::Feature(void (*fn)()) {
    this->status = false;
    this->previousStatus = false;

    this->fn = fn;
}

Helper::Helper() {
    this->boss = Boss();
    this->player = Player();

    // 0 auto click
    // 1 lock player position on auto lock
    // 2 summon boss
    this->huntingFeatures.emplace_back(autoClick);
    this->huntingFeatures.emplace_back(nullptr);
    this->huntingFeatures.emplace_back(teleportBossToPlayer);
}

DWORD Helper::run(LPVOID) {
    while (g_initialised) {
        lButtonDown = GetKeyState(VK_LBUTTON) < 0;
        rButtonDown = GetKeyState(VK_RBUTTON) < 0;

        helper.player.updateAttribute();
        helper.player.coordinate.update();
        helper.boss.updateAttribute();

        if (inHunt) {
            if (startHunt) {
                startHunt = false;
                Sleep(18000);
            }

            helper.boss.coordinate.update();

            if (GetAsyncKeyState(VK_F2) & 1) helper.huntingFeatures[2].previousStatus = helper.huntingFeatures[2].status = !helper.huntingFeatures[2].status;
        }

        for (Feature &feature : helper.huntingFeatures) {
            if (inHunt) {
                feature.status = feature.previousStatus;
                if (feature.status && feature.fn) feature.fn();
            } else {
                feature.previousStatus = feature.status;
                feature.status = false;
            }
        }

        Sleep(16);
    }

    return NULL;
}

void Helper::draw() {
//    ImGui::ShowDemoWindow(&g_showMenu);

    ImGui::Begin("Dauntless Helper");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::Text("Author: Xavier Lau - c.estlavie@icloud.com");
    ImGui::Text("Github: github.com/AurevoirXavier/dauntless-helper");

    ImGui::Checkbox("Active menu", &g_activeMenu);

    if (ImGui::CollapsingHeader("Hunting features")) {
        ImGui::Checkbox("Auto click", &helper.huntingFeatures[0].status);
        helper.huntingFeatures[0].previousStatus = helper.huntingFeatures[0].status;
        ImGui::Checkbox("Lock player position on auto click", &helper.huntingFeatures[1].status);
        helper.huntingFeatures[1].previousStatus = helper.huntingFeatures[1].status;
        ImGui::Checkbox("Summon Boss", &helper.huntingFeatures[2].status);
        helper.huntingFeatures[2].previousStatus = helper.huntingFeatures[2].status;
    }

    if (ImGui::CollapsingHeader("Player Attributes")) {
        ImGui::Checkbox("Modify attack speed", &helper.player.attributes[0].status);
        ImGui::SliderFloat("attack speed", &helper.player.attributes[0].value, 1, 1000);
        ImGui::Checkbox("Modify movement speed", &helper.player.attributes[1].status);
        ImGui::SliderFloat("Movement speed", &helper.player.attributes[1].value, 1, 1000);
        ImGui::Checkbox("Enable instant movement", &(bool &) helper.player.attributes[2].status);
        ImGui::SliderFloat("Instant movement", &helper.player.attributes[2].value, 500, 100000);
        ImGui::Checkbox("Infinity stamina", &(bool &) helper.player.attributes[3].status);
    }

    if (ImGui::CollapsingHeader("Status")) {
        if (ImGui::TreeNode("Player")) {
            ImGui::Text("x %.2f | y %.2f | z %.2f", helper.player.coordinate.x, helper.player.coordinate.y, helper.player.coordinate.z);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Boss")) {
            ImGui::Text("Hp: %.2f", helper.boss.attributes[0].value);
            ImGui::Text("x %.2f | y %.2f | z %.2f", helper.boss.coordinate.x, helper.boss.coordinate.y, helper.boss.coordinate.z);
            ImGui::TreePop();
        }
    }
    ImGui::End();
}
