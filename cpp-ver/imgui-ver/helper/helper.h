#include <Windows.h>
#include <imgui.h>
#include <iostream>
#include <vector>

class Attribute {
public:
    bool status;
    bool previousStatus;

    DWORD64 baseAddress;
    std::vector<DWORD64> offsets;
    PFLOAT pointer;
    float value;
    float defaultValue;

    Attribute();

    Attribute(DWORD64, std::vector<DWORD64>, float);
};

class Coordinate {
public:
    bool isLocked;

    DWORD64 baseAddress;
    std::vector<DWORD64> offsets;
    PFLOAT pX;
    PFLOAT pY;
    PFLOAT pZ;

    float x;
    float lockX;
    float y;
    float lockY;
    float z;
    float lockZ;

    Coordinate();

    Coordinate(DWORD64, std::vector<DWORD64>);

    void lock();

    void lockTo(float, float, float);

    void update();

    void initInHunt();

    void clearData();
};

class Boss {
public:
//    std::vector<Attribute> attributes;
    Attribute hp;

    Coordinate coordinate;

    Boss();

//    void updateAttribute();

    static DWORD WINAPI monitoring(LPVOID);

    void initInHunt();
};

class Player {
public:
    std::vector<Attribute> attributes;

    Coordinate coordinate;

    Player();

    void updateAttribute();

    void initInHunt();
};

class Feature {
public:
    bool status;

    void (*fn)();

    explicit Feature(void(*)());
};

class Helper {
public:
    LPARAM cursor;

    Boss boss;
    Player player;

    std::vector<Feature> huntingFeatures;

    Helper();

    static DWORD WINAPI run(LPVOID);

    static void draw();
};
