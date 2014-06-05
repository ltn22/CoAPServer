#ifndef CoAPServer_h
#define CoAPServer_h

#define ERROR_CODE(a, b) ((a) <<4 | (b))

#define COAP_TYPE_CON 0x00
#define COAP_TYPE_NON 0x01
#define COAP_TYPE_ACK 0x02
#define COAP_TYPE_RST 0x03

#define MAX_BUF 80 
#define MAX_OBSERVE 2
#define URI_LENGTH 20

#define DUMP_COAP

class CoAPResource;
class CoAPServer;
void sendObserve(void*);
void dumpObserve(void*);

typedef String (*t_answer) (CoAPResource*, uint8_t);

#define CR_READ 0x01
#define CR_WRITE 0x02
#define CR_DELETE 0x04

class CoAPResource {  
  friend class CoAPServer;
  friend void sendObserve(void*); 

  CoAPResource* next;
  String        name;
  t_answer      function;
  uint32_t*     intPtr;
  String        des; 
  
 public:
  CoAPResource() { name = "";  next = NULL; };
  CoAPResource(String n, t_answer f) {name = n; function = f; next = NULL; };

  CoAPResource*  add (String, t_answer);
  CoAPResource* find(String); 

#ifdef DUMP_COAP
  void list ();
#endif
};


class CoAPToken{
  friend class CoAPServer;

  int tkl;
  byte token[8];
 public:
  CoAPToken () { tkl = 0; };
  inline CoAPToken(uint8_t l, byte* p) {setToken (l, p);};
  inline CoAPToken (CoAPToken* t) {setToken (t->tkl, t->token); }

  void setToken (uint8_t, byte *);
  void writeToken (byte*);
  inline uint8_t getTokenLength() { return tkl; };
  inline bool compareToken (CoAPToken* t) { return compareToken(t->tkl, t->token); };
  bool compareToken (uint8_t, byte*);
  bool operator== (CoAPToken t) { 
    Serial.println("operator==");
#ifdef DUMP_COAP    
    this->printToken();
    t.printToken(); 
#endif
    compareToken (t.tkl, t.token);
  }

  void copy(CoAPToken*); 
#ifdef DUMP_COAP
  void printToken();
#endif
};

class CoAPObserve{
  friend class CoAPMessage;
  friend class CoAPServer;
  friend void sendObserve(void*);
#ifdef DUMP_COAP
  friend void dumpObserve(void*);
#endif
  
  CoAPServer*   Cserver;

  IPAddress     IPserver;
  uint16_t      port;

  CoAPToken     obsToken;
  CoAPResource* obsRes;
  uint32_t      sequence;
  uint8_t       format;

  uint32_t      lastMID;
  int           event;
};

class CoAPServer {
  friend class CoAPMessage;
  friend void sendObserve(void*);
#ifdef DUMP_COAP
  friend void dumpObserve(void*);
#endif

  Timer         tempo;
  int           comMode; //eth, WiFi, I2C
  EthernetUDP   eudp;
  CoAPResource* resList;
  IPAddress     sender;
  uint16_t      port;

  uint16_t      sizeToEOP;

  CoAPObserve  obs[MAX_OBSERVE]; 

  uint32_t      period;
  
  uint16_t      s_mid;
  uint16_t      currentOption;
public:
  CoAPServer ();
  void begin (EthernetUDP); // UDP with port number;
  int      incoming();
  uint8_t  processRequest();

  uint32_t getValue (uint8_t);
  byte     readByte();

  void     setHeader (IPAddress, uint16_t, uint8_t, uint8_t, uint16_t, CoAPToken*);
  void     addOption (uint8_t, uint8_t, byte*);
  void     addOption (uint8_t t) { addOption(t, 0, (byte*) 0); }
  void     addOption (uint8_t T, uint32_t V); // Optimize option Length
  void     addValue (CoAPResource*, uint8_t);
  void     endMessage();


  CoAPResource*  addRes(String s, t_answer f) { return resList->add (s, f); } 

  int      findObserve(IPAddress, uint16_t, CoAPToken*); 

#ifdef DUMP_COAP
  void displayMime (int);
  void displayBlock (uint32_t);
  void listRes() {resList->list(); }
#endif

};

#endif // CoAPServer.h
