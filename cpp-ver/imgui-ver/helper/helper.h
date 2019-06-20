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
};

class Boss {
public:
    bool positionLocked;

    std::vector<Attribute> attributes;

    Coordinate coordinate;

    Boss();

    void updateAttribute();
};

class Player {
public:
    std::vector<Attribute> attributes;

    Coordinate coordinate;

    Player();

    void updateAttribute();
};

class Feature {
public:
    bool status;
    bool previousStatus;

    void (*fn)();

    explicit Feature(void(*)());
};

class Helper {
public:
    Boss boss;
    Player player;

    std::vector<Feature> huntingFeatures;

    Helper();

    static DWORD run(LPVOID);

    static void draw();
};
