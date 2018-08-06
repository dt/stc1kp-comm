
#define  RETRIES     10 // ICSPCLK
#define  RETRY_WAIT  30 // ICSPCLK

#define COM_READ_EEPROM   0x20
#define COM_WRITE_EEPROM  0xE0
#define COM_READ_TEMP   0x01
#define COM_READ_COOLING  0x02
#define COM_READ_HEATING  0x03
#define COM_ACK     0x9A
#define COM_NACK    0x66

void write_bit(uint8_t pin, unsigned const char data){
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delayMicroseconds(7);
  if(!data){
    pinMode(pin, INPUT);
    digitalWrite(pin, LOW);
  }
  delayMicroseconds(400);
  pinMode(pin, INPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(100);
}

unsigned char read_bit(uint8_t pin){
  unsigned char data;

  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delayMicroseconds(7);
  pinMode(pin, INPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(200);
  data = digitalRead(pin);
  delayMicroseconds(300);

  return data;
}

void write_byte(uint8_t pin, unsigned const char data){
  unsigned char i;
  
  for(i=0;i<8;i++){
    write_bit(pin, ((data << i) & 0x80));
  }
  delayMicroseconds(500);
}

unsigned char read_byte(uint8_t pin){
  unsigned char i, data;
  
  for(i=0;i<8;i++){
    data <<= 1;
    if(read_bit(pin)){
      data |= 1;
    }
  }
  delayMicroseconds(500);

  return data;
}


bool write_eeprom(uint8_t pin, const unsigned char address, unsigned const int value){
  unsigned char ack;
  for (int i = 0; i < RETRIES; i++) {
    write_byte(pin, COM_WRITE_EEPROM);
    write_byte(pin, address);
    write_byte(pin, ((unsigned char)(value >> 8)));
    write_byte(pin, (unsigned char)value);
    write_byte(pin, COM_WRITE_EEPROM ^ address ^ ((unsigned char)(value >> 8)) ^ ((unsigned char)value));
    delay(6); // Longer delay needed here for EEPROM write to finish, but must be shorter than 10ms
    ack = read_byte(pin);
    if (ack == COM_ACK) 
      return true;
    delay(RETRY_WAIT);
   }
  return false; 
}

bool read_eeprom(int pin, const unsigned char address, int *value){
  unsigned char xorsum;
  unsigned char ack;
  unsigned int data;
  for (int i = 0; i < RETRIES; i++) {
    write_byte(pin, COM_READ_EEPROM);
    write_byte(pin, address);
    data = read_byte(pin);
    data = (data << 8) | read_byte(pin);
    xorsum = read_byte(pin);
    ack = read_byte(pin);
    if(ack == COM_ACK && xorsum == (COM_READ_EEPROM ^ address ^ ((unsigned char)(data >> 8)) ^ ((unsigned char)data))){
      *value = (int)data;
      return true;
    delay(RETRY_WAIT);
    }
  }
  return false;
}

bool read_command(uint8_t pin, unsigned char command, int *value){
  unsigned char xorsum;
  unsigned char ack;
  unsigned int data;

  for (int i = 0; i < RETRIES; i++) {
    write_byte(pin, command);
    data = read_byte(pin);
    data = (data << 8) | read_byte(pin);
    xorsum = read_byte(pin);
    ack = read_byte(pin);
  
    if(ack == COM_ACK && xorsum == (command ^ ((unsigned char)(data >> 8)) ^ ((unsigned char)data))){
      *value = (int)data;
      return true;
    }
    delay(RETRY_WAIT);
  }
  return false;
}

bool read_temp(uint8_t pin, int *temperature){
  return read_command(pin, COM_READ_TEMP, temperature); 
}

bool read_heating(uint8_t pin, int *heating){
  return read_command(pin, COM_READ_HEATING, heating); 
}

bool read_cooling(uint8_t pin, int *cooling){
  return read_command(pin, COM_READ_COOLING, cooling); 
}

/* End of communication implementation */

/* From here example implementation begins, this can be exchanged for your specific needs */
enum set_menu_enum {
  setpoint,     // SP (setpoint)
  hysteresis,     // hy (hysteresis)
  temperature_correction,   // tc (temperature correction)
  setpoint_alarm,     // SA (setpoint alarm)
  step,       // St (current running profile step)  
  duration,     // dh (current running profile step duration in hours)
  cooling_delay,      // cd (cooling delay minutes)
  heating_delay,      // hd (heating delay minutes)
  ramping,      // rP (0=disable, 1=enable ramping)
  run_mode      // rn (0-5 run profile, 6=thermostat)
};

const char menu_opt[][4] = {
  "SP",
  "hy",
  "tc",
  "SA",
  "St",
  "dh",
  "cd",
  "hd",
  "rP",
  "rn"
};

bool isBlank(char c){
  return c == ' ' || c == '\t';
}

bool isDigit(char c){
  return c >= '0' && c <= '9';
}

bool isEOL(char c){
  return c == '\r' || c == '\n';
}

void print_temperature(int temperature){
  if(temperature < 0){
    temperature = -temperature;
    Serial.print('-');
  }
  if(temperature >= 1000){
    temperature /= 10;
    Serial.println(temperature);
  } else {
    Serial.print(temperature/10);
    Serial.print('.');
    Serial.println(temperature%10);
  }
}

void print_config_value(unsigned char address, int value){
  if(address < EEADR_SET_MENU){
    unsigned char profile=0;
    while(address >= 19){
      address-=19;
      profile++;
    }
    if(address & 1){
      Serial.print("dh");
    } else {
      Serial.print("SP");
    }
    Serial.print(profile);
    Serial.print(address >> 1);
    Serial.print('=');
    if(address & 1){
      Serial.println(value);
    } else {
      print_temperature(value);
    }
  } else {
    Serial.print(menu_opt[address-EEADR_SET_MENU]);
    Serial.print('=');
    if(address == EEADR_SET_MENU_ITEM(run_mode)){
      if(value >= 0 && value <= 5){
        Serial.print("Pr");
        Serial.println(value);
      } else {
        Serial.println("th");
      }
    } else if(address <= EEADR_SET_MENU_ITEM(setpoint_alarm)){
      print_temperature(value);
    } else {
      Serial.println(value);
    }
  }
}

unsigned char parse_temperature(const char *str, int *temperature){
  unsigned char i=0;
  bool neg = false;

  if(str[i] == '-'){
    neg = true;
    i++;
  }

  if(!isDigit(str[i])){
    return 0;
  }

  *temperature = 0;
  while(isDigit(str[i])){
    *temperature = *temperature * 10 + (str[i] - '0');
    i++;
  }
  *temperature *= 10;
  if(str[i] == '.'){
    i++;
    if(isDigit(str[i])){
      *temperature += (str[i] - '0');
      i++;
    } else {
      return 0;
    } 
  }

  if(neg){
    *temperature = -(*temperature);
  }

  return i;
} 

unsigned char parse_address(const char *cmd, unsigned char *addr){
  char i; 

  if(!strncmp("SP", cmd, 2)){
    if(isDigit(cmd[2]) && isDigit(cmd[3]) && cmd[2] < '6'){
      *addr = EEADR_PROFILE_SETPOINT(cmd[2]-'0', cmd[3]-'0');
      return 4;
    }
  }

  if(!strncmp("dh", cmd, 2)){
    if(isDigit(cmd[2]) && isDigit(cmd[3]) && cmd[2] < '6' && cmd[3] < '9'){
      *addr = EEADR_PROFILE_DURATION(cmd[2]-'0', cmd[3]-'0');
      return 4;
    }
  }

  for(i=0; i<(sizeof(menu_opt)/sizeof(menu_opt[0])); i++){
    unsigned char len = strlen(menu_opt[i]);
    if(!strncmp(cmd, &menu_opt[i][0], len) && (isBlank(cmd[len]) || isEOL(cmd[len]))){
      *addr = EEADR_SET_MENU + i;
      return strlen(menu_opt[i]);
    }
  }

  *addr = 0;
  for(i=0; i<30; i++){
    if(isBlank(cmd[i]) || isEOL(cmd[i])){
      break;
    }
    if(isDigit(cmd[i])){
      if(*addr>12){
        return 0;
      } else {
        *addr = *addr * 10 + (cmd[i] - '0');
      }
    } else {
      return 0;
    }
  }

  if(*addr > 127){
    return 0;
  }

  return i;
}

unsigned char parse_config_value(const char *cmd, int address, bool pretty, int *data){
  unsigned char i=0;
  bool neg=false;

  if(pretty){
    if(address < EEADR_SET_MENU){
      while(address >= 19){
        address-=19;
      }
      if((address & 1) == 0){
        return parse_temperature(cmd, data);
      }
    } else if(address <= EEADR_SET_MENU_ITEM(setpoint_alarm)){
      return parse_temperature(cmd, data);
    } else if(address == EEADR_SET_MENU_ITEM(run_mode)) {
      if(!strncmp(cmd, "Pr", 2)){
        *data = cmd[2] - '0';
        if(*data >= 0 && *data <= 5){
          return 3;
        }
      } else if(!strncmp(cmd, "th", 2)){
        *data = 6;
        return 2;
      }
      return 0;
    }
  } 

  if(cmd[i] == '-'){
    neg = true;
    i++;
  }

  if(!isDigit(cmd[i])){
    return 0;
  }

  for(*data=0; i<6; i++){
    if(!isDigit(cmd[i])){
      break;
    }
    if(isDigit(cmd[i]) && *data < 3276){
      *data = *data * 10 + (cmd[i] - '0');
    } else {
      return 0;
    }
  }

  if((neg && *data > 32768) || (!neg && *data > 32767)){
    return 0;
  }

  if(neg){
    *data = -(*data);
  }
  
  return i;
}

void parse_command(char *cmd){
  int data;
  uint8_t pin = STC1;

  if(cmd[0] == 't'){
    if(!isEOL(cmd[1])){
      Serial.println("?Syntax error");
      return;
    }
    if(read_temp(pin, &data)){
      Serial.print("Temperature=");
      print_temperature(data);
    } else {
      Serial.println("?Communication error");
    }
  } else if(cmd[0] == 'h'){
    if(!isEOL(cmd[1])){
      Serial.println("?Syntax error");
      return;
    }
    if(read_heating(pin, &data)){
      Serial.print("Heating=");
      Serial.println(data ? "on" : "off");
    } else {
      Serial.println("?Communication error");
    }
  } else if(cmd[0] == 'c'){
    if(!isEOL(cmd[1])){
      Serial.println("?Syntax error");
      return;
    }
    if(read_cooling(pin, &data)){
      Serial.print("Cooling=");
      Serial.println(data ? "on" : "off");
    } else {
      Serial.println("?Communication error");
    }
  } else if(cmd[0] == 'r' || cmd[0] == 'w') {
    unsigned char address=0;
    unsigned char i=0, j;
    bool neg = false;

    if(!isBlank(cmd[1])){
      Serial.println("?Syntax error");
      return;
    }

    j = parse_address(&cmd[2], &address);
    i+=j+2;

    if(j==0){
      Serial.println("?Syntax error");
      return;
    }

    if(cmd[0] == 'r'){
      if(!isEOL(cmd[i])){
        Serial.println("?Syntax error");
        return;
      }
      if(read_eeprom(pin, address, &data)){
        if(isDigit(cmd[2])){
          Serial.print("EEPROM[");
          Serial.print(address);
          Serial.print("]=");
          Serial.println(data);
        } else {
          print_config_value(address, data);
        }
      } else {
        Serial.println("?Communication error");
      }
      return;
    }

    if(!isBlank(cmd[i])){
      Serial.println("?Syntax error");
      return;
    }
    i++;

    j = parse_config_value(&cmd[i], address, !isDigit(cmd[2]), &data);
    i += j;
    if(j == 0){
      Serial.println("?Syntax error");
      return;
    } else {
      if(!isEOL(cmd[i])){
        Serial.println("?Syntax error");
        return;
      }
      Serial.println(address);
      Serial.println(EEADR_SET_MENU);
      Serial.println(data);
      
      if(write_eeprom(pin, address, data)){
        Serial.println("Ok");
      } else {
        Serial.println("?Communication error");
      }
    }
  }
}

