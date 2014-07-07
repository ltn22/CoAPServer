#include <SPI.h>        
#include <Timer.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <CoAPServer.h>dump



enum SMLED {SM1_LOW, SM1_HIGH} LED ;

int sensorPin = A0; 
int sensorValue = 0;
float meanValue;
float alpha = 0.1;
uint32_t nbFlash = 0;
int nbLow = 0;

byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x6F, 0x30 };
unsigned int localPort = 5683;      // CoAP Default port
EthernetUDP Udp;
CoAPServer CS; 

CoAPResource* resFlash;
String CoAPAnswer;

String answer_get (CoAPResource* res, uint8_t format)
{
  String s;
  
    if (res == resFlash) {
            switch (format) {
              case COAP_FORMAT_TEXT:
                return String(nbFlash);
                break;
              case COAP_FORMAT_XML:  
                  s = "<flash>";
                  s += String(nbFlash);
                  s += "</flash>";     
                 return s;
            case COAP_FORMAT_JSON:        
                  s = " {\"flash\":";
                  s += String(nbFlash);
                  s += "}";        
                 return s;
            }  
      }   
}

uint8_t answer_put (CoAPResource* res, uint8_t format, uint8_t method, String input)
{
   if (method == COAP_METHOD_POST) {
        nbFlash = 0;
        return ERROR_CODE(2,04);
   }
   if (res == resFlash) {
          switch (format) {
              case 0: nbFlash = input.toInt();
                return ERROR_CODE(2,04);
              default:
                return ERROR_CODE(4,15);
          }
   } 
}

void setup()
{
  Serial.begin(9600);
  meanValue = analogRead(sensorPin);
  LED=SM1_HIGH;

  Serial.print ("Starting DHCP..."); 
  Ethernet.begin(mac);
  Serial.println (Ethernet.localIP());
  
  Udp.begin(localPort);
  CS.begin(Udp);
  
  resFlash = CS.addRes(String("/flash"), answer_get, answer_put);
#ifdef DUMP_COAP
   CS.listRes();
#endif 
  
}


void loop() {
  sensorValue = analogRead(sensorPin);    
  meanValue = alpha * sensorValue + (1 - alpha) * meanValue; 

  switch (LED) {
    case SM1_HIGH:
      if ((meanValue - sensorValue) > 80) {
        nbLow = 0;
        LED = SM1_LOW;
      }
      break;
      
    case SM1_LOW:
        if ((meanValue - sensorValue) > 80) {
            nbLow++;
            if (nbLow == 2) {
                nbFlash++;
            }
              
        } else LED = SM1_HIGH;
        break;
    default:
       Serial.println ("SM1 unknown state");
  } 
    
  CS.incoming();
      
}


