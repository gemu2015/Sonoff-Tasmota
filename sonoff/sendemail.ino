#ifdef USE_SENDMAIL

#include "sendemail.h"

// enable serial debugging
//#define DEBUG_EMAIL_PORT Serial

SendEmail::SendEmail(const String& host, const int port, const String& user, const String& passwd, const int timeout, const int auth_used) :
    host(host), port(port), user(user), passwd(passwd), timeout(timeout), ssl(ssl), auth_used(auth_used), client(new BearSSL::WiFiClientSecure_light(1024,1024))
{

}



String SendEmail::readClient()
{
  String r = client->readStringUntil('\n');
  r.trim();
  while (client->available()) {
    delay(0);
    r += client->readString();
  }
  return r;
}

//void SetSerialBaudrate(int baudrate);

bool SendEmail::send(const String& from, const String& to, const String& subject, const String& msg)
{
bool status=false;
String buffer;

  if (!host.length()) {
    return status;
  }
  ESP.wdtFeed();

  client->setTimeout(8000);
  // smtp connect
#ifdef DEBUG_EMAIL_PORT
  SetSerialBaudrate(115200);
  DEBUG_EMAIL_PORT.print("Connecting: ");
  DEBUG_EMAIL_PORT.print(host);
  DEBUG_EMAIL_PORT.print(":");
  DEBUG_EMAIL_PORT.println(port);
#endif


  if (!client->connect(host.c_str(), port)) {
#ifdef DEBUG_EMAIL_PORT
      DEBUG_EMAIL_PORT.println("Connection failed: ");
      //DEBUG_EMAIL_PORT.println (client->getLastSSLError());
#endif
    goto exit;
  }

  ESP.wdtFeed();

  buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  if (!buffer.startsWith(F("220"))) {
    goto exit;
  }

  ESP.wdtFeed();

  buffer = F("EHLO ");
  buffer += client->localIP().toString();

  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  if (!buffer.startsWith(F("250"))) {
    goto exit;
  }
  if (user.length()>0  && passwd.length()>0 ) {

    //buffer = F("STARTTLS");
    //client->println(buffer);
    ESP.wdtFeed();

    buffer = F("AUTH LOGIN");
    client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
    DEBUG_EMAIL_PORT.println(buffer);
#endif
    buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
    DEBUG_EMAIL_PORT.println(buffer);
#endif
    if (!buffer.startsWith(F("334")))
    {
      goto exit;
    }
    base64 b;
    //buffer = user;
    //buffer = b.encode(buffer);
    buffer = b.encode(user);

    client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  //DEBUG_EMAIL_PORT.println(user);
  DEBUG_EMAIL_PORT.println(buffer);
#endif
    buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
    if (!buffer.startsWith(F("334"))) {
      goto exit;
    }
    //buffer = this->passwd;
    //buffer = b.encode(buffer);
    buffer = b.encode(passwd);
    client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  //DEBUG_EMAIL_PORT.println(passwd);
  DEBUG_EMAIL_PORT.println(buffer);
#endif
    buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
    if (!buffer.startsWith(F("235"))) {
      goto exit;
    }
  }

  ESP.wdtFeed();

  // smtp send mail
  buffer = F("MAIL FROM:");
  buffer += from;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  if (!buffer.startsWith(F("250"))) {
    goto exit;
  }
  buffer = F("RCPT TO:");
  buffer += to;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  if (!buffer.startsWith(F("250"))) {
    goto exit;
  }

  ESP.wdtFeed();

  buffer = F("DATA");
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  if (!buffer.startsWith(F("354"))) {
    goto exit;
  }
  buffer = F("From: ");
  buffer += from;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  buffer = F("To: ");
  buffer += to;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  buffer = F("Subject: ");
  buffer += subject;
  buffer += F("\r\n");
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  buffer = msg;
  client->println(buffer);
  client->println('.');
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif
  buffer = F("QUIT");
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  DEBUG_EMAIL_PORT.println(buffer);
#endif

  status=true;
exit:

  ESP.wdtFeed();
  return status;
}

#ifdef USE_PLAIN
void SendEmail::a3_to_a4(unsigned char * a4, unsigned char * a3) {
	a4[0] = (a3[0] & 0xfc) >> 2;
	a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
	a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
	a4[3] = (a3[2] & 0x3f);
}

int SendEmail::base64_encode(char *output, const char *input, int inputLen) {
  const char* _b64_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int i = 0, j = 0;
	int encLen = 0;
	unsigned char a3[3];
	unsigned char a4[4];
	while(inputLen--) {
		a3[i++] = *(input++);
		if(i == 3) {
			a3_to_a4(a4, a3);
			for(i = 0; i < 4; i++) {
				output[encLen++] = _b64_alphabet[a4[i]];
			}
			i = 0;
		}
	}
	if(i) {
		for(j = i; j < 3; j++) {
			a3[j] = '\0';
		}
		a3_to_a4(a4, a3);
		for(j = 0; j < i + 1; j++) {
			output[encLen++] = _b64_alphabet[a4[j]];
		}
		while((i++ < 3)) {
			output[encLen++] = '=';
		}
	}
	output[encLen] = '\0';
	return encLen;
}
#endif

#endif // USE_SENDMAIL
