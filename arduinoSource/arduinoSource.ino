//DHT11 예시(온습도) : https://github.com/RobTillaart/Arduino/tree/master/libraries/DHTlib
//CO2 예시 : http://www.dfrobot.com/wiki/index.php/CO2_Sensor_SKU:SEN0159
//와이파이 쉴드 응용 : http://kocoafab.cc/tutorial/view/108

#include <dht.h>
#include <SPI.h>
#include <WiFi.h>

#include <Time.h>  
#include <Wire.h>  
#include <DS1307RTC.h> 

dht DHT;

char ssid[] = "ССИД";      //  연결하실 와이파이 SSID
char pass[] = "пароль";   // 네트워크 보안키

int status = WL_IDLE_STATUS;
WiFiServer server(80);  // 80포트를 사용하는 웹서버 선언

#define DHT11_PIN 4
#define         MG_PIN                       (0)     //define which analog input channel you are going to use
#define         BOOL_PIN                     (2)
#define         DC_GAIN                      (8.5)   //define the DC gain of amplifier
#define         READ_SAMPLE_INTERVAL         (50)    //define how many samples you are going to take in normal operation
#define         READ_SAMPLE_TIMES            (5)     //define the time interval(in milisecond) between each samples in 
#define         ZERO_POINT_VOLTAGE           (0.324) //define the output of the sensor in volts when the concentration of CO2 is 400PPM
#define         REACTION_VOLTGAE             (0.020) //define the voltage drop of the sensor when move the sensor from air into 1000ppm CO2
float           CO2Curve[3]  =  {2.602,ZERO_POINT_VOLTAGE,(REACTION_VOLTGAE/(2.602-3))};   

void setup()
{
  //시리얼 모니터에서 115200 보드 레이트로 하지 않으면
  //문자가 깨져 나온다.
  Serial.begin(115200);

  if(timeStatus()!= timeSet) 
     Serial.println("Unable to sync with the RTC");
  else
     Serial.println("RTC has set the system time"); 
  
  pinMode(BOOL_PIN, INPUT);                        //set pin to input
    digitalWrite(BOOL_PIN, HIGH);                    //turn on pullup resistors

  
   if (WiFi.status() == WL_NO_SHIELD) { // 현재 아두이노에 연결된 실드를 확인
    Serial.println("WiFi shield not present"); 
    while (true);  // 와이파이 실드가 아닐 경우 계속 대기
  } 

  // 와이파이에 연결 시도
  while ( status != WL_CONNECTED) { //연결될 때까지 반복
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);  // WPA/WPA2 연결
  } 

  server.begin();

  printWifiStatus();  // 연결 성공시 연결된 네트워크 정보를 출력
  
  Serial.println("Type,\tstatus,\tHumidity (%),\tTemperature (C)");
  //서버 상에 온습도 정보 출력
}

void loop()
{
  
  int chk = DHT.read11(DHT11_PIN);//온습도 정보를 센서로 부터 가져옴
  //센서 연결 상태옴
   int percentage;
    float volts;
    volts = MGRead(MG_PIN);//이산화탄소 농도를 센서로 부터 가져옴
    percentage = MGGetPercentage(volts,CO2Curve);
  
  WiFiClient client = server.available();  // 들어오는 클라이언트를 수신한다.
  if (client) {  // 클라이언트를 수신 시
    Serial.println("new client");  // 클라이언트 접속 확인 메시지 출력
    boolean currentLineIsBlank = true;

    while (client.connected ()) { 
      if (client.available()) {
        char c = client.read();
        // 문자의 끝을 입력 받으면 http 요청이 종료되고, 답신을 보낼 수 있습니다.
        if (c == '\n' && currentLineIsBlank) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println("Refresh: 1"); // 1초당 페이지 refresh
          
          client.println();
         
          
          client.print("{");
         
          client.print("\"temperature\":\"");
          client.print(DHT.temperature);//온도 출력
          client.print("\"");

          client.print(",");

          client.print("\"humidity\":\"");
          client.print(DHT.humidity);//습도 출력
          client.print("\"");

          client.print(",");

          client.print("\"co2\":\"");
          if (percentage == -1) {
              client.println("400");
          } else {
              client.println(percentage);//측정된 이산화탄소 농도 출력
          }
          client.print("\"");

          client.print("}");
          
          break;
        }
        if (c == '\n') { 
          currentLineIsBlank = true;
        }

        else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
    delay(1);
    client.stop();
    Serial.println("client disonnected");
    // 클라이언트와 연결을 끊는다.
  }
  
}

void printWifiStatus() {  // 연결된 네트워크 정보 출력
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  // 네트워크 SSID 출력

  IPAddress ip = WiFi.localIP(); 
  Serial.print("IP Address: ");
  Serial.println(ip);
  // 네트워크 ip 출력

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // 수신 강도 출력
}



/*****************************  MGRead *********************************************
Input:   mg_pin - analog channel
Output:  output of SEN-000007
Remarks: This function reads the output of SEN-000007
************************************************************************************/
float MGRead(int mg_pin)
{
    int i;
    float v=0;
 
    for (i=0;i<READ_SAMPLE_TIMES;i++) {
        v += analogRead(mg_pin);
        delay(READ_SAMPLE_INTERVAL);
    }
    v = (v/READ_SAMPLE_TIMES) *5/1024 ;
    return v;  
}
 
/*****************************  MQGetPercentage **********************************
Input:   volts   - SEN-000007 output measured in volts
         pcurve  - pointer to the curve of the target gas
Output:  ppm of the target gas
Remarks: By using the slope and a point of the line. The x(logarithmic value of ppm) 
         of the line could be derived if y(MG-811 output) is provided. As it is a 
         logarithmic coordinate, power of 10 is used to convert the result to non-logarithmic 
         value.
************************************************************************************/
int  MGGetPercentage(float volts, float *pcurve)
{
   if ((volts/DC_GAIN )>=ZERO_POINT_VOLTAGE) {
      return -1;
   } else { 
      return pow(10, ((volts/DC_GAIN)-pcurve[1])/pcurve[2]+pcurve[0]);
   }
}


//
// END OF FILE
//

