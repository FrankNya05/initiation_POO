#pragma once
#include <cstdint>
#include <vector>
namespace RobotConstants {

enum class Direction { AVANT, DROITE, GAUCHE, ARRIERE };
enum class MotorSide  { LEFT, RIGHT };
enum class RobotState { IDLE, SEARCHING, ATTACKING, FLEEING };

}