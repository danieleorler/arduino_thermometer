#include <sha1.h>
#include <Time.h>
#include <SPI.h>
#include <Ethernet.h>
#include <OneWire.h>
#include <DallasTemperature.h>

char privateKey[] = "yourPrivateKey";
String publicKey = "yourPublicKey";
char serverName[] = "sultry-ridge-5201.herokuapp.com";
long delay_ms = 1800000;
String deviceName = "viabasse";


//mac address
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x01 };
//ip address
IPAddress ip(192,168,0,20);
//client
EthernetClient client;

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 2

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer, outsideThermometer;

void setup(void)
{
  Serial.begin(9600);
  
  //Setup sensors
  sensors.begin();
  sensors.getAddress(insideThermometer, 0);
  sensors.setResolution(insideThermometer, 9);
  
  sensors.getAddress(outsideThermometer, 1);
  sensors.setResolution(outsideThermometer, 9);
  
  //setup ethernet
  if(!Ethernet.begin(mac))
  {
    Ethernet.begin(mac, ip);
  }
}


/*
* creates hash
*/
uint8_t* createHash(String stringToHash)
{
  Sha1.init();
  Sha1.print(stringToHash);
  return Sha1.result();
}

/*
* convert hash to String
*/
String hashToString(uint8_t* hash)
{
  int hashIndex = 0;
  char hashResult[40];
  
  for (int i=0; i<20; i++)
  {
    hashResult[hashIndex++] = "0123456789abcdef"[hash[i]>>4];
    hashResult[hashIndex++] = "0123456789abcdef"[hash[i]&0xf];
  }
  return String(hashResult);
}

/*
* concatenates parameters
*/
String concatenateParameters(char* pk, char* device, char* sensor, float temperature)
{
  char joined[50];
  sprintf(joined,"%s%s%s%d",pk,device,sensor,(int)(temperature*100));
  return String(joined);
}

/*
* creates URL
*/
String createURL(char* device, char* sensor, float temperature)
{
  char URL[100];
  sprintf(URL,"GET /rest/survey/insert?device=%s&sensor=%s&temperature=%d HTTP/1.1",device,sensor,(int)(temperature*100));
  return String(URL);
}

/*
* sends data to the webservice
*/
void sendToServer(String URL, String hash)
{
  // attempt to connect, and wait a millisecond:
  if (client.connect(serverName, 80))
  {
    Serial.println("making HTTP request...");

    client.println(URL);
    client.println("Host: sultry-ridge-5201.herokuapp.com");
    client.println("User-Agent: arduino-ethernet");
    client.println("ARD-HASH: "+hash);
    client.println("ARD-APIKEY: "+publicKey);
    //client.println("Connection: close");
    client.println();
    
    while(!client.available()){
        delay(1);
    }

   /*while(client.available()) {
        char c = client.read();
        Serial.print(c);
    }*/
    
    client.stop();
  }
  else
  {
    Serial.println("error");
  }
  // note the time of this connect attempt:
  //lastAttemptTime = millis();
}

float readTemperature(DeviceAddress device)
{
  sensors.requestTemperatures();
  return sensors.getTempC(device);
}

void loop() {
  
  String concatenatedParams;
  String hashResult;
  
  float temp_in = readTemperature(insideThermometer);
  float temp_out = readTemperature(outsideThermometer);
  
  concatenatedParams = concatenateParameters(privateKey,deviceName,"in",temp_in);
  hashResult = hashToString(createHash(concatenatedParams)).substring(0,40);
  sendToServer(createURL(deviceName,"in",temp_in),hashResult);

  concatenatedParams = concatenateParameters(privateKey,deviceName,"out",temp_out);
  hashResult = hashToString(createHash(concatenatedParams)).substring(0,40);
  sendToServer(createURL(deviceName,"out",temp_out),hashResult);

  delay(delay_ms);
}
