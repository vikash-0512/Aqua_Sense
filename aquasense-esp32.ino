/* AQUASENSE ESP32 water sensor monitor
   Browser dashboard connects to ws://<ESP32-IP>/ws (WebSockets on port 80).
   Serial commands: A4.01, B6.86, Z, R (115200 baud, newline).
*/
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <math.h>

// Enter your 2.4 GHz Wi-Fi credentials.
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const uint16_t WEBSOCKET_PORT = 80;

const int PIN_DS18B20 = 4, PIN_PH = 34, PIN_LDR = 35, PIN_BUZZER = 26;
const float TEMP_MAX_C=35.0, TEMP_MIN_C=5.0, PH_MIN=6.5, PH_MAX=8.5;
const float LDR_CHANGE_PCT=25.0;
const int LDR_RAW_MIN=300, LDR_RAW_MAX=3800;
const float PH_NEUTRAL_V=2.50, PH_SLOPE_V=0.18, DIVIDER_FACTOR=2.0;
const int SAMPLES=30;
const unsigned long PRINT_INTERVAL_MS=20000;
OneWire oneWire(PIN_DS18B20);
DallasTemperature tempSensor(&oneWire);
Preferences prefs;
WebSocketsServer webSocket(WEBSOCKET_PORT);
float calV_A=0,calPH_A=0,calV_B=0,calPH_B=0,ldrBaseline=0;
bool hasA=false,hasB=false,hasBaseline=false;
unsigned long lastPrint=0,lastWifiRetry=0;

float averageRaw(int pin){long sum=0;for(int i=0;i<SAMPLES;i++){sum+=analogRead(pin);delay(5);}return sum/(float)SAMPLES;}
float averageMilliVolts(int pin){long sum=0;for(int i=0;i<SAMPLES;i++){sum+=analogReadMilliVolts(pin);delay(5);}return sum/(float)SAMPLES;}
float phSensorVoltage(){return (averageMilliVolts(PIN_PH)/1000.0f)*DIVIDER_FACTOR;}
bool phCalibrated(){return hasA&&hasB&&fabsf(calV_B-calV_A)>0.02f;}
float voltageToPH(float v){return calPH_A+((calPH_B-calPH_A)/(calV_B-calV_A))*(v-calV_A);}
float voltageToApproxPH(float v){return 7.0f+(PH_NEUTRAL_V-v)/PH_SLOPE_V;}
void beep(int ms){digitalWrite(PIN_BUZZER,HIGH);delay(ms);digitalWrite(PIN_BUZZER,LOW);}
void loadSaved(){prefs.begin("waterq",true);hasA=prefs.getBool("hasA",false);hasB=prefs.getBool("hasB",false);calV_A=prefs.getFloat("vA",0);calPH_A=prefs.getFloat("phA",0);calV_B=prefs.getFloat("vB",0);calPH_B=prefs.getFloat("phB",0);hasBaseline=prefs.getBool("hasBase",false);ldrBaseline=prefs.getFloat("base",0);prefs.end();}
void handleCommand(String cmd){cmd.trim();if(!cmd.length())return;char c=toupper(cmd.charAt(0));
 if(c=='A'||c=='B'){float ph=cmd.substring(1).toFloat();if(ph<1||ph>13){Serial.println("Use A4.01 or B6.86");return;}float v=phSensorVoltage();prefs.begin("waterq",false);if(c=='A'){calV_A=v;calPH_A=ph;hasA=true;prefs.putFloat("vA",v);prefs.putFloat("phA",ph);prefs.putBool("hasA",true);}else{calV_B=v;calPH_B=ph;hasB=true;prefs.putFloat("vB",v);prefs.putFloat("phB",ph);prefs.putBool("hasB",true);}prefs.end();Serial.printf("Saved %c: pH %.2f at %.3f V\n",c,ph,v);if(phCalibrated())Serial.println("Two-point pH calibration complete.");
 }else if(c=='Z'){ldrBaseline=averageRaw(PIN_LDR);hasBaseline=true;prefs.begin("waterq",false);prefs.putFloat("base",ldrBaseline);prefs.putBool("hasBase",true);prefs.end();Serial.printf("LDR raw baseline saved: %.0f\n",ldrBaseline);
 }else if(c=='R'){prefs.begin("waterq",false);prefs.clear();prefs.end();hasA=hasB=hasBaseline=false;ldrBaseline=0;Serial.println("Saved pH calibration and LDR baseline erased.");}else Serial.println("Commands: A4.01 | B6.86 | Z | R");}
void onWebSocketEvent(uint8_t client,WStype_t type,uint8_t *payload,size_t length){(void)client;(void)payload;(void)length;if(type==WStype_CONNECTED)Serial.println("Dashboard WebSocket connected");else if(type==WStype_DISCONNECTED)Serial.println("Dashboard WebSocket disconnected");}
String jsonNumber(float v,int digits){if(!isfinite(v))return "null";return String(v,digits);}
void sendReading(){
 tempSensor.requestTemperatures();float tempC=tempSensor.getTempCByIndex(0);bool tempOk=tempC!=DEVICE_DISCONNECTED_C&&isfinite(tempC)&&tempC>-100&&tempC<150;
 float phRaw=averageRaw(PIN_PH),phV=phSensorVoltage(),ldrRaw=averageRaw(PIN_LDR);bool calibrated=phCalibrated();float ph=calibrated?voltageToPH(phV):voltageToApproxPH(phV);bool phOk=isfinite(ph)&&ph>=0&&ph<=14;
 float change=hasBaseline&&ldrBaseline>0?(ldrRaw-ldrBaseline)/ldrBaseline*100.0f:NAN;
 bool buzzerActive=false;String alerts="[";bool first=true;
 auto addAlert=[&](const char *s){if(!first)alerts+=",";alerts+="\"";alerts+=s;alerts+="\"";first=false;};
 if(!tempOk)addAlert("TEMP SENSOR ERROR");else if(tempC>TEMP_MAX_C)addAlert("TEMP HIGH");else if(tempC<TEMP_MIN_C)addAlert("TEMP LOW");
 if(!phOk)addAlert("PH SENSOR INVALID");else {if(ph<PH_MIN)addAlert("PH LOW");if(ph>PH_MAX)addAlert("PH HIGH");}
 if(hasBaseline&&ldrBaseline>0){if(fabsf(change)>LDR_CHANGE_PCT)addAlert("LDR CHANGED");}else if(ldrRaw<LDR_RAW_MIN||ldrRaw>LDR_RAW_MAX)addAlert("LDR OUT OF RANGE");
 alerts+="]";buzzerActive=alerts!="[]";
 Serial.printf("Temp: %s | pH %s %.2f (%.3f V) | LDR raw %.0f",tempOk?String(tempC,2).c_str():"ERROR",calibrated?"calibrated":"approx",ph,phV);
 if(hasBaseline&&ldrBaseline>0)Serial.printf(" (%+.1f%% vs baseline)",change);Serial.printf(" | Alerts: %s\n",buzzerActive?alerts.c_str():"none");
 String msg="{";msg+="\"temperature\":"+(tempOk?jsonNumber(tempC,2):"null");msg+=",\"temperatureValid\":"+(tempOk?"true":"false");msg+=",\"ph\":"+(phOk?jsonNumber(ph,2):"null");msg+=",\"phValid\":"+(phOk?"true":"false");msg+=",\"phVoltage\":"+jsonNumber(phV,3);msg+=",\"phRaw\":"+jsonNumber(phRaw,0);msg+=",\"phCalibrated\":"+(calibrated?"true":"false");msg+=",\"ldr\":"+jsonNumber(ldrRaw,0);msg+=",\"ldrBaseline\":"+(hasBaseline?jsonNumber(ldrBaseline,0):"null");msg+=",\"ldrChangePercent\":"+jsonNumber(change,1);msg+=",\"hasLdrBaseline\":"+(hasBaseline?"true":"false");msg+=",\"alerts\":"+alerts;msg+=",\"alertActive\":"+(buzzerActive?"true":"false");msg+=",\"buzzerActive\":"+(buzzerActive?"true":"false");msg+="}";
 webSocket.broadcastTXT(msg);if(buzzerActive){beep(300);delay(150);beep(300);}
}
void setup(){Serial.begin(115200);delay(500);pinMode(PIN_BUZZER,OUTPUT);digitalWrite(PIN_BUZZER,LOW);analogReadResolution(12);analogSetPinAttenuation(PIN_PH,ADC_11db);analogSetPinAttenuation(PIN_LDR,ADC_11db);tempSensor.begin();loadSaved();
 Serial.println("\n=== AQUASENSE ESP32 ===");Serial.printf("DS18B20 sensors found: %d\n",tempSensor.getDeviceCount());Serial.println(phCalibrated()?"pH: two-point calibrated":"pH: APPROXIMATE (no complete calibration)");Serial.println("Serial commands: A4.01 | B6.86 | Z | R");
 WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);Serial.print("Connecting to Wi-Fi");unsigned long start=millis();while(WiFi.status()!=WL_CONNECTED&&millis()-start<30000){delay(500);Serial.print('.');}Serial.println();if(WiFi.status()==WL_CONNECTED){Serial.print("Wi-Fi connected. ESP32 IP: ");Serial.println(WiFi.localIP());}else Serial.println("Wi-Fi not connected; will retry in loop.");
 webSocket.begin();webSocket.onEvent(onWebSocketEvent);beep(100);lastPrint=millis()-PRINT_INTERVAL_MS;
}
void loop(){webSocket.loop();if(Serial.available())handleCommand(Serial.readStringUntil('\n'));if(WiFi.status()!=WL_CONNECTED && millis()-lastWifiRetry>10000){lastWifiRetry=millis();WiFi.reconnect();}if(millis()-lastPrint>=PRINT_INTERVAL_MS){lastPrint=millis();sendReading();}}
