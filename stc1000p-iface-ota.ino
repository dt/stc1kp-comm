#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>

#define STC1 4 // pin for STC1
#define STC2 5  // pin for STC2

const char* ssid = "snail";
const char* password = "uuddlrlrba";


ESP8266WebServer server ( 80 );
HTTPClient http;

void setup(void)
{
  Serial.begin(115200);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");  
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  server.on ( "/", handleRoot );
  server.on ( "/set", handleSet );
  server.begin();
  
  Serial.println("webserver started");
  MDNS.addService("http", "tcp", 80);
  
  ArduinoOTA.setHostname("blinky");
  ArduinoOTA.begin();
}

/* Defines for EEPROM config addresses */
#define EEADR_PROFILE_SETPOINT(profile, stp)  (((profile)*19) + ((stp)<<1))
#define EEADR_PROFILE_DURATION(profile, stp)  (EEADR_PROFILE_SETPOINT(profile, stp) + 1)
#define EEADR_SET_MENU        EEADR_PROFILE_SETPOINT(6, 0)
#define EEADR_SET_MENU_ITEM(name)   (EEADR_SET_MENU + (name))
#define EEADR_POWER_ON        127

String str_temp(int temp) {
  if (temp == -999) {
    return "err";
  }
  if (temp == -998) {
    return "Off";
  }
  return String(float(temp)/ 10, 1);
}

String getStatus(uint8_t pin) {
  int status;
  if(!read_eeprom(pin, EEADR_POWER_ON, &status))
    return "comm err";
  if (!status) 
    return "off";
  
  if (!read_cooling(pin, &status)) 
    return "comm err";
  if (status)
    return "cooling";
  return "idle";
}

void handleRoot() {
  int sp1;
  if(!read_eeprom(STC1, EEADR_SET_MENU, &sp1))
    sp1 = -999;    
  int up1 = sp1 + 10;
  int down1 = sp1 - 10;

  int hy1;
  if(!read_eeprom(STC1, EEADR_SET_MENU+1, &hy1)) 
    hy1 = -999;
  int otherHy1 = 10;
  if (hy1 == 10) 
    otherHy1 = 50;
  if (hy1 == 50)
    otherHy1 = 5;
  
  String status1 = getStatus(STC1);
  int curTemp1;
 if (status1 == "off")
     curTemp1 = -998;
  else if(!read_temp(STC1, &curTemp1))
    curTemp1 = -999;

    
  int sp2;
  if(!read_eeprom(STC2, EEADR_SET_MENU, &sp2))
    sp1 = -999;
    
  int up2 = sp2 + 10;
  int down2 = sp2 - 10;

  int hy2;
  if(!read_eeprom(STC2, EEADR_SET_MENU+1, &hy2)) 
    hy1 = -999;

  int otherHy2 = 10;
  if (hy2 == 10) 
    otherHy2 = 50;
  if (hy2 == 50)
    otherHy2 = 5;
  
  String status2 = getStatus(STC2);
  int curTemp2;
  if (status2 == "off")
     curTemp2 = -998;
  else if(!read_temp(STC2, &curTemp2))
    curTemp2 = -999;

  String page = String(
    "<html><head><meta charset='utf-8'/>"
    "<meta http-equiv=\"refresh\" content=\"60\" />"
    "<title>Temp Control</title>"
    "<link href='https://fonts.googleapis.com/icon?family=Material+Icons' rel='stylesheet'/>"
    "<style>"
    "body { font-family: Helvetica, Arial, Sans-Serif; background-color: #eee; }"
    "a { color: inherit; } "
    ".stc { max-width: 300; min-height: 140px; background-color: #fff; border-radius: 5px; padding: 10px; margin-bottom: 10px; box-shadow: rgba(0,0,0,0.8) 0 0 10px;}"
    ".status { height: 75px; width: 75px; }"
    ".status.cooling { content: url('https://www.dropbox.com/s/ooh2oooyg50muew/on.gif?raw=1') }"
    ".status.idle { content: url('https://www.dropbox.com/s/1k55rfydein6k7z/off.png?raw=1') }"
    ".status.off { display: none }"
    ".current { font-size: 3em; }"
    ".controls { float: right; }"
    ".controls a { display: block; border-radius: 5px; text-decoration: none; color: #000; background-color: #EEEEEE; padding: 8px; margin: 2px; border:#aaa 1px solid;}"
    ".controls a i { font-size: 48px; }"
    "</style>"
    "<meta name='viewport' content='width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0'>"
    "</head>"
    "<body>\n"
    "<div class='stc'>\n"
      "<div class='controls'>\n"
        "<a href='/set?sp1="+str_temp(up1)+"'><i class='material-icons'>keyboard_arrow_up</i></a>\n"
        "<a href='/set?sp1="+str_temp(down1)+"'><i class='material-icons'>keyboard_arrow_down</i></a>\n"
       "</div>\n"
        "<div class='current'> A: " + str_temp(curTemp1) + "</div>\n"
        "<div class='status " + status1 + "'>" + status1 +"</div>\n"
        "<div class='cfg'>Target: <span>" + str_temp(sp1) + "</span> ± <a href='/set?hy1="+str_temp(otherHy1)+"'>"+str_temp(hy1) + "</a></div>\n"
        "<form class='set' action='/set' method=get>"
        "<input type=text name='sp1' size=4 value='"+str_temp(sp1) + "' />"
        "<input type=text name='hy1' size=4 value='"+str_temp(hy1) + "' />"
        "<input type=submit value='Apply' />"
        "</form>"
    "</div>" //  /stc
    "<div class='stc'>\n"
      "<div class='controls'>\n"
        "<a href='/set?sp1="+str_temp(up2)+"'><i class='material-icons'>keyboard_arrow_up</i></a>\n"
        "<a href='/set?sp1="+str_temp(down2)+"'><i class='material-icons'>keyboard_arrow_down</i></a>\n"
      "</div>\n"
      "<div class='current'>B: " + str_temp(curTemp2) + "</div>\n"
      "<div class='status " + status2 + "'>" + status2 +"</div>\n"
      "<div class='cfg'>Target: <span>" + str_temp(sp2) + "</span> ± <a href='/set?hy2="+str_temp(otherHy2)+"'>"+str_temp(hy2) + "</a></div>\n"
      "<form class='set' action='/set' method=get>"
        "<input type=text name='sp2' size=4 value='"+str_temp(sp2) + "' />"
        "<input type=text name='hy2' size=4 value='"+str_temp(hy2) + "' />"
        "<input type=submit value='Apply' />"
      "</form>"
    "</div>" //  /stc
    "</body></html>"
  );
  server.send(200, "text/html", page);
}

void handleSet() {  
  if (server.hasArg("sp1")) {
    int data = int(server.arg("sp1").toFloat()*10);
    if(!(write_eeprom(STC1, EEADR_SET_MENU, data))){
      server.send(500, "text/plain", "set SP1: comm err");
      return;
    }
    delay(10);
  }
  if (server.hasArg("hy1")) {
    int data = int(server.arg("hy1").toFloat()*10);
    if(!(write_eeprom(STC1, EEADR_SET_MENU+1, data))){
      server.send(500, "text/plain", "set hy1: comm err");
      return;
    }
    delay(10);
  }

  if (server.hasArg("sp2")) {
    int data = int(server.arg("sp2").toFloat()*10);
    if(!(write_eeprom(STC2, EEADR_SET_MENU, data))){
      server.send(500, "text/plain", "set SP2: comm err");
      return;
    }
    delay(10);
  }
  if (server.hasArg("hy2")) {
    int data = int(server.arg("hy2").toFloat()*10);
    if(!(write_eeprom(STC2, EEADR_SET_MENU+1, data))){
      server.send(500, "text/plain", "set hy2: comm err");
      return;
    }
    delay(10);
  }
  server.sendHeader("Location", "/", false);
  server.sendHeader("Cache-Control", "no-cache");
  server.send(302);
}

void logToInflux(String name, uint8_t pin) {
  int status;
  if(!read_eeprom(pin, EEADR_POWER_ON, &status))
    return;
    
  if (!status) 
    return;

  int temp;
  if(!read_temp(pin, &temp))
    return;

  int sp;
  if(!read_eeprom(pin, EEADR_SET_MENU, &sp))
    return;

  int hy;
  if(!read_eeprom(pin, EEADR_SET_MENU+1, &hy)) 
    return;

  int cooling;
  if (!read_cooling(pin, &cooling)) 
    return;
    
  int heating;
  if (!read_heating(pin, &heating)) 
    return;
  
  http.begin("http://192.168.4.4:8086/write?db=brewery");
  int httpCode = http.POST(String(name +
    " Temperature="+String(float(temp)/ 10, 1) +
    ",SP="+String(float(sp)/ 10, 1) +
    ",hy="+String(float(hy)/ 10, 1) +
    ",cooling="+String(cooling)+
    ",heating="+String(heating)
   ));
  
  if(httpCode > 0) {
    Serial.print("Reply(");
    Serial.print(httpCode);
    Serial.print("): ");
    Serial.print(http.getString());
  }
  http.end();
}

unsigned long lastReport;
unsigned long nextReport;
uint32 ticks;

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  // Serial CLI for stc1000k comm
  static char cmd[32], rxchar=' ';
  static unsigned char index=0;
  if(Serial.available() > 0){
    char c = Serial.read();
    if(!(isBlank(rxchar) && isBlank(c))){
      cmd[index] = c;
      rxchar = c;
      index++;
    }

    if(index>=31 || isEOL(rxchar)){
      cmd[index] = '\0';
      parse_command(cmd);
      index = 0;
      rxchar = ' ';
    }
  }
  ticks++;
  if (ticks > 10000) {
    ticks = 0;
    unsigned long now = millis();
    if (now > nextReport || now < lastReport) {
       nextReport = now + 1000 * 60;
       logToInflux("stc_a", STC1);
       logToInflux("stc_b", STC2);
       lastReport = now;
    }
  }
}


