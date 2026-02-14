
#include <vector>
#include <memory>
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"
using namespace std;
// namespace std;

class SensorManager {
private:
    vector <unique_ptr<SensorsInterface>> sensors;

public:
    void addSensor(unique_ptr<SensorsInterface> sensor) {
        sensors.push_back(move(sensor));
    }

    bool initAll() {
        bool ok = true;
        for (auto& s : sensors) ok &= s->init();
        return ok;
    }

    void updateAll() {
        for (auto& s : sensors) s->update();
    }

    float getValue(SensorType type) {
        for (auto& s : sensors)
            if (s->getType() == type)
                //return s->getValue();
                return -10.0;
        return -1.0f;
    }
};