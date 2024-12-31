#include <arduinojson.h>

/// @brief Get the string value from a JsonObject
/// @param obj the json object
/// @param key the key in the json object
/// @return the value from the key given
String GetStringValue(JsonObject obj, String key);

/// @brief Print the value of a key
/// @param obj The json object
/// @param key The key in the json object
void PrintValue(JsonObject obj, String key);