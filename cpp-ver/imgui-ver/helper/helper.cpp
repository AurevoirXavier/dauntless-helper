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
//int huntingTimer = 0;

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

    // during teleport
    if (helper.huntingFeatures[2].status) return;
    if (helper.huntingFeatures[1].status && (lButtonDown || rButtonDown)) helper.player.coordinate.lock(); else helper.player.coordinate.isLocked = false;
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

DWORD WINAPI TeleportPlayerToBoss(LPVOID) {
    float dZ = helper.player.coordinate.z - helper.boss.coordinate.z;
    if (dZ < 0) move(helper.player.coordinate.pZ, helper.boss.coordinate.z + 100, nullptr, 15);
    move(helper.player.coordinate.pY, helper.boss.coordinate.y, helper.player.coordinate.pZ, 15);
    move(helper.player.coordinate.pX, helper.boss.coordinate.x, helper.player.coordinate.pZ, 15);
    // prevent fall down too fast
    if (dZ > 2000) move(helper.player.coordinate.pZ, helper.boss.coordinate.z + 100, nullptr, 10);

    helper.huntingFeatures[2].status = false;

    return NULL;
}

void teleportPlayerToBoss() {
    // locked by auto click
    if (helper.player.coordinate.isLocked) helper.huntingFeatures[2].status = false; else CreateThread(nullptr, NULL, TeleportPlayerToBoss, nullptr, NULL, nullptr);
}

DWORD WINAPI TeleportBossToPlayer(LPVOID) {
    helper.boss.coordinate.isLocked = true;

    float x = helper.player.coordinate.x + 500;
    float y = helper.player.coordinate.y;
    float z = helper.player.coordinate.z;

    //  need some delay to active some monsters like Rezakiri
    for (int i = 0; i < 100; i += 1) {
        helper.boss.coordinate.teleportTo(x, y, z);
        Sleep(1);
    }
    while (helper.huntingFeatures[3].status) helper.boss.coordinate.teleportTo(x, y, z);

    helper.boss.coordinate.isLocked = false;

    return NULL;
}

void teleportBossToPlayer() {
    // locked by self thread
    if (!helper.boss.coordinate.isLocked) CreateThread(nullptr, NULL, TeleportBossToPlayer, nullptr, NULL, nullptr);
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

void Coordinate::teleportTo(float x, float y, float z) {
    *this->pX = x;
    *this->pY = y;
    *this->pZ = z;
}

void Coordinate::update() {
    if (this->isLocked) return;

    this->x = *this->pX;
    this->y = *this->pY;
    this->z = *this->pZ;

    if (!lButtonDown && !rButtonDown) {
        this->lockX = this->x;
        this->lockY = this->y;
        this->lockZ = this->z;
    }
}

void Coordinate::initInHunt() {
    PDWORD64 pCoordinate = nullptr;
    while (!pCoordinate) {
        pCoordinate = readFromMemSecurity(this->baseAddress, this->offsets);
        Sleep(100);
    }

    this->pX = (PFLOAT) (*pCoordinate + 0x190);
    this->pY = (PFLOAT) (*pCoordinate + 0x194);
    this->pZ = (PFLOAT) (*pCoordinate + 0x198);

}

void Coordinate::clearData() {
    this->isLocked = false;

    this->pX = nullptr;
    this->pY = nullptr;
    this->pZ = nullptr;

    this->x = 0;
    this->y = 0;
    this->z = 0;

    this->lockX = 0;
    this->lockY = 0;
    this->lockZ = 0;
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
            lButtonDown = false;
            rButtonDown = false;
            helper.huntingFeatures[2].status = false;
            helper.huntingFeatures[3].status = false;

            Sleep(100);

            if (!startHunt) {
                helper.boss.coordinate.clearData();
                helper.player.coordinate.clearData();
            }

            startHunt = true;
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
    // 2 jump height
    // 3 instant movement 1
    // 4 instant movement 2
    // 5 stamina
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x758, 0x190, 0x8, 0xD4}, 10.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x758, 0x190, 0x10, 0x30}, 4.f);
    this->attributes.emplace_back(processAddress + 0x4076340, std::initializer_list<DWORD64>{0x30, 0x3B8, 0x398, 0x1AC}, 2000.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x398, 0x1F8}, 10000.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x398, 0x214}, 10000.f);
    this->attributes.emplace_back(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x758, 0x190, 0, 0xB4}, 106.f);

    this->coordinate = Coordinate(processAddress + 0x3E323B0, std::initializer_list<DWORD64>{0x10, 0x48, 0x8, 0xC8});
}

void Player::updateAttribute() {
    for (Attribute &attribute : this->attributes) {
        if (attribute.status) {
            attribute.previousStatus = true;
            *attribute.pointer = attribute.value;
        } else if (attribute.previousStatus) {
            attribute.previousStatus = false;
            if (attribute.status) *attribute.pointer = attribute.defaultValue;
        }
    }
}

void Player::initInHunt() {
    for (Attribute &attribute : this->attributes) {
        PFLOAT p = nullptr;
        while (!p) {
            p = (PFLOAT) readFromMemSecurity(attribute.baseAddress, attribute.offsets);
            Sleep(100);
        }

        attribute.pointer = p;
    }

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
    // 2 teleport to boss
    // 3 summon boss
    this->huntingFeatures.emplace_back(autoClick);
    this->huntingFeatures.emplace_back(nullptr);
    this->huntingFeatures.emplace_back(teleportPlayerToBoss);
    this->huntingFeatures.emplace_back(teleportBossToPlayer);
}

DWORD WINAPI Helper::run(LPVOID) {
    CreateThread(nullptr, NULL, Boss::monitoring, nullptr, NULL, nullptr);

    while (g_initialised) {
        Sleep(16);
        if (!inHunt) {
            Sleep(1000);
            continue;
        }

        if (startHunt) {
            startHunt = false;
            Sleep(16000);

            helper.player.initInHunt();
            helper.boss.initInHunt();
        }

        helper.boss.coordinate.update();
        helper.player.updateAttribute();
        helper.player.coordinate.update();

        if (dauntlessWindow == GetForegroundWindow()) {
            lButtonDown = GetKeyState(VK_LBUTTON) < 0;
            rButtonDown = GetKeyState(VK_RBUTTON) < 0;

            // feature 2 only call once
            if (GetAsyncKeyState(VK_F2) & 1) helper.huntingFeatures[2].status = true;
            // feature 3 in a loop
            if (GetAsyncKeyState(VK_F3) & 1) helper.huntingFeatures[3].status = !helper.huntingFeatures[3].status;
        }

        for (Feature &feature : helper.huntingFeatures) if (feature.status && feature.fn) feature.fn();
    }

    return NULL;
}

void Helper::draw() {
//    ImGui::ShowDemoWindow(&g_showMenu);

    ImGui::Begin("Dauntless Helper");

    ImGui::Checkbox("Active menu [F1] ", &g_activeMenu);

    if (ImGui::CollapsingHeader("Info")) {
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("Author: Xavier Lau - c.estlavie@icloud.com");
        ImGui::Text("Github: github.com/AurevoirXavier/dauntless-helper");
        ImGui::Text(" Press INSERT to hide the menu completely");
    }

    if (ImGui::CollapsingHeader("Hunting features")) {
        ImGui::Checkbox("Auto click", &helper.huntingFeatures[0].status);
        ImGui::Checkbox("Lock player position on auto click", &helper.huntingFeatures[1].status);
        ImGui::Checkbox("Teleport to Boss [F2]", &helper.huntingFeatures[2].status);
        ImGui::Checkbox("Summon Boss [F3]", &helper.huntingFeatures[3].status);
    }

    if (ImGui::CollapsingHeader("Player Attributes")) {
        ImGui::Checkbox("Modify attack speed", &helper.player.attributes[0].status);
        ImGui::SliderFloat("Attack speed", &helper.player.attributes[0].value, 1, 1000);
        ImGui::Checkbox("Modify movement speed", &helper.player.attributes[1].status);
        ImGui::SliderFloat("Movement speed", &helper.player.attributes[1].value, 1, 8);
        ImGui::Checkbox("Modify jump height", &helper.player.attributes[2].status);
        ImGui::SliderFloat("Jump height", &helper.player.attributes[2].value, 980, 5000);
        ImGui::Checkbox("Instant movement", &helper.player.attributes[3].status);
        helper.player.attributes[4].status = helper.player.attributes[3].status;
        ImGui::Checkbox("Infinity stamina", &helper.player.attributes[5].status);
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
