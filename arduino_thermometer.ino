#include <sha1.h>
#include <Time.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/****** TEMPERATURE CONFIG ******/
char privateKey[] = "yourprivateKey";
String publicKey = "yourpublicKey";
char serverName[] = "sultry-ridge-5201.herokuapp.com";
char deviceName[] = "viabasse";
//last sensors read in seconds
unsigned long lastTempRead = 0;
//sensors read interval in seconds
unsigned long tempReadInterval = 1800;

/****** NTP CONFIG ******/
IPAddress timeServer(193,92,150,3); 
unsigned int ntpSyncTime = 15;
// local port to listen for UDP packets
unsigned int localPort = 8888;
// NTP time stamp is in the first 48 bytes of the message
const int NTP_PACKET_SIZE= 48;
// Buffer to hold incoming and outgoing packets
byte packetBuffer[NTP_PACKET_SIZE];
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;
// Keeps track of how long ago we updated the NTP server
unsigned long ntpLastUpdate = 0;
// sync interval in seconds
unsigned long ntpIntervalSync = 43200;

/****** ARDUINO ETHERNET CONFIG ******/
//mac address
byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x01 };
//arduino ip address
IPAddress ip(192,168,0,20);
//client
EthernetClient client;

/****** ONEWIRE ETHERNET CONFIG ******/
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
  
  //sync time via NTP
  syncTimeNTP(15);
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
int sendToServer(String URL, String hash)
{
  // attempt to connect
  if (client.connect(serverName, 80))
  {
    Serial.println("making HTTP request...");

    client.println(URL);
    client.println("Host: sultry-ridge-5201.herokuapp.com");
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println("ARD-HASH: "+hash);
    client.println("ARD-APIKEY: "+publicKey);
    client.println();
  }
  else
  {
    Serial.println("error");
    return 0;
  }
  
  int connectLoop = 0;
  int inChar = 0;
  while(client.connected())
  {
    while(client.available())
    {
      inChar = client.read();
      Serial.write(inChar);
      connectLoop = 0;
    }
    
    delay(1);
    
    connectLoop++;
    if(connectLoop > 10000)
    {
      Serial.println();
      Serial.println(F("Timeout inside while"));
      client.stop();
    }
  }
  
  Serial.println();
  Serial.println(F("Timeout outside while"));
  client.stop();
  return 1;
}

float readTemperature(DeviceAddress device)
{
  sensors.requestTemperatures();
  return sensors.getTempC(device);
}

void loop()
{
  //sensors read
  if(lastTempRead + tempReadInterval < now())
  {
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
    
    lastTempRead = now();
  }
  
  //time sync
  if(ntpLastUpdate + ntpIntervalSync < now())
  {
    if(syncTimeNTP(10))
    {
      Serial.println("time updated");
    }
    else
    {
      Serial.println("cannot sync time");
    }
  }
}

/***** NTP FUNCTIONS *****/
int syncTimeNTP(int numberOfTrys)
{
  //Try to get the date and time
  int trys = 0;
  while(!getTimeAndDate() && trys<numberOfTrys)
  {
    trys++;
  }
  
  if(trys < 10)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}


// Do not alter this function, it is used by the system
int getTimeAndDate()
{
  int flag=0;
  Udp.begin(localPort);
  sendNTPpacket(timeServer);
  delay(1000);
  if(Udp.parsePacket())
  {
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer
    unsigned long highWord, lowWord, epoch;
    highWord = word(packetBuffer[40], packetBuffer[41]);
    lowWord = word(packetBuffer[42], packetBuffer[43]);  
    epoch = highWord << 16 | lowWord;
    epoch = epoch - 2208988800;
    flag=1;
    setTime(epoch);
    ntpLastUpdate = now();
  }
  return flag;
}

// Do not alter this function, it is used by the system
unsigned long sendNTPpacket(IPAddress& address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;		   
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket(); 
}
