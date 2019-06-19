#include <Windows.h>
#include <WinBase.h>
#include <WinUser.h>
#include <cmath>
#include <detours.h>
#include <d3d11.h>
#include <FW1FontWrapper.h>
#include <iostream>
#include <sstream>
#include <vector>

#pragma once
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "detours.lib")
#pragma comment(lib, "FW1FontWrapper.lib")

using namespace std;

typedef HRESULT(WINAPI *Present)(IDXGISwapChain *pSwapChain, UINT syncInterval, UINT flags);

class Player {
public:
    PFLOAT pAttackSpeed;
    PFLOAT pMovementSpeed;
    PFLOAT pInstantMovement;
    PFLOAT pStamina;

    PFLOAT pX;
    float x;
    PFLOAT pY;
    float y;
    PFLOAT pZ;
    float z;

    Player();

    bool updateAttribute();

    bool updateCoordinate();

    void lockPosition();

    static DWORD teleportToBoss(LPVOID);
};

typedef void(*hack)(Player *, bool);

class Boss {
public:
    bool positionLocked;

    PFLOAT pHp;
    float hp;

    PFLOAT pX;
    PFLOAT pY;
    PFLOAT pZ;

    Boss();

    void updateAttribute();

    void updateCoordinate();

    static DWORD teleportToPlayer(LPVOID);
};

class Option {
public:
    bool status;
    bool prevStatus;
    LPCWSTR text;
    hack feature;

    explicit Option(LPCWSTR, hack);
};

class Menu {
public:
    bool visible;

    float y;
    Present present;
    DWORD64 processAddress;

    ID3D11DeviceContext *pContext;
    ID3D11RenderTargetView *pTargetView;
    IFW1FontWrapper *pFontWrapper;

    vector<Option> options;
    uint8_t selectedOption;

    Player player;
    Boss boss;

    Menu();

    ~Menu();

    void draw();

    static DWORD run(LPVOID);
};
