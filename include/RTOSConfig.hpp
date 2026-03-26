#pragma once

// Dans un header de config
#define DEBUG_ENABLED  // ← commenter pour désactiver tout

#ifdef DEBUG_ENABLED
  #define LOG(msg)        Serial.println(msg)
  #define LOGF(fmt, ...)  Serial.printf(fmt, __VA_ARGS__)
#else
  #define LOG(msg)        // ← remplacé par rien, zéro coût
  #define LOGF(fmt, ...)
#endif
