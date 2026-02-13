
/*
Cette classe implement la gestion des capteurs;
Sensor Manager est responsable de :

Stocker tous les capteurs.

Appeler init() pour tous les capteurs.

Appeler update() régulièrement (boucle ou tâche FreeRTOS).

Fournir un accès aux valeurs pour le reste du robot.
*/

#include <vector>
#include "SensorsInterface.hpp"
#include "SensorTypes.hpp"

using namespace std;


class SensorManager
{
private:
    vector <SensorsInterface*> sensors;
public:
   void addSensor( SensorsInterface * sensor){
    sensors.push_back(sensor);
   }

   bool initAll(){
    bool allOk = true;
    for (auto s: sensors)
    {
        allOk &= s->init(); 
    }
    return allOk;
}
  void updateAll(){
      for (auto s: sensors)
    {
        s->update(); 
    }
  }

  float getValue(SensorType type){
    return sensors[static_cast<int>(type)]->getValue();
  }
};
