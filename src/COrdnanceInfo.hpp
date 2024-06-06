#pragma once
#include <vector>

struct RwFrame;
class CVehicle;

struct COrdnanceInfo
{
    std::vector<RwFrame*> guns;
    std::vector<RwFrame*> flares;
    std::vector<RwFrame*> rockets;
    std::vector<RwFrame*> missiles;

    struct
    {
        bool processed : 1;
        bool needProcess : 1;
        bool disableTheOriginal : 1;

        bool rockets_cyclic : 1;
        bool missiles_cyclic : 1;
    }flags;

    explicit COrdnanceInfo(CVehicle* vehicle) //parameter no used
    {
        flags.processed = false;
        flags.needProcess = false;
        flags.rockets_cyclic = false;
        flags.missiles_cyclic = false;
        flags.disableTheOriginal = false;
    }
};