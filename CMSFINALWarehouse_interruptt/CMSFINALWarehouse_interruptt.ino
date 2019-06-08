#include "DHT.h"
#define DHTPIN 4 // Temp/Hum Sensor is Connected to Pin 4
#define doorPin 2 // Door Sensor is Connected to Pin 2
#define lcdPin1 7 //Door Open Indicator
#define lcdPin2 10 //Door Close Indicator
#define lcdPin3 13  //Data Sending Indicator
#define DHTTYPE DHT22 

DHT dht(DHTPIN, DHTTYPE); // Tem/Hum Library
char temperatureValue[6]; // Arrary to Store Temperature Value
char humidityValue[6]; // Array to Store Humidity Value
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
char *deviceId = "CM004";
byte dataSendingInterval = 0;
char devicePassword[7];
char deviceAPI[50];
byte confStatus = 0; // Check if the Device is Configured or Not
byte initStatus = 0; // Check if the Device is SET to Send Data
byte i = 0;
#include <LiquidCrystal.h>
LiquidCrystal lcd(A3, A2, A1, A0, 12, 11);
void setup() {
  //Serial.begin(9600);
  mySerial.begin(9600);
  lcd.begin(16, 2);
  pinMode(gsmPin, OUTPUT);
  pinMode(lcdPin1, OUTPUT);
  pinMode(lcdPin2, OUTPUT);
  pinMode(lcdPin3, OUTPUT);
  pinMode(doorPin,INPUT_PULLUP);
  dht.begin();
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
  delay(1000);
  readTempHumSensor();
  printingMessage();
  float difference = float(float((millis() - previousMillis)/1000)/60); // Calculate Time Difference
  if((confStatus == 1 && initStatus == 0) && ((difference > dataSendingInterval) || (initialValue == false) || (doorStatus != doorPrevStatus)))
  {
    gsmStatus = setNetworkGSM();
    if (gsmStatus == 1)
    {
      digitalWrite(lcdPin3, HIGH);
      sendLastValue(doorStatus);
    }
  }
  confStatus = checkSMSToConfigure();
  initStatus = getInitializationStatus();
  delay(1000);
}

void printingMessage()
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Temp : ");
  lcd.print(temperatureValue);
  lcd.print(" C");
  lcd.setCursor(0,1);
  lcd.print("Hum  : ");
  lcd.print(humidityValue);
  lcd.print(" %");
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

void sendLastValue(byte doorCurrentStatus) // This Function is Used to Send Device Data
{
  answer = sendATcommand("AT+HTTPINIT", "OK", 10000);
  if (answer == 1)
  {
    answer = sendATcommand("AT+HTTPPARA=\"CID\",1", "OK", 5000);
    if (answer == 1)
    {
      char aux_str_local[150];
      snprintf(aux_str_local, sizeof(aux_str_local), "AT+HTTPPARA=\"URL\",\"http://%s?dev_id=%s&lat=90.4093738&lng=23.7914707&tem=%s&hum=%s&dlck=%d&pid=1\"",deviceAPI,deviceId,temperatureValue,humidityValue,doorCurrentStatus);      
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

void readTempHumSensor() // Function to Check Temperature + Humidity Value
{
  memset(temperatureValue, '\0', sizeof(temperatureValue));
  memset(humidityValue, '\0', sizeof(temperatureValue));
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity))
  {
    temperature = 0;
    humidity = 0;
  }
  dtostrf(temperature,4,2,temperatureValue);
  dtostrf(humidity,4,2,humidityValue);
}

int checkSMSToConfigure() // Function to Deal with SMS Commands
{
  char SMS[102];
  char message[110];
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
            else if(strstr(SMS,"CMS") != NULL) // Check The Initial Message Pattern
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
                    snprintf(message, sizeof(message), "ID: %s\nTemp: %sC\nHum: %s%%\nInterval: %dmin\nDoor Status : %s\nAPI: %s",deviceId,temperatureValue,humidityValue,dataSendingInterval,"Closed",deviceAPI);
                  }
                  else
                  {
                    snprintf(message, sizeof(message), "ID: %s\nTemp: %sC\nHum: %s%%\nInterval: %dmin\nDoor Status : %s\nAPI: %s",deviceId,temperatureValue,humidityValue,dataSendingInterval,"Open",deviceAPI);
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
  EEPROM.write(1, '0');
}
void initializeDeviceSet() // Function to SET Device
{
  EEPROM.write(1, '1');
}

byte getInitializationStatus() // Function to Check If the Device is in SET/RESET Mode
{
  if(EEPROM.read(1) == '1')
  {
    return 1;
  }
  else
  {
    return 0;
  }
}
void getDevicePassword() // Function to Get Stored Password
{
  char getPassword[6];
  char ch;
  memset(getPassword, '\0', sizeof(getPassword));
  memset(devicePassword, '\0', sizeof(devicePassword));
  for (i = 100; i < EEPROM.length(); i++)
  {
    //Serial.println(i);
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    ch = EEPROM.read(i);
    getPassword[i-100] = ch; 
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
  for (i = 150; i < EEPROM.length(); i++)
  {
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    ch = EEPROM.read(i);
    deviceAPI[i-150] = ch; 
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
  for (i = 150 ; i < EEPROM.length() ; i++)
  {
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    EEPROM.write(i, 0);
  }
  for (i = 0; i < strlen(newAPI); i++)
  {
    EEPROM.write(i+150,newAPI[i]); 
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
  for (i = 100 ; i < EEPROM.length() ; i++)
  {
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    EEPROM.write(i, 0);
  }
  for (i = 0; i < strlen(newPassword); i++)
  {
    EEPROM.write(i+100,newPassword[i]); 
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
    for (i = 50 ; i < EEPROM.length() ; i++)
    {
      if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
      {
        break;
      }
      EEPROM.write(i, 0);
    }
    for (i = 0;interValString[i] != '\0' ; i++)
    {
      EEPROM.write(i+50,interValString[i]); 
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
  for (i = 50; i < EEPROM.length(); i++)
  {
    if(EEPROM.read(i) == 255 || EEPROM.read(i) == 0)
    {
      break;
    }
    ch = EEPROM.read(i);
    interValString[i-50] = ch; 
  }
  return atoi(interValString);
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
  char response[200];
  unsigned long previous;
  answer = 0;
  memset(response, '\0', 200);    // Initialize the string
  delay(100);
  //Serial.println(ATcommand);
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
    }
    // Waits for the answer with time out
  }
  while((answer == 0) && ((millis() - previous) < timeout));
  //Serial.println(response);
  return answer;
}

