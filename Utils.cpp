#include <arduinojson.h>

String GetStringValue(JsonObject obj, String key)
{
  if(obj[key].is<String>()) {
    return obj[key].as<String>();
  } else {
    Serial.println(key + " doesn't exist in library");
    return "";
  }
}

void PrintValue(JsonObject obj, String key)
{
  if(obj[key].is<String>())
    Serial.println(obj[key].as<String>());
  else
    Serial.println(key + " doesn't exist in library");
}