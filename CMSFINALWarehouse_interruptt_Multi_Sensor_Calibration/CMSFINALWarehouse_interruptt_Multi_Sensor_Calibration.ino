#include "DHT.h"
#define DHTPIN1 3 // Temp/Hum Sensor is Connected to Pin 4
#define DHTPIN2 7
#define DHTPIN3 6
#define DHTPIN4 4
#define doorPin 2 // Door Sensor is Connected to Pin 2
#define lcdPin1 A4 //Door Open Indicator
#define lcdPin2 10 //Door Close Indicator
#define lcdPin3 13  //Data Sending Indicator
#define SENSORNUMBERADDRESS 2
#define INITIALIZATIONSTATUSADDRESS 1
#define DEVICECALIBRATIONADDRESS 3
#define DEVICEPASSWORDADDRESS 100
#define DEVICEAPIADDRESS 150
#define DEVICEINTERVALADDRESS 50
#define DHTTYPE DHT22 
char avgTemperatureValue[7]; // Arrary to Store Temperature Value
char avgHumidityValue[7]; // Array to Store Humidity Value
byte doorPrevStatus = 0; 
byte doorStatus = 0;
long previousMillis = 0;
boolean initialValue = false;
#include <EEPROM.h> // EEPROM is used to Store Volatile Data
#include <SoftwareSerial.h> // This Library is Used to Communicate with GSM
#define gsmPin 5 // GSM Power is Connected to Pin 5
SoftwareSerial mySerial(8,9);
uint8_t answer = 0;
char aux_str[50];
String smsSIMNumber; 
byte gsmONOFF = 0; // Check the Power Status of GSM
byte gsmStatus = 0; // Check the Network Status of GSM
byte messageFlag;
char *deviceId = "1003";
byte dataSendingInterval = 0;
char devicePassword[7];
char deviceAPI[50];
byte deviceSensorNumber = 0;
byte confStatus = 0; // Check if the Device is Configured or Not
byte initStatus = 0; // Check if the Device is SET to Send Data
byte i = 0;
byte calibrationValue = 0;
char calibrationSymbol = '+';
#include <LiquidCrystal.h>
LiquidCrystal lcd(A3, A2, A1, A0, 12, 11);

void setup() {
//  Serial.begin(9600);
  mySerial.begin(9600);
  lcd.begin(20, 4);
  pinMode(gsmPin, OUTPUT);
  pinMode(lcdPin1, OUTPUT);
  pinMode(lcdPin2, OUTPUT);
  pinMode(lcdPin3, OUTPUT);
  pinMode(doorPin,INPUT_PULLUP);
  setCalibrationFactor(calibrationValue,calibrationSymbol);
  setSensorNumber(0);
  confStatus = getDeviceInfoFromEEPROM();
  initStatus = getInitializationStatus();
  attachInterrupt(digitalPinToInterrupt(doorPin), doorStatusCheck, CHANGE);
  doorStatus = digitalRead(doorPin);
  if (doorStatus == 1)
  {
    digitalWrite(lcdPin1, LOW);
    digitalWrite(lcdPin2, HIGH);
  }
  else
  {
    digitalWrite(lcdPin2, LOW);
    digitalWrite(lcdPin1, HIGH);
  }
}

void loop() {
  char* dataToSend = readTempHumSensor();
  float difference = float(float((millis() - previousMillis)/1000)/60); // Calculate Time Difference
  if((confStatus == 1 && initStatus == 0) && ((difference > dataSendingInterval) || (initialValue == false) || (doorStatus != doorPrevStatus)))
  {
    gsmStatus = setNetworkGSM();
    if (gsmStatus == 1)
    {
      digitalWrite(lcdPin3, HIGH);
      sendLastValue(doorStatus,dataToSend);
    }
  }
  confStatus = checkSMSToConfigure();
  initStatus = getInitializationStatus();
  delay(1000);
}

float getTemperatureValue(byte curPosX, DHT &dht, char* allSensorsValue, float avgTemperature, float avgHumidity)
{
  float temperature = 0;
  float humidity = 0;
  char temperatureValue[6];
  char humidityValue[6];
  dht.begin();
  memset(temperatureValue,'\0',sizeof(temperatureValue));
  memset(humidityValue,'\0',sizeof(humidityValue));
  lcd.setCursor(0,curPosX);
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity))
  {
    temperature = 0;
    humidity = 0;
  }
  else
  {
    if (calibrationSymbol == '+')
    {
      temperature = temperature + calibrationValue;
    }
    else
    {
      temperature = temperature - calibrationValue;
    }
  }
  avgTemperature = avgTemperature + temperature;
  
  avgHumidity = avgHumidity + humidity;
  dtostrf(temperature,4,2,temperatureValue);
  dtostrf(humidity,4,2,humidityValue);
  dtostrf(avgTemperature,5,2,avgTemperatureValue);
  dtostrf(avgHumidity,5,2,avgHumidityValue);
  
  lcd.print("T:");
  lcd.print(temperatureValue);
  lcd.print(" C");
  lcd.setCursor(10,curPosX);
  lcd.print("H:");
  lcd.print(humidityValue);
  lcd.print(" %");

  strcat(allSensorsValue,"T");
  strcat(allSensorsValue,temperatureValue);
  strcat(allSensorsValue,",");
  strcat(allSensorsValue,"H");
  strcat(allSensorsValue,humidityValue);
  strcat(allSensorsValue,"+");
  
}
char* readTempHumSensor() // Function to Check Temperature + Humidity Value
{
  char allSensorsValue[80];
  lcd.clear();
  memset(allSensorsValue,'\0',sizeof(allSensorsValue));
  snprintf(allSensorsValue, sizeof(allSensorsValue), "I%s+",deviceId);
  
  memset(avgTemperatureValue,'\0',sizeof(avgTemperatureValue));
  memset(avgHumidityValue,'\0',sizeof(avgHumidityValue));
  DHT dht1(DHTPIN1, DHTTYPE);
  getTemperatureValue(0,dht1,allSensorsValue,atof(avgTemperatureValue),atof(avgHumidityValue));
  if (deviceSensorNumber > 1)
  {
    DHT dht2(DHTPIN2, DHTTYPE);
    getTemperatureValue(1,dht2,allSensorsValue,atof(avgTemperatureValue),atof(avgHumidityValue));
    if (deviceSensorNumber > 2)
    {
      DHT dht3(DHTPIN3, DHTTYPE);
      getTemperatureValue(2,dht3,allSensorsValue,atof(avgTemperatureValue),atof(avgHumidityValue));
      if (deviceSensorNumber > 3)
      {
        DHT dht4(DHTPIN4, DHTTYPE);
        getTemperatureValue(3,dht4,allSensorsValue,atof(avgTemperatureValue),atof(avgHumidityValue));
      }
    }
  }
  dtostrf((atof(avgTemperatureValue)/deviceSensorNumber),4,2,avgTemperatureValue);
  dtostrf((atof(avgHumidityValue)/deviceSensorNumber),4,2,avgHumidityValue);
//  strcat(allSensorsValue,"AT");
//  strcat(allSensorsValue,avgTemperatureValue);
//  strcat(allSensorsValue,",");
//  strcat(allSensorsValue,"AH");
//  strcat(allSensorsValue,avgHumidityValue);
//  strcat(allSensorsValue,"+");

  delay(2000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" Average Temperature");
  lcd.setCursor(7,1);
  lcd.print(avgTemperatureValue);
  lcd.print(" C");
  lcd.setCursor(0,2);
  lcd.print("  Average Humidity");
  lcd.setCursor(7,3);
  lcd.print(avgHumidityValue);
  lcd.print(" %");
  return allSensorsValue;
}
void doorStatusCheck()
{
  doorStatus = digitalRead(doorPin);
  if (doorStatus == 1)
  {
    digitalWrite(lcdPin1, LOW);
    digitalWrite(lcdPin2, HIGH);
  }
  else
  {
    digitalWrite(lcdPin2, LOW);
    digitalWrite(lcdPin1, HIGH);
  } 
}

void sendLastValue(byte doorCurrentStatus, char* dataToSendServer) // This Function is Used to Send Device Data
{
  strcat(dataToSendServer,"D");
  if (doorCurrentStatus == 1)
  {
    strcat(dataToSendServer,"1");
  }
  else
  {
    strcat(dataToSendServer,"0");
  }
//  Serial.println(dataToSendServer);
  answer = sendATcommand("AT+HTTPINIT", "OK", 10000);
  if (answer == 1)
  {
    answer = sendATcommand("AT+HTTPPARA=\"CID\",1", "OK", 5000);
    if (answer == 1)
    {
      char aux_str_local[200];
//      http://103.108.147.49/cms/public/addDetails/I1001+T24.60,H61.60+T26.10,H58.00+T25.10,H63.60+T25.00,H59.50+D0
//      snprintf(aux_str_local, sizeof(aux_str_local), "AT+HTTPPARA=\"URL\",\"http://%s?dev_id=%s&lat=90.4093738&lng=23.7914707&tem=%s&hum=%s&dlck=%d&pid=1\"",deviceAPI,deviceId,temperatureValue,humidityValue,doorCurrentStatus); 
      snprintf(aux_str_local, sizeof(aux_str_local), "AT+HTTPPARA=\"URL\",\"http://%s/%s\"",deviceAPI,dataToSendServer);     
      answer = sendATcommand(aux_str_local, "OK", 6000);
      if (answer == 1)
      {
        answer = sendATcommand("AT+HTTPACTION=0", "+HTTPACTION: 0,200", 20000);
        if (answer == 1)
        {
          initialValue = true;
          previousMillis = millis();
          doorPrevStatus = doorCurrentStatus;
          digitalWrite(lcdPin3, LOW);
        }
        sendATcommand("AT+SAPBR=0,1", "OK", 10000);
      }
      else
      {
        restartGSM();
      }
    }
    else
    {
      restartGSM();
    }
  } 
  else 
  {
    restartGSM();
  } 
  sendATcommand("AT+HTTPTERM", "OK", 5000);
}

void restartGSM() // Function to Shutdown the GSM
{
  digitalWrite(gsmPin, LOW);
  gsmONOFF = 0;
  gsmStatus = 0;
  delay(1000);
}

void turnOnGSM() // Function to Shutdown the GSM
{
  digitalWrite(gsmPin, HIGH);
  delay(15000);
  gsmONOFF = 1;
}

int getDeviceInfoFromEEPROM() // Function to check The Stored Values of EEPROM
{
  getDevicePassword();
  getDeviceAPI();
  dataSendingInterval = getTimeInterval();
  if (strlen(deviceAPI) > 0 && dataSendingInterval > 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int checkSMSToConfigure() // Function to Deal with SMS Commands
{
  char SMS[102];
  char message[100];
  if (gsmONOFF == 0)
  {
    turnOnGSM();
  }
  answer = sendATcommand("AT+CMGF=1", "OK", 1000);
  if (answer == 1)
  {
    answer = sendATcommand("AT+CMGL=\"ALL\"", "AT+CMGL=\"ALL\"", 15000);
    unsigned long previous = millis();
    if (answer == 1)
    {
      int smsNumber;
      memset(SMS,'\0',sizeof(SMS));
      answer = 0;
      byte smsCharCount = 0;
      while(mySerial.available() == 0);
      do
      {
        if(mySerial.available() > 0)
        {
          SMS[smsCharCount] = char(mySerial.read());
          if(SMS[smsCharCount] == '\n')
          {
            char *smsRestPart;
            if (strstr(SMS,"+CMGL:") != NULL)
            {
              strtok_r(SMS, ", ",&smsRestPart);
              smsNumber = atoi(strtok_r(smsRestPart, ", ",&smsRestPart));
              strtok_r(smsRestPart, ",",&smsRestPart);
              smsSIMNumber = strtok_r(smsRestPart, ", ",&smsRestPart);
            }
            else if(strstr(SMS,"CSM") != NULL) // Check The Initial Message Pattern
            {
              char *command;
              char *password;
              strtok_r(SMS,",",&smsRestPart);
              command = strtok_r(smsRestPart,",",&smsRestPart);
              if (strcmp(command,"Status") == 0) // Status Command Sends All The Necessary Values 
              {
                password = strtok_r(smsRestPart,",",&smsRestPart);
                if (strcmp(password,devicePassword) == 0)
                {
                  memset(message,'\0',sizeof(message));
                  if (doorStatus == 0)
                  {
                    snprintf(message, sizeof(message), "ID: %s\nAvg Temp: %sC\nAvg Hum: %s%%\nInterval: %dmin\nDoor Status : %s",deviceId,avgTemperatureValue,avgHumidityValue,dataSendingInterval,"Closed");
                  }
                  else
                  {
                    snprintf(message, sizeof(message), "ID: %s\nAvg Temp: %sC\nAvg Hum: %s%%\nInterval: %dmin\nDoor Status : %s",deviceId,avgTemperatureValue,avgHumidityValue,dataSendingInterval,"Open");
                  }
                  answer = sendReturnSms(message);
                  deleteSMS(smsNumber);
                  answer = 1;
                }
                else
                {
                  deleteSMS(smsNumber);
                  answer = 1;
                }             
              }
              else if(strcmp(command,"Interval") == 0) // Sets the Time Interval of Data Sending
              {
                char *interval = strtok_r(smsRestPart,",",&smsRestPart);
                password = strtok_r(smsRestPart,",",&smsRestPart);
                if (strcmp(password,devicePassword) == 0)
                {
                  messageFlag = setTimeInterval(atoi(interval));
                  if (messageFlag > 0)
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nInterval: %dmin\nNew Interval Set Successfully",deviceId,dataSendingInterval);
                  }
                  else
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nInterval: %dmin\nInvalid Time Interval",deviceId,dataSendingInterval);
                  }
                  answer = sendReturnSms(message);
                  deleteSMS(smsNumber);
                  answer = 1;
                }
                else
                {
                  deleteSMS(smsNumber);
                  answer = 1;
                }
              }
              else if(strcmp(command,"Sensor") == 0) // Sets the Time Interval of Data Sending
              {
                char *sensor = strtok_r(smsRestPart,",",&smsRestPart);
                password = strtok_r(smsRestPart,",",&smsRestPart);
                if (strcmp(password,devicePassword) == 0)
                {
                  messageFlag = setSensorNumber(atoi(sensor));
                  if (messageFlag > 0)
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nSensor Number: %d\nNew Sensor Number Set",deviceId,deviceSensorNumber);
                  }
                  else
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nSensor Number: %d\nInvalid Sensor Number",deviceId,deviceSensorNumber);
                  }
                  answer = sendReturnSms(message);
                  deleteSMS(smsNumber);
                  answer = 1;
                }
                else
                {
                  deleteSMS(smsNumber);
                  answer = 1;
                }
              }              
              else if(strcmp(command,"Password") == 0) // Sets the Password of the Device
              {
                char *newPassword = strtok_r(smsRestPart,",",&smsRestPart);
                password = strtok_r(smsRestPart,",",&smsRestPart);
                if (strcmp(password,devicePassword) == 0)
                {
                  messageFlag = setDevicePassword(newPassword);
                  if (messageFlag > 0)
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nPassword Changed Successfully",deviceId);
                  }
                  else
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nInvalid Password or Password Not Matched",deviceId);
                  }
                  answer = sendReturnSms(message);
                  deleteSMS(smsNumber);
                  answer = 1;
                }
                else
                {
                  deleteSMS(smsNumber);
                  answer = 1;
                }
              }
              else if(strcmp(command,"API") == 0) // Sets the API Address
              {
                char *newAPI = strtok_r(smsRestPart,",",&smsRestPart);
                password = strtok_r(smsRestPart,",",&smsRestPart);
                if (strcmp(password,devicePassword) == 0)
                {
                  messageFlag = setDeviceAPI(newAPI);
                  if (messageFlag > 0)
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nAPI Changed Successfully\nAPI: %s",deviceId,deviceAPI);
                  }
                  else
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nInvalid API or Password Not Matched",deviceId);
                  }
                  answer = sendReturnSms(message);
                  deleteSMS(smsNumber);
                  answer = 1;
                }
                else
                {
                  deleteSMS(smsNumber);
                  answer = 1;
                }
              }
              else if(strcmp(command,"Calibration") == 0) // Sets the Password of the Device
              {
                char *value = strtok_r(smsRestPart,",",&smsRestPart);
                char *symbol = strtok_r(smsRestPart,",",&smsRestPart);
                password = strtok_r(smsRestPart,",",&smsRestPart);
                if (strcmp(password,devicePassword) == 0)
                {
                  messageFlag = setCalibrationFactor(atoi(value),symbol[0]);
                  if (messageFlag > 0)
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nCalibration Done Successfully",deviceId);
                  }
                  else
                  {
                    memset(message,'\0',sizeof(message));
                    snprintf(message, sizeof(message), "ID: %s\nInvalid Calibration Request",deviceId);
                  }
                  answer = sendReturnSms(message);
                  deleteSMS(smsNumber);
                  answer = 1;
                }
                else
                {
                  deleteSMS(smsNumber);
                  answer = 1;
                }
              }
              else if(strcmp(command,"RESET") == 0) // RESET the Device to Send Data
              {
                password = strtok_r(smsRestPart,",",&smsRestPart);
                if (strcmp(password,devicePassword) == 0)
                {
                  initializeDevice();
                  memset(message,'\0',sizeof(message));
                  snprintf(message, sizeof(message), "ID: %s\nDevice is Reset",deviceId);
                  answer = sendReturnSms(message);
                  deleteSMS(smsNumber);
                  answer = 1;
                }
                else
                {
                  deleteSMS(smsNumber);
                  answer = 1;
                }
              }
              else if(strcmp(command,"SET") == 0) // SET the Device to Stop Sending Data
              {
                password = strtok_r(smsRestPart,",",&smsRestPart);
                if (strcmp(password,devicePassword) == 0)
                {
                  initializeDeviceSet();
                  memset(message,'\0',sizeof(message));
                  snprintf(message, sizeof(message), "ID: %s\nDevice is Set",deviceId);
                  answer = sendReturnSms(message);
                  deleteSMS(smsNumber);
                  answer = 1;
                }
                else
                {
                  deleteSMS(smsNumber);
                  answer = 1;
                }
              }
              else if(strcmp(command,"CURRENTPASSWORD") == 0) // Command to Get the Existing Passord
              {
                memset(message,'\0',sizeof(message));
                snprintf(message, sizeof(message), "ID: %s\nPassword: %s",deviceId,devicePassword);
                answer = sendReturnSms(message);
                deleteSMS(smsNumber);
                answer = 1;
              }
              else if(strcmp(command,"RESETPASSWORD") == 0) // Command to Reset the Device Password to 123456
              {
                setDevicePassword("123456");
                memset(message,'\0',sizeof(message));
                snprintf(message, sizeof(message), "ID: %s\nPassword Is Reset",deviceId);
                answer = sendReturnSms(message);
                deleteSMS(smsNumber);
                answer = 1;
              }            
            }
            smsCharCount = -1;
            memset(SMS,'\0',sizeof(SMS));
          }
          else if (smsCharCount > 100)
          {
            smsCharCount = -1;
            memset(SMS,'\0',sizeof(SMS));
          }
          else if (strcmp(SMS, "OK") == 0 )
          {
            answer = 1;
            deleteSMS(smsNumber);
          }
          else if (strcmp(SMS, "ERROR") == 0 )
          {
            answer = 1;
          }
          smsCharCount++;
        }
      }while((answer == 0) && ((millis() - previous) < 10000));    
    }
    else
    {
      restartGSM();
    }
  }
  else
  {
    restartGSM();
  }
  if (strlen(deviceAPI) > 0 && dataSendingInterval > 0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

void initializeDevice() // Function to RESET Device
{
  EEPROM.write(INITIALIZATIONSTATUSADDRESS, '0');
}
void initializeDeviceSet() // Function to SET Device
{
  EEPROM.write(INITIALIZATIONSTATUSADDRESS, '1');
}

byte getInitializationStatus() // Function to Check If the Device is in SET/RESET Mode
{
  if(EEPROM.read(INITIALIZATIONSTATUSADDRESS) == '1')
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int setSensorNumber(int sensorNumber) // Function to Sensor
{
  if (EEPROM.read(SENSORNUMBERADDRESS) == 255 || EEPROM.read(SENSORNUMBERADDRESS) == 0)
  {
    EEPROM.write(SENSORNUMBERADDRESS, char(1));
  }
  else if (sensorNumber > 0 && sensorNumber < 5)
  {
    EEPROM.write(SENSORNUMBERADDRESS, char(sensorNumber));
  }
  deviceSensorNumber = int(EEPROM.read(SENSORNUMBERADDRESS));
  return 1;
}

void getDevicePassword() // Function to Get Stored Password
{
  char getPassword[6];
  char ch;
  memset(getPassword, '\0', sizeof(getPassword));
  memset(devicePassword, '\0', sizeof(devicePassword));
  for (i = DEVICEPASSWORDADDRESS; i < EEPROM.length(); i++)
  {
    //Serial.println(i);
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    ch = EEPROM.read(i);
    getPassword[i-DEVICEPASSWORDADDRESS] = ch; 
  }
  if(strlen(getPassword) > 0)
  {
    strcpy(devicePassword,getPassword);
  }
  else
  {
    strcpy(devicePassword,"123456");
  }
}

void getDeviceAPI() // Funtion to Get Stored API Address
{
  char ch;
  memset(deviceAPI, '\0', sizeof(deviceAPI));
  for (i = DEVICEAPIADDRESS; i < EEPROM.length(); i++)
  {
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    ch = EEPROM.read(i);
    deviceAPI[i-DEVICEAPIADDRESS] = ch; 
  }
}

int setDeviceAPI(char* newAPI) // Function to Set API Address
{
  i = 0;
  for (i = 0; i < strlen(newAPI) ; i++)
  {
    if(newAPI[i] != " ")
    {
      continue;
    }
    else
    {
      return 0;
    }
  }
  for (i = DEVICEAPIADDRESS ; i < EEPROM.length() ; i++)
  {
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    EEPROM.write(i, 0);
  }
  for (i = 0; i < strlen(newAPI); i++)
  {
    EEPROM.write(i+DEVICEAPIADDRESS,newAPI[i]); 
  }
  strcpy(deviceAPI,newAPI);
  return 1;
}

int setDevicePassword(char* newPassword) // Function to Set Password
{
  i = 0;
  for (i = 0; i < strlen(newPassword) ; i++)
  {
    if(((newPassword[i] >= 'a' && newPassword[i] <= 'z') || (newPassword[i] >= 'A' && newPassword[i] <= 'Z') || (isdigit(newPassword[i]))) && i < 7)
    {
      continue;
    }
    else
    {
      return 0;
    }
  }
  for (i = DEVICEPASSWORDADDRESS ; i < EEPROM.length() ; i++)
  {
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    EEPROM.write(i, 0);
  }
  for (i = 0; i < strlen(newPassword); i++)
  {
    EEPROM.write(i+DEVICEPASSWORDADDRESS,newPassword[i]); 
  }
  strcpy(devicePassword,newPassword);
  return 1;
}

int setTimeInterval(int interval) // Function to Set Time Interval
{
  if (interval > 0 && interval < 10000)
  {
    char interValString[4];
    memset(interValString,'\0',sizeof(interValString));
    sprintf(interValString,"%d",interval);
    for (i = DEVICEINTERVALADDRESS ; i < EEPROM.length() ; i++)
    {
      if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
      {
        break;
      }
      EEPROM.write(i, 0);
    }
    for (i = 0;interValString[i] != '\0' ; i++)
    {
      EEPROM.write(i+DEVICEINTERVALADDRESS,interValString[i]); 
    }
    dataSendingInterval = interval;
    return 1;
  }
  else
  {
    return 0;
  }
}
int getTimeInterval() // Function to Get Time Interval
{
  char interValString[4];
  char ch;
  memset(interValString,'\0',sizeof(interValString));
  for (i = DEVICEINTERVALADDRESS; i < EEPROM.length(); i++)
  {
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    ch = EEPROM.read(i);
    interValString[i-DEVICEINTERVALADDRESS] = ch; 
  }
  return atoi(interValString);
}
byte setCalibrationFactor(byte calValue,char calSymbol)
{
  if (EEPROM.read(DEVICECALIBRATIONADDRESS) == 255 || EEPROM.read(DEVICECALIBRATIONADDRESS) == 0)
  {
    EEPROM.write(DEVICECALIBRATIONADDRESS, char(0));
    EEPROM.write(DEVICECALIBRATIONADDRESS+1, '+');
    calibrationValue = int(EEPROM.read(DEVICECALIBRATIONADDRESS));
    calibrationSymbol = char(EEPROM.read(DEVICECALIBRATIONADDRESS+1));
    return 1;
  }
  else if ((calValue > 0 && calValue < 10) && (calSymbol == '+' || calSymbol == '-'))
  {
    EEPROM.write(DEVICECALIBRATIONADDRESS, calValue);
    EEPROM.write(DEVICECALIBRATIONADDRESS+1, calSymbol);
    calibrationValue = int(EEPROM.read(DEVICECALIBRATIONADDRESS));
    calibrationSymbol = char(EEPROM.read(DEVICECALIBRATIONADDRESS+1));
    return 1;
  }
  return 0;
}

byte setNetworkGSM() // Function to Configure GSM Network
{
  i = 0;
  for(i = 0 ; i < 3 ; i++)
  {
    boolean gsmConnection = false;
    if(gsmONOFF == 0)
    {
      turnOnGSM();
    }
    byte j;
    for (j = 0 ; j < 2 ; j++)
    {
      if(sendATcommand("AT+CREG?", "+CREG: 0,1", 2000) != 0)
      {
        gsmConnection = true;
        break;
      }
    }
    if (gsmConnection == true)
    {
      sendATcommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", "OK", 2000);
      sendATcommand("AT+SAPBR=3,1,\"APN\",\"internet\"", "OK", 2000);
      sendATcommand("AT+SAPBR=3,1,\"USER\",\"\"", "OK", 2000);
      sendATcommand("AT+SAPBR=3,1,\"PWD\",\"\"", "OK", 2000);
      if(sendATcommand("AT+SAPBR=1,1", "OK", 25000) != 0)
      {
        return 1;
        break;
      }
    }
    restartGSM();
  }
  return 0;
}

void deleteSMS(byte smsNo) // Function to Delete Read SMSs
{
  if (smsNo > 0)
  {
    snprintf(aux_str,sizeof(aux_str),"AT+CMGD=%d",smsNo);
    sendATcommand(aux_str,"OK", 2000);
  }
}

int8_t sendReturnSms(char *message) // Function to Send Confirmation SMS
{
  snprintf(aux_str, sizeof(aux_str), "AT+CMGS=%s",smsSIMNumber.c_str());
  answer = sendATcommand(aux_str, ">", 2000);
  if (answer == 1)
  {
    mySerial.println(message);
    mySerial.write(0x1A);
    answer = sendATcommand("", "OK", 20000);
    if (answer == 1)
    {
        return 1;    
    }
  }
}

int8_t sendATcommand(char* ATcommand, char* expected_answer1, unsigned int timeout) // Function to Communicate with GSM
{
  uint8_t x=0;
  char response[50];
  unsigned long previous;
  answer = 0;
  memset(response,'\0',sizeof(response));// Initialize the string
  delay(100);
//  Serial.println(ATcommand);
  while( mySerial.available() > 0) mySerial.read();
  mySerial.println(ATcommand);    // Send the AT command 
  x = 0;
  previous = millis();
  // this loop waits for the answer
  do{
    if(mySerial.available() != 0)
    {    
      response[x] = mySerial.read();
      x++;
      // check if the desired answer is in the response of the module
      if (strstr(response, expected_answer1) != NULL)    
      {
        answer = 1;
      }
      else if(x == 50)
      {
        x = 0;
        memset(response,'\0',sizeof(response));
      }
    }
    // Waits for the answer with time out
  }
  while((answer == 0) && ((millis() - previous) < timeout));
//  Serial.println(response);
  return answer;
}

