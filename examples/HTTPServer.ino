#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>

#define COAP_OPTION_URI_PATH 11
#define COAP_OPTION_CONTENT 12
#define COAP_OPTION_QUERY 15



enum SearchState {SSNoConnection, // no connection started with server.
                  SSEOL, // flush until end of line
                  SSfound, // keywork found
                  SSsearch, // searching for keywords
                  SSGetURL, // take URL (no spaces)
                  SSflush, // error read until end of packet;
                  SSData,
                  SSCreateCoAP,
                  SSWaitAnswer,
                  SSProcessResponse
                } SState = SSsearch;

#define SETSTATE(s) {SState = s; Serial.print("State to "); Serial.println ((byte) s); }

char *TypeMsg[] = { "CON", "NON", "ACK", "RST" };
char *TypeCode[] = { "PING", "GET", "POST", "PUT", "DELETE" };


byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x6F, 0x30 };
EthernetServer server = EthernetServer(80);

int idxKW; 
uint32_t token = 0x00;
uint32_t tokenRx;
uint32_t mid = 0x00;

byte request[40];
uint8_t idxReq;
byte readBuf[32]; 
uint8_t readSize=0;
uint8_t readIdx=4;

struct keyword {
    char* key;
    bool cont;
    int  idx; 
    String* content;
};

String URLorig, acceptContent;      
struct keyword keywords[] = { {"GET", true, 0, &URLorig}, 
                              {"POST", true, 0, &URLorig},
                              {"PUT", true, 0, &URLorig},
                              {"DELETE", true, 0, &URLorig}, 
                              {"Accept:", true, 0, &acceptContent}
                          };
uint8_t method = 0xFF; // method received
uint8_t nbKW; 


void setup()
{
  Serial.begin(9600);
  Serial.print ("Starting DHCP..."); 
  
  Wire.begin(); //I'm the master of the I2C bus
  
  Ethernet.begin(mac);
  Serial.println (Ethernet.localIP());
  server.begin();
  nbKW = sizeof keywords / sizeof (struct keyword); 
  for (int k; k < nbKW; k++) {
      keywords[k].cont = true;
      keywords[k].idx = 0;  
  }
  
  SETSTATE(SSNoConnection); 
}

bool charOK (char c) {
 Serial.print('~'); Serial.print (c);
 if ((c =='/') || (c == '&') || (c == '?')) return false;  
 return true;
}

void sendRequest(byte* req, uint8_t len, uint8_t addr) {

  Serial.print("Send to ");  Serial.println(addr);
  for (int k=0; k < idxReq; k++) {
    Serial.print(req[k], HEX); 
    Serial.print(" ");
  }


  Wire.beginTransmission(addr);
  Wire.write (req, len);
  Wire.endTransmission();
  Serial.println("fin de transmimssion");
}

EthernetClient client;
bool oneCR;     // if 2 CR means that data starts
bool skipSpace; // in GetURL to skip first spaces before URL
  

void loop()
{
  char c;
  bool oneMatch; // true in SSsearch if one keyword is still active
  uint8_t currentOption= 0;
  uint8_t deltaOption;
  byte O[4];
  uint32_t mask = 0xFF000000;
  uint8_t L = 4;
  bool opt = true; 
  int idxSeg = 0; 
  int k;  
  uint8_t contentFormat = 0;

  
    switch (SState) {
       case SSNoConnection:
         client = server.available();
         if (client == true) {
           oneCR = false; 
           SETSTATE (SSsearch); 
         }
         break;
         
      case SSsearch:
        if (client.available() ) {
          c = client.read();
          if (c == '\r') break; // skip carriage retrun
          if (oneCR) {
              if (c == '\n') {
                 SETSTATE(SSData);  
                 break;
              } else oneCR = false;
          } //else if (c == '\n') oneCR = true;

          oneMatch = false;
          for (int k=0; k <nbKW; k++) {
              if (keywords[k].cont) {
                if (c == keywords[k].key[keywords[k].idx]) {
                   oneMatch = true; 
                   Serial.print ("("); Serial.print(k); Serial.print (")");

                   if (keywords[k].idx == (strlen(keywords[k].key)-1)) {
                     idxKW = k;
                     *(keywords[k].content) = "";
                     skipSpace = true;
             
                     if (k < 4) {
                         skipSpace=true; 
                         SETSTATE(SSGetURL);;
                         method = k;
                     }
                     else SETSTATE(SSfound); // to be changed to get a string
                       
                   } else keywords[k].idx++;
                } else {
                     keywords[k].cont = false;
                }
              }
             }
             if (!oneMatch) SETSTATE(SSEOL);


          Serial.print(c);Serial.print("-");
        } else SETSTATE(SSNoConnection);
        
        break;
        
      case SSGetURL:
       if (client.available() ) {
         c = client.read();
          if (c == '\r') break; // skip carriage retrun
          if (oneCR) {
              if (c == '\n') {
                 SETSTATE(SSData);  
                 break;
              } else {
                oneCR = false;
                Serial.print ("URL:"); Serial.println(*(keywords[idxKW].content));
                SETSTATE(SSsearch);
              }
          } else if (c == '\n') oneCR = true;

       if ((skipSpace) && (c == ' ')) break;
        
        skipSpace = false;
        
        if (c == ' ') {
           SETSTATE (SSEOL);
           break;
        }
        
         *(keywords[idxKW].content) += c;
         Serial.print("U"); Serial.print(c);
       } else SETSTATE (SSNoConnection);
      break;
      
      case SSfound:
       if (client.available() ) {
         c = client.read();
          if (c == '\r') break; // skip carriage retrun
          if (oneCR) {
              if (c == '\n') {
                 SETSTATE(SSData);  
                 break;
              } else {
                 oneCR = false;
                 Serial.print ("contenu:"); Serial.println(*(keywords[idxKW].content));
                 SETSTATE(SSsearch); 
               }
          } else if (c == '\n') oneCR = true;

        
         *(keywords[idxKW].content) += c;
         Serial.print("F"); Serial.print(c);
       } else SETSTATE (SSNoConnection);
      break;
      
      case SSEOL:
        if (client.available() ) {
         c = client.read();
         Serial.print ("\\"); Serial.print (c);
          if (c == '\r') break; // skip carriage retrun
          if (c == '\n') {
             oneCR = true;
               for (int k; k < nbKW; k++) {
                keywords[k].cont = true;
                keywords[k].idx = 0;  
              }

             SETSTATE(SSsearch);
            break; 
          }
          oneCR = false;
        }
        break;
        
          
      case SSData:
        c = client.read();

        if (client.available()) {
          Serial.print("D"); Serial.print(c);  
        } else SETSTATE(SSCreateCoAP); 
        break;
        
      case SSCreateCoAP:
        Serial.println("Create CoAP");
              Serial.println("Gest request");
             Serial.print ("Method:"); Serial.println(method);
             Serial.print("URL:"); Serial.println (URLorig);
             Serial.print("Accept:"); Serial.println(acceptContent);
          
          
          token++;  // new request -> new token
          
          // compute token length 
          L = 4; opt = true; 
          
          for (int k = 0; k < 4; k++)
          {
          O[k] = ((token & mask) >> (3 - k) * 8);
            mask >>= 8;
            if (!O[k] & opt) 
	    L--;
            else opt = false;
          }

          if (!L) L = 1;			// send a least 1 byte

          Serial.print("Token="); Serial.print(token); Serial.print(" length="); Serial.println(L);
          
          // 
          mid++; 
          idxReq = 0x0; 

// CoAP HEADER

          if (method < 4) { 
            request[idxReq] = 0x40; // Version 1 CON
          } else return; // not a known method

          request[idxReq++] |= L; // on search GET is 0 and DEL 3, add 1 for CoAP code
          request[idxReq++] = method+1;
          request[idxReq++] = (mid >> 16);
          request[idxReq++] = (mid & 0x00FF); 
          
//TOKEN
          
          for (int k=0; k < L; k++) 
            request[idxReq++] = O[3-k];
 
 // URI options         
 
          currentOption = 0;
 
          while( idxSeg < URLorig.length()) {
            Serial.print(URLorig[idxSeg]);
            
            if (URLorig[idxSeg] == '/') {
              Serial.println("new TLV path");
              
              deltaOption = (COAP_OPTION_URI_PATH - currentOption);
              currentOption = COAP_OPTION_URI_PATH; 
            } else  if ((URLorig[idxSeg] == '/') || (URLorig[idxSeg] == '&')) {
              Serial.println("new TLV query");
              
              deltaOption = (COAP_OPTION_QUERY - currentOption);
              currentOption = COAP_OPTION_QUERY; 
            };
            
            Serial.print("Current ="); Serial.println(currentOption);
            Serial.print ("Delta ="); Serial.println(deltaOption);
            
            if ((deltaOption < 0) || (deltaOption > 12)) {
                 Serial.println("Error in option delta"); 
                 SState == SSflush;
                 return;
            };
            
              request[idxReq] = deltaOption << 4;
              uint8_t lidx = idxReq++; // keep the value to set length when known
              idxSeg++; // skip ctrl char
              while ((idxSeg < URLorig.length()) && charOK(URLorig[idxSeg])) {
                  Serial.print(".");
                 request[idxReq++] = URLorig[idxSeg++];
              }
              uint8_t size = idxReq-lidx-1;
               if ((size < 0) || (size > 12)) {
                 Serial.println("Error in option size"); 
                 SState = SSflush;
                 return;
              };
             
              request[lidx] |= size;
              
              Serial.print("size"); Serial.println(idxReq-lidx-1);
              Serial.print("current char="); Serial.println(URLorig[idxSeg]);
              
            }
                    
          sendRequest(request, idxReq, 4); 
          SETSTATE(SSWaitAnswer);
          break;
          
          case SSWaitAnswer:
          Wire.requestFrom(4, 32);
          delay(10);
          if (Wire.available()) {
            Serial.print ("available = "); Serial.println(Wire.available());
            readSize = Wire.read();

            Serial.print("Size = "); Serial.print(readSize); Serial.print (" Available "); Serial.println(Wire.available());
            int k = 0;
            while (Wire.available() && (k <31)) {
                readBuf[k++] = Wire.read();
                Serial.print(k);
            }
            readSize--; // remove the length of the length byte
            SETSTATE(SSProcessResponse);

          }
          Serial.println("----");
          break;
          
          case SSProcessResponse:
          Serial.print("CoAP msg length = "); Serial.println(readSize); 
          Serial.println("receive response");
          for (k = 0; k < readSize; k++) {
             Serial.print(" "); Serial.print((uint8_t)readBuf[k], HEX); 
          }
          
          int version = (readBuf[0]  & 0xC0) >> 6;
          Serial.print ("Version = "); Serial.println (version);
          
          if (version != 1) SETSTATE(SSNoConnection);
          
          uint8_t type = (readBuf[0]  & 0x30) >> 4;
          Serial.print ("Type = "); Serial.print (type); Serial.println(TypeMsg[type]);
          if ((type == 1) || (type == 3)) SETSTATE(SSNoConnection); // not NON or ACK not response
          
          uint8_t tkl = (readBuf[0]  & 0x0F);
          Serial.print ("Token Length = "); Serial.println (tkl); 
          if (tkl > 4) SETSTATE(SSNoConnection); // in a int32
          
          uint8_t code = readBuf[1];
          int cla = (code & 0xE0) >> 5;
          int detail = code & 0x1F;
          Serial.print ("Code = "); Serial.print(cla); Serial.print("."); Serial.println(detail); 
          
          if ((cla != 2) && (detail != 5)) SETSTATE(SSNoConnection); // no data
          
          uint16_t midRx = (readBuf[2] << 8) | readBuf[3];
          Serial.print ("MID = "); Serial.print (midRx); Serial.print(" MID sent "); Serial.println(mid); 
          
          if ((type == 2) && (midRx != mid))  SETSTATE(SSNoConnection); // mid do not match
         
          uint32_t tokenRx = 0;
          
          
          int readIdx = 4; tokenRx = 0;
          for (int k = 0; k < tkl; k ++) {
              tokenRx << 8;
              tokenRx |= readBuf[readIdx++];
          }
          Serial.print ("rcv token = "); Serial.println(tokenRx); 
          
          if (token != tokenRx) SETSTATE(SSNoConnection); // token do not match
          
          // check options
          
          byte b = readBuf[readIdx++];
          
          currentOption = 0;
          
          while ((readIdx < readSize) && (b != 0xFF)) {
               
               uint8_t delta = (b & 0xF0) >> 4;
               uint8_t length = (b & 0x0F);
               
               currentOption += delta; 

               Serial.print("Option delta = "), Serial.print(delta, HEX); Serial.print ("option = "); Serial.println (currentOption); 
               Serial.print("Option length = "), Serial.println(length, HEX); 
               
               switch (currentOption) {
                 case COAP_OPTION_CONTENT:
                   Serial.println("Content format");
                   contentFormat = readBuf[readIdx++];
                   Serial.print ("current format = "); Serial.println(contentFormat);
                   break;
                 default:
                   Serial.println ("unknown option");
                   Serial.print("Content :");
                   for (int k =0; k < length; k++) {
                    Serial.print(readBuf[readIdx++], HEX);   
                   }
                   Serial.println(); 
               }
               b = readBuf[readIdx];
               Serial.print("B = "); Serial.println(b, HEX);
          }
          
          client.write ("HTTP/1.1 200 OK\r\n");
          client.write ("Connection: close\r\n");
          client.write ("Server: Arduino CoAP_HTTP proxy\r\n");
          client.write ("Content-Type: text/plain; charset=utf-8\r\n");
          client.write ("\r\n");
          client.write ("coucou");
          
          
          if (b == 0xFF) {
              Serial.println("There is data\r\n");
          }
         
          SETSTATE(SSNoConnection);
          break;
    }
}


