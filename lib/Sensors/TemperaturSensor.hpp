/*
* Capteur de temperature interne de la puce 
*/
#include "SensorsInterface.hpp"


class TemperaturSensor: public SensorsInterface
{
private:
   bool _state;
public:
    bool  init(){

    }

    // update sensor value
    void update(){

    }

     // get sensor value
    float getValue(){

    }

    SensorType getType(){

    }

};

