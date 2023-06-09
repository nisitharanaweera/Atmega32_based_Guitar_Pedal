#include <Arduino.h>
#include <time.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>

const char* SSID = "RAND Sams";
const char* PSK = "12345678";
const char* DEPLOYMENT_ID = "AKfycbw_3BraA5hwAA4OQ-Rl21iqGlYVbHHDpi4AeWnbayoMZTJ-PmDI6iYigr3kJ45zSd61";
const unsigned char INTERVAL_SECS = 60;
//initiating the variable pins d
const unsigned char BMP_SCL = 22;
const unsigned char BMP_SDA = 21;
const unsigned char DHT_PIN = 18;
const unsigned char CLR_PIN = 19;

//asd
unsigned long chanelId = 1874223;
const char* rightAPIkey = "COBWKAR68XHIL8AR";

HTTPClient http;
DHT dht(DHT_PIN, DHT11);
Adafruit_BMP085 bmp;

// initiating a Structure to hold the data values
struct Record {
    unsigned int uuid;
    unsigned long sentTimestamp;
    double tempDHT;
    double humidity;
    double pressure;
    double altitude;
    double seaLevelPressure;
};

//reading data from the storage
String readFile(String path) {
    File file = SPIFFS.open(path);
    String content = file.readString();
    content.trim();
    file.close();
    return content;
}
//writing data to the flies
void writeFile(String path, String content) {
    File file = SPIFFS.open(path, FILE_WRITE);
    file.println(content);
    file.close();
}

void writeRecord(Record record) {
    String content = "";
    content += "?1=";
    content += record.uuid;
    content += "&2=";
    content += record.sentTimestamp;
    content += "&3=";
    content += record.tempDHT;
    content += "&4=";
    content += record.humidity;
    content += "&5=";
    content += record.pressure;
    content += "&6=";
    content += record.seaLevelPressure;
    content += "&7=";

    String filePath = "/recData/" + String(record.uuid) + ".txt";
    writeFile(filePath, content);
    Serial.println("WRITTEN: " + content + " TO: " + filePath);
}
//Http request
int sendRequest(String url) {
    Serial.println("\nSENDING: " + url);
    http.begin(url.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    Serial.println("HTTP CODE: " + String(httpCode));
    if (httpCode > 0) {
        Serial.println("RECEIVED: " +  http.getString());
    }
    http.end();
    return httpCode;
}

void sleepDeep(int secs) {
    Serial.println("\nINITIATING: Deep sleep FOR: " + String(secs) + "s" );
    esp_sleep_enable_timer_wakeup(secs * 1000000);
    
    Serial.println("================================================================");
    
    esp_deep_sleep_start();//initiate deep sleep
}

RTC_DATA_ATTR bool timeConfigured = false;

Record generateRecord() {
    
    Record record;

    unsigned int uuid;
    unsigned long sentTimestamp;
    double tempDHT;
    double humidity;
    double pressure;
    double altitude;
    double seaLevelPressure;
    
    record.tempDHT = dht.readTemperature();
    record.humidity = dht.readHumidity();
    record.pressure = bmp.readPressure();
    record.altitude = bmp.readAltitude();
    record.seaLevelPressure = bmp.readSealevelPressure();

    //timestamp initiation
    long lastTimestamp;
    if (timeConfigured) {
        lastTimestamp = time(NULL);
    } else {
        lastTimestamp = readFile("/timestamp_last.txt").toInt() + INTERVAL_SECS;
    }
    record.sentTimestamp = lastTimestamp;
    writeFile("/timestamp_last.txt", String(lastTimestamp));

    //Assign UUID
    int nextUuid = readFile("/uuid_next.txt").toInt();
    record.uuid = nextUuid;
    writeFile("/uuid_next.txt", String(nextUuid+1));

    return record;
}
WiFiClient client;
void setup() {
    //Serial initiate
    Serial.begin(115200);
    Serial.println("\n===============================================================");
    Serial.println("CONFIGURED: Serial communication AT: 115200");

    //SPIFFS initiation
    SPIFFS.begin(true);
    if (digitalRead(CLR_PIN) == HIGH) {
        Serial.println("INITIATING: Memory wipe BECAUSE: Memory wipe pin is high");

        if (SPIFFS.remove("/uuid_next.txt")) {
            Serial.println("REMOVED: /uuid_next.txt");
        }
        if (SPIFFS.remove("/timestamp_last.txt")) {
            Serial.println("REMOVED: /timestamp_last.txt");
        }
        File recordsDir = SPIFFS.open("/recData");
        File nextRecordFile;
        
        if (recordsDir) {
            while (nextRecordFile = recordsDir.openNextFile()) {
                String nextRecordFilePath = "/recData/" + String(nextRecordFile.name());
                if (SPIFFS.remove(nextRecordFilePath)) {
                    Serial.println("REMOVED: " + nextRecordFilePath);
                }
            }
        }
        
        Serial.println("CONFIGURED: Storage DO: Reset chip");
        while(true);
    } else {
        if (!SPIFFS.exists("/uuid_next.txt")) {
            writeFile("/uuid_next.txt", "0");
            Serial.println("WRITTEN: /uuid_next.txt CONTENT: 0 BECAUSE: File not found");
        }
        if (!SPIFFS.exists("/timestamp_last.txt")) {
            writeFile("/timestamp_last.txt", "0");
            Serial.println("WRITTEN: /timestamp_last.txt CONTENT: 0 BECAUSE: File not found");
        }
        if (!SPIFFS.open("/recData")) {
            Serial.println("CREATED: /recData BECAUSE: Directory not found");
        }
        Serial.println("CONFIGURED: Storage");
    }

    //IO pins
    pinMode(LDR_PIN, INPUT);
    pinMode(DHT_PIN, INPUT);
    pinMode(CLR_PIN, INPUT);
    Serial.println("CONFIGURED: IO");

    //DHT11 sensor initiation
    dht.begin();
    Serial.println("CONFIGURED: DHT11");

    //BMP180 sensor initiation
    if (bmp.begin()) {
        Serial.println("CONFIGURED: BMP11");
    } else {
        Serial.println("FAILED: BMP11");
    }


    //Create record
    writeRecord(generateRecord());

    //WiFi connection status
    Serial.print("CONFIGURING: WiFi WITH: " + String(SSID) + " ");
    unsigned char wiFiConnectTimeLeft = 20;
    WiFi.begin(SSID, PSK);
    while (WiFi.status() != WL_CONNECTED && wiFiConnectTimeLeft > 0) {
        Serial.print(".");
        wiFiConnectTimeLeft--;
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nCONFIGURED: WiFi WITH: " + String(SSID));

        if (!timeConfigured || time(NULL) < 1663485411) {
            configTime(19800, 0, "pool.ntp.org");
            timeConfigured = true;
            Serial.println("CONFIGURED: System time WITH: pool.ntp.org BECAUSE: Time not configured to time is wrong");
        }
        File recordsDir = SPIFFS.open("/recData");
        File recordFile;
        
        while (WiFi.status() == WL_CONNECTED && (recordFile = recordsDir.openNextFile())) {
            if(!recordFile.isDirectory()){
                String recordFilePath = "/recData/" + String(recordFile.name());
                String url = "https://script.google.com/macros/s/" + String(DEPLOYMENT_ID) + "/exec" + readFile(recordFilePath);
                int httpCode = sendRequest(url);
                if (httpCode == 200) {
                    SPIFFS.remove(recordFilePath);
                }
            }
        }
    } else {
        Serial.println("\nERR: Configuring WiFi WITH: " + String(SSID) + " BECAUSE: Timed out");
    }
    ThingSpeak.begin(client);  
}

void loop() {
  Record record = generateRecord();

 float tempDht = record.tempDHT;
 float humid = record.humidity;
 float pressure = record.pressure;
 float alt = record.altitude;


  ThingSpeak.setField(1,tempDht);
  ThingSpeak.setField(2,humid);
  ThingSpeak.setField(3,pressure);
  ThingSpeak.setField(4,alt);

  int respons = ThingSpeak.writeFields(chanelId,rightAPIkey);
 

  if (respons == 200) {
    Serial.println("Data upload sucessful");

  } else {
    Serial.println("Data upload unsucessful");

  }
    sleepDeep(INTERVAL_SECS);
}