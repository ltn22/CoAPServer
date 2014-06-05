#include <Timer.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <CoAPServer.h>


#ifdef DUMP_COAP
char *TypeMsg[] = { "CON", "NON", "ACK", "RST" };
char *TypeCode[] = { "PING", "GET", "PUT", "POST", "DELETE" };
#endif

CoAPResource* 
CoAPResource::add (String newName, t_answer f)
{
  CoAPResource *NE = this;


  while (NE->next != NULL)
    {
      if (NE->name == newName)
	{
	  return NE;
	}
      NE = NE->next;
    }
  NE->next = new CoAPResource (newName, f);
  return NE->next;
}

#ifdef DUMP_COAP
void
CoAPResource::list ()
{
  Serial.println ("list ");

  CoAPResource *NE = this;
  while (NE != NULL)
    {
      Serial.println (NE->name);
      NE = NE->next;
    }
}
#endif

CoAPResource *
CoAPResource::find (String targetName)
{
  CoAPResource *current = this;

  while (current)
    {
      if (current->name == targetName)
	return current;

      current = current->next;
    }

  return NULL;
}

//==

void
CoAPToken::setToken (uint8_t l, byte * p)
{
  tkl = l;

  for (int k = 0; k < l; k++)
    token[k] = p[k];
}

void
CoAPToken::writeToken (byte * p)
{
  for (int k = 0; k < tkl; k++)
    p[k] = token[k];
}

bool CoAPToken::compareToken (uint8_t l, byte * p)
{
  if (tkl != l)
    return false;

  for (int k = 0; k < l; k++)
    if (token[k] != p[k])
      return false;

  return true;
}

void
CoAPToken::copy (CoAPToken * source)
{
  tkl = source->tkl;

  for (int k = 0; k < tkl; k++)
    token[k] = source->token[k];
}


#ifdef DUMP_COAP
void
CoAPToken::printToken ()
{
  for (int k = 0; k < tkl; k++)
    Serial.print (token[k], HEX);
  Serial.println ();
}
#endif


//========

CoAPServer::CoAPServer ()
{
  resList = new CoAPResource ();
  
  for (int k=0; k < MAX_OBSERVE; k++) obs[k].port = 0;

  period = 1000;
  s_mid = 1;
  currentOption = 0;

#ifdef DUMP_COAP
  tempo.after (5000, dumpObserve, (void*) this);
#endif
}

void
CoAPServer::begin (EthernetUDP ctxtUDP)
{
  comMode = 1;			// EthernetUdp
  eudp = ctxtUDP;
}

int
CoAPServer::incoming ()
{
  int status;
  int packetSize = eudp.parsePacket ();

  if (packetSize)
    {
      sizeToEOP = packetSize;	// copy is new packet arrives when processing
      sender = eudp.remoteIP ();
      port = eudp.remotePort ();

      status = processRequest ();
    }

  tempo.update ();		// update timers
}

byte CoAPServer::readByte ()
{
  byte
    b = eudp.read ();
  sizeToEOP--;
  return b;
}

uint32_t CoAPServer::getValue (uint8_t len)
{
  int
    Value = 0;

  for (int k = 0; k < len; k++)
    {
      Value <<= 8;
      Value += readByte ();
    }
  return Value;
}

int CoAPServer::findObserve(IPAddress a, uint16_t p, CoAPToken* t)
{
  for (int k = 0; k < MAX_OBSERVE; k++) {
    if (obs[k].port && (obs[k].IPserver == a) && (obs[k].port == p) && (obs[k].obsToken.compareToken(t))) return k;
  }
  return -1;
}

#ifdef DUMP_COAP
void dumpObserve (void *context)
{
  CoAPServer* s= (CoAPServer*) context;

  for (int k =0; k < MAX_OBSERVE; k++) {
    Serial.print (k); Serial.print (":");
    Serial.print (s->obs[k].IPserver); Serial.print ("|"); Serial.print (s->obs[k].port); Serial.print (" ");
    s->obs[k].obsToken.printToken();
  }
  s->resList->list();

  s->tempo.after(60000, dumpObserve, context);
}
#endif

void
sendObserve (void *context)
{
  CoAPObserve *
    obs = (CoAPObserve *) context;
  CoAPServer *
    s = obs->Cserver;

  s->setHeader (obs->IPserver, obs->port, COAP_TYPE_NON, ERROR_CODE (2, 05), s->s_mid, &(obs->obsToken));	//ACK same MID, NON new

  obs->lastMID = s->s_mid;
  s->s_mid++;

  s->addOption (0x06, obs->sequence++);
  s->addOption (0x0C, obs->format);
  s->addOption (0xFF);
  s->addValue (obs->obsRes, obs->format);

  s->endMessage ();

  obs->event = s->tempo.after (s->period, sendObserve, context);
}


uint8_t CoAPServer::processRequest ()
{
  CoAPToken
    token;
  CoAPToken *
    tokenObs;			// copy for observe
  String
    URIPath = "";
  CoAPResource *
    URIres;
  int
    option = 0, format = 0;
  bool
    observe = false;
  uint32_t
    observeVal;

  CoAPResource* AnsRes; 

  uint16_t
    currentDelta;
  byte
    b;
  uint8_t
    status = 0xFF;

  b = readByte ();

  int
    version = (b & 0xC0) >> 6;
  uint8_t
    type = (b & 30) >> 4;
  int
    tkl = (b & 0x0F);

  b = readByte ();

  uint8_t
    code = b;

  uint16_t
    mid = (readByte () << 8) | readByte ();
#ifdef DUMP_COAP
  Serial.print ("ver:");
  Serial.print (version);
  Serial.print (" Type = ");
  Serial.print (type);
  Serial.print (" (");
  Serial.print ((type < 4) ? TypeMsg[type] : "???");
  Serial.print (")");
  Serial.print (" Token Length = ");
  Serial.print (tkl);
  Serial.print (" code ");
  Serial.print (code);
#endif

  int
    cla = (code & 0xE0) >> 5;
  int
    detail = code & 0x1F;

#ifdef DUMP_COAP
  Serial.print (" (");
  if (!cla)
    {
      Serial.print ((type < 4) ? TypeCode[detail] : "???");
    }
  else
    {
      Serial.print (cla);
      Serial.print (".");
      Serial.print (detail);
    }
  Serial.print (")");
  Serial.print (" Msg id = ");
  Serial.println (mid, HEX);
#endif

  byte
    tok[8];
  eudp.read (tok, tkl);
  sizeToEOP -= tkl;
  token.setToken (tkl, tok);

#ifdef DUMP_COAP
  Serial.print ("Token: ");
  token.printToken ();
#endif

  // process Options

  b = 0x00;

  while (sizeToEOP && (b != 0xFF))
    {
      b = readByte ();
      if (b != 0xFF)
	{
	  uint32_t
	    delta = (b & 0xF0) >> 4;
	  uint32_t
	    length = (b & 0x0F);

	  if (delta == 0x0F)
	    {
	      if (length = 0x0F)
		break;
	      else
		{
		  status = ERROR_CODE (4, 02);
		  goto proc_error;
		};
	    }

	  if ((delta == 13) || (delta == 14))
	    {
	      uint16_t
		d1 = readByte () + 13;
	      if (delta == 14)
		d1 = d1 * 256 + readByte () + 256;

	      delta = d1;
	    }

	  option += delta;

	  if ((length == 13) || (length == 14))
	    {
	      uint16_t
		l1 = readByte () + 13;
	      if (delta == 14)
		l1 = l1 * 256 + readByte () + 256;

	      length = l1;
	    }
#ifdef DUMP_COAP
	  Serial.print ("option :");
	  Serial.print (option);
	  Serial.print (" length=");
	  Serial.println (length);
#endif
	  switch (option)
	    {

	    case 6:
#ifdef DUMP_COAP
	      Serial.print ("Observe ");
#endif
	      observeVal = 0;
	      observeVal = getValue (length);
#ifdef DUMP_COAP
	      Serial.print (observeVal, HEX);
#endif

	      observe = true;
	      break;

	    case 11:
#ifdef DUMP_COAP
	      Serial.print ("Uri-Path ");
#endif
	      URIPath += '/';
	      for (int k = 0; k < length; k++)
		{
		  URIPath += (char) readByte ();
		  if (URIPath.length () > URI_LENGTH)
		    {
		      return ERROR_CODE (5, 00);
		    }
		}
#ifdef DUMP_COAP
	      Serial.println (URIPath);
#endif
	      break;
	    case 12:
	      format = getValue (length);
#ifdef DUMP_COAP
	      Serial.print ("Content-Format ");
	      displayMime (format);
#endif
	      break;

	    case 23:
#ifdef DUMP_COAP
	      Serial.print ("Block2 ");
	      displayBlock (getValue (length));
#endif
	      break;

	    case 60:
#ifdef DUMP_COAP
	      Serial.print ("Size 1");
	      Serial.print (getValue (length));
#endif
	      break;

	    default:		// flush option
#ifdef DUMP_COAP
	      Serial.print ("Unknown Option :");
	      Serial.println (option);
#endif
	      for (int k = 0; k < length; k++)
		{
		  b = readByte ();
#ifdef DUMP_COAP
		  Serial.print (b, HEX);
#endif
		}
	      break;
	    }
#ifdef DUMP_COAP
	  Serial.println ();
#endif
	}			// b != 0xFF

    }				// while for options

  if (b == 0xFF)
    {
      Serial.println ("these is data");
      while (sizeToEOP)
	Serial.print ((char) readByte ());
    }

  if (type == 0x03)
    {				// RST desable observe ?
      Serial.println ("RST MID =");
      Serial.println (mid);
    }


  if (type == 0x02)
    return 0xFF;		// should never receive an ACK

  if ((type != 0x00) && (type != 0x01))
    {				// not CON or NON
      status = ERROR_CODE (4, 00);
      goto proc_error;
    }

  switch (code)
    {				// method
    case 0x01:
      resList->list();

      URIres = resList->find (URIPath);

      if (!URIres)
	{
	  status = ERROR_CODE (4, 04);
	  goto proc_error;
	}

      if (observe && token.tkl) // no token no observe
	{
	  int ctx = findObserve (sender, port, &token);
	  int k;

	  if (observeVal) // Value so remove observe
	    {
	      Serial.print ("contexte = "); Serial.println(ctx); 
	      if (ctx != -1) {
		tempo.stop (obs[ctx].event);
		obs[ctx].port = 0; 
	      }
	    }
	  else // No Value add observe
	    if (ctx == -1) {
	      for (k=0; k <MAX_OBSERVE; k++) // find an avaible context
		if (!obs[k].port)
		  {
		    obs[k].IPserver = sender;
		    obs[k].port = port;
		    obs[k].obsToken.copy(&token);
		    obs[k].obsRes = URIres;
		    obs[k].format = format;
		    obs[k].Cserver = this;
		    obs[k].sequence = 1;
		    
		    obs[k].event = tempo.after (period, sendObserve, (void *) &obs[k]);
		    break;
		  }

	      if (k == MAX_OBSERVE) observe = false; // no room for observe
	    }
	}

      setHeader (sender, port, ((type == COAP_TYPE_CON) ? COAP_TYPE_ACK : COAP_TYPE_NON), ERROR_CODE (2, 05), ((type == COAP_TYPE_CON) ? mid : s_mid++), &token);	//ACK same MID, NON new

      if (observe)
	{
	  addOption (0x06, obs[0].sequence++);
	}
      addOption (0x0C, format);
      addOption (0xFF);
      addValue (URIres, format);
      endMessage ();

      return ERROR_CODE (2, 05);
      break;

    case 0x00:			// Unknown code
      for (int k=0; k < MAX_OBSERVE; k++) {
	if (obs[k].lastMID == mid) {
	  tempo.stop(obs[k].event); 
	}
      }
      return 0xFF;
    }



  status = ERROR_CODE (2, 04);
  goto proc_error;

  return status;

proc_error:
  Serial.print ("Error code = ");
  Serial.println (status, HEX);
  if (status)
    {
      setHeader (sender, port,
		 ((type == COAP_TYPE_CON) ? COAP_TYPE_ACK : COAP_TYPE_NON),
		 status, ((type == COAP_TYPE_CON) ? mid : s_mid++), &token);
      endMessage ();

    }
  return status;
}

void
CoAPServer::setHeader (IPAddress s, uint16_t p,
		       uint8_t type, uint8_t code, uint16_t mid,
		       CoAPToken * token)
{
  byte b[4];
  eudp.beginPacket (s, p);
  b[0] = 0x40 | ((type << 4) & 0x30);	// Version = 01
  b[0] |= (token->tkl & 0x0F);
  b[1] = code;
  b[2] = (mid >> 8);
  b[3] = (mid & 0x00FF);
  eudp.write (b, 4);
  eudp.write (token->token, token->tkl);

  currentOption = 0;
}

void
CoAPServer::addOption (uint8_t T, uint8_t L, byte * V)
{
  byte b;

  if (T == 0xFF)
    {
      b = 0xFF;
      eudp.write (&b, 1);
      return;
    }

  if (T < currentOption)
    return;			// only increase

  uint8_t delta = T - currentOption;

  currentOption = T;

  if (delta < 0x0F)
    {
      b = (delta << 4);
    }
  else
    Serial.println ("not implemented yet");

  if (L < 0x0F)
    {
      b |= L;
    }
  else
    Serial.println ("not implemented yet");

  eudp.write (&b, 1);
  eudp.write (V, L);
}

void
CoAPServer::addOption (uint8_t T, uint32_t V)
{
  byte b;
  uint8_t L;
  bool opt = true; 

  if (T == 0xFF)
    {
      b = 0xFF;
      eudp.write (&b, 1);
      return;
    }

  if (T < currentOption)
    return;			// only increase

  uint8_t delta = T - currentOption;

  currentOption = T;

  byte O[4];
  uint32_t mask = 0xFF000000;
  L = 4;

  for (int k = 0; k < 4; k++)
    {
      O[k] = ((V & mask) >> (3 - k) * 8);
      mask >>= 8;
      if (!O[k] & opt) 
	L--;
      else opt = false;
    }

  if (!L)
    L = 1;			// send a least 1 byte

  if (delta < 0x0F)
    {
      b = (delta << 4);
    }
  else
    Serial.println ("not implemented yet");

  if (L < 0x0F)
    {
      b |= L;
    }
  else
    Serial.println ("not implemented yet");

  eudp.write (&b, 1);
  for (int k = 4 - L; k < 4; k++)
    eudp.write (&O[k], 1);
}

void
CoAPServer::addValue (CoAPResource* res, uint8_t format)
{
  Serial.println ("call user function");
  String result = (*res->function) (res, format);

  Serial.print ("result="); Serial.println(result);
  for (int k = 0; k < result.length (); k++)
    eudp.write ((byte) result[k]);
}

void
CoAPServer::endMessage ()
{
  eudp.endPacket ();
}

#ifdef DUMP_COAP
void
CoAPServer::displayMime (int m)
{
  Serial.print ("(");
  Serial.print (m);
  Serial.print (") ");
  switch (m)
    {
    case 0:
      Serial.print ("text/plain; charset=utf-8");
      break;
    case 40:
      Serial.print ("Application/link-format");
      break;
    case 41:
      Serial.print ("Application/xml");
      break;
    case 42:
      Serial.print ("Application/octet-stream");
      break;
    case 47:
      Serial.print ("Application/exi");
      break;
    case 50:
      Serial.print ("Application/json");
      break;
    case 60:
      Serial.print ("Application/cbor");
      break;
    default:
      Serial.print ("Unknown");
    }
}

void
CoAPServer::displayBlock (uint32_t v)
{
  uint32_t szx = v & 0x00000007;
  uint32_t m = (v & 0x00000008) >> 3;
  uint32_t num = (v & 0xFFFFFFF0) >> 4;

  Serial.print (" Size = ");
  Serial.print ((uint32_t) 0x01 << (szx + 4));
  Serial.print (" More = ");
  Serial.print (m);
  Serial.print (" Num = ");
  Serial.print (num);
}

#endif
