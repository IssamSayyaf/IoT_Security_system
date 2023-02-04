    #include <WiFiClientSecure.h>
    #include <PubSubClient.h>
    #include <ArduinoJson.h>
    #include <LiquidCrystal_I2C.h>
    #include <string.h>
    #include "WiFi.h"
    #include "DHTesp.h"

    #define THINGNAME "ESP32_001"                       
    char MSG[10];
    const char WIFI_SSID[] = "";              
    const char WIFI_PASSWORD[] = "";
    //Digital twin name         
    const char AWS_IOT_ENDPOINT[] = "";      
 
    LiquidCrystal_I2C lcd_i2c(0x27, 16, 2); // I2C address 0x27, 16 column and 2 rows

// Amazon Root CA 1
static const char AWS_CERT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
-----END CERTIFICATE-----
)EOF";
 
// Device Certificate                                               
static const char AWS_CERT_CRT[] PROGMEM = R"KEY(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
 
 
)KEY";
 
// Device Private Key                                               //change this
static const char AWS_CERT_PRIVATE[] PROGMEM = R"KEY(
-----BEGIN RSA PRIVATE KEY-----
-----END RSA PRIVATE KEY-----
 
 
)KEY";






    #define Alarm_pin   2 //red led
    #define Fire_Pin  14
    #define dhtPin    13     // Digital pin connected to the DHT sensor
    #define DHTTYPE DHT11   // DHT 11
    DHTesp dht;
    #define AWS_IOT_PUBLISH_TOPIC   "esp32/pub"
    #define AWS_IOT_SUBSCRIBE_TOPIC "esp32/sub"
    const int analogOutPin = 5; //heating system PWM
    float h ;
    float t;
    volatile byte Alarm_fire=0;
    volatile byte Alarm_timer=0;
    volatile byte flag=0;
    hw_timer_t * timer = NULL;
    portMUX_TYPE synch = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;   
    void IRAM_ATTR onTimer(){
      portENTER_CRITICAL_ISR(&timerMux);
      Alarm_timer=1;
      portEXIT_CRITICAL_ISR(&timerMux);
    }

      void fire_Alarm(){
        portENTER_CRITICAL(&synch);
        Alarm_fire =1;       
        portEXIT_CRITICAL(&synch);

    }    
    
    WiFiClientSecure net = WiFiClientSecure();
    PubSubClient client(net);

    void connect_WiFi(){
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
      Serial.println("Connecting to Wi-Fi");
    
      while (WiFi.status() != WL_CONNECTED)
      {
        
        delay(500);
        Serial.print(".");
        
      }
    }   
    
    void connectAWS()
    {

      
      // Configure WiFiClientSecure to use the AWS IoT device credentials
      net.setCACert(AWS_CERT_CA);
      net.setCertificate(AWS_CERT_CRT);
      net.setPrivateKey(AWS_CERT_PRIVATE);
    
      // Connect to the MQTT broker on the AWS endpoint we defined earlier
      client.setServer(AWS_IOT_ENDPOINT, 8883);
    
      // Create a message handler
      client.setCallback(messageHandler);
    
      Serial.println("Connecting to AWS IOT");
    
      while (!client.connect(THINGNAME))
      {
        Serial.print(".");
        delay(100);
      }
    
      if (!client.connected())
      {
        Serial.println("AWS IoT Timeout!");
        return;
      }
    
      // Subscribe to a topic
      client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
    
      Serial.println("AWS IoT Connected!");
    }
    
    void publishMessage()
    {
      StaticJsonDocument<200> doc;
      doc["humidity"] = h;
      doc["temperature"] = t;
      doc["Device_ID"]="ESP32_001";
      doc["Alarm"]=Alarm_fire;   
      if(Alarm_fire==1)         
        Alarm_fire=2;
      char jsonBuffer[512];
      serializeJson(doc, jsonBuffer); // print to client
    
      client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
    }
    
    void messageHandler(char* topic, byte* payload, unsigned int length)
    {
      Serial.print("incoming: ");
      Serial.println(topic);
    
      StaticJsonDocument<200> doc;
      deserializeJson(doc, payload);
      String message = doc["Heating"];
      int outputValue = message.toInt();
      analogWrite(analogOutPin, outputValue);
      Serial.println(message);
    }
    
    void setup()
    {
      lcd_i2c.init(); // initialize the lcd
      lcd_i2c.backlight();
      lcd_i2c.clear();              // clear display
      lcd_i2c.setCursor(0, 0);      // move cursor to   (0, 0)
      lcd_i2c.print("***Connecting***");       // print message at (0, 0)
      Serial.begin(115200);
      connect_WiFi();
      lcd_i2c.clear();   
      connectAWS();
      pinMode(Alarm_pin,OUTPUT);
      dht.setup(dhtPin, DHTesp::DHT11);
      attachInterrupt(Fire_Pin,fire_Alarm,FALLING);
      timer = timerBegin(0,80,true);
      timerAttachInterrupt(timer,&onTimer,true);
      timerAlarmWrite(timer,10000000,true);
      timerAlarmEnable(timer);      
    }
    
    void loop()
    {
      
      while (WiFi.status() != WL_CONNECTED)
      {
        flag=1;
        WiFi.reconnect();
        delay(500);
      }
      if (flag==1){
        connectAWS();
        flag=0;
      }
      if(Alarm_timer==1){
        measure();
      }
            
    }
    void measure(){
            TempAndHumidity newValues = dht.getTempAndHumidity();
            h = newValues.humidity;
            t = newValues.temperature; 
          
            Alarm_timer=0;
          
            if (isnan(h) || isnan(t) )  // Check if any reads failed and exit early (to try again).
            {
              Serial.println(F("Failed to read from DHT sensor!"));
              return;
            }

            if(Alarm_fire==1)
              digitalWrite(Alarm_pin,HIGH);

            Serial.print(F("Humidity: "));
            Serial.print(h);
            Serial.print(F("%  Temperature: "));
            Serial.print(t);
            Serial.println(F("°C "));
            sprintf(MSG,"Temp=%.2f",t);     
            lcd_i2c.clear();              // clear display
            lcd_i2c.setCursor(0, 0);      // move cursor to   (0, 0)
            lcd_i2c.print(MSG);  
            sprintf(MSG,"Hum =%.2f",h);                // clear display
            lcd_i2c.setCursor(0, 1);      // move cursor to   (0, 1)
            lcd_i2c.print(MSG); 
            publishMessage();
            client.loop();
      
    }
