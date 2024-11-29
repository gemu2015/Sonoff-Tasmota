
// inspired by https://github.com/MoritzLerch/tesla-pv-display
#ifndef Powerwall_h
#define Powerwall_h


#define PW_RETRIES 2


#define PWL_LOGLVL LOG_LEVEL_INFO

// include libraries
#ifdef USE_BEAR
#include "WiFiClientSecureLightBearSSL.h"
#else
ESP_SSLClient ssl_client;
//EthernetClient basic_client;
WiFiClient basic_client;
#endif

class Powerwall {
   private:
    const char* powerwall_ip;
    String tesla_email;
    String tesla_password;
    String authCookie;
    
   public:
    Powerwall();
    String getAuthCookie();
    String GetRequest(String url, String authCookie);
    String GetRequest(String url);
    String AuthCookie();
    String Pwl_test(String);
};


Powerwall::Powerwall() {
    powerwall_ip   = POWERWALL_IP_CONFIG;
    tesla_email    = TESLA_EMAIL;
    tesla_password = TESLA_PASSWORD;
    authCookie     = "";
}

String Powerwall::AuthCookie() {
    return authCookie;
}

String Powerwall::Pwl_test(String ip) {
    AddLog(PWL_LOGLVL, PSTR("PWL: try to open %s"), ip.c_str());

    ssl_client.setInsecure();
  /** Call setDebugLevel(level) to set the debug
  * esp_ssl_debug_none = 0
  * esp_ssl_debug_error = 1
  * esp_ssl_debug_warn = 2
  * esp_ssl_debug_info = 3
  * esp_ssl_debug_dump = 4
  */
    ssl_client.setDebugLevel(0);

  // Set the receive and transmit buffers size in bytes for memory allocation (512 to 16384).
  // For server that does not support SSL fragment size negotiation, leave this setting the default value
  // by not set any buffer size or set the rx buffer size to maximum SSL record size (16384) and 512 for tx buffer size.  
    //ssl_client.setBufferSizes(1024 /* rx */, 512 /* tx */);
  
  // Assign the basic client
  // Due to the basic_client pointer is assigned, to avoid dangling pointer, basic_client should be existed 
  // as long as it was used by ssl_client for transportation.
    ssl_client.setClient(&basic_client);

    int retry = 0;
    while (retry < PW_RETRIES) {
        int32_t res = ssl_client.connect(ip.c_str(), 443);
        if (res) {
            break;
        }
        delay(100);
        retry++;
    }

    if (retry >= PW_RETRIES) {
        AddLog(PWL_LOGLVL, PSTR("PWL: failed"));
    } else {
        AddLog(PWL_LOGLVL, PSTR("PWL: connected"));
    }

    ssl_client.stop();

    return "\n";

/*
    //WiFiClientSecure *httpsClient = new WiFiClientSecure;
    BearSSL::WiFiClientSecure_light *httpsClient = new BearSSL::WiFiClientSecure_light(1024, 1024);
    httpsClient->setInsecure();
    httpsClient->setTimeout(1000);
    int retry = 0;
    while ((!httpsClient->connect(ip.c_str(), 443)) && (retry < 5)) {
        delay(100);
        //Serial.print(".");
        retry++;
    }

    if (retry >= 5) {
        AddLog(LOG_LEVEL_INFO, PSTR("PWL: failed"));
    } else {
        AddLog(LOG_LEVEL_INFO, PSTR("PWL: connected"));
    }
    httpsClient->stop();
    delete httpsClient;
    return "\n";
    */
}


void pHexdump(uint8_t *sbuff, uint32_t slen) {
  char cbuff[slen*3+10];
  char *cp = cbuff;
  *cp++ = '>';
  *cp++ = ' ';
  for (uint32_t cnt = 0; cnt < slen; cnt ++) {
    sprintf_P(cp, PSTR("%02x "), sbuff[cnt]);
    cp += 3;
  }
  AddLog(PWL_LOGLVL, PSTR("PWL: response: %s"), cbuff);

}


/**
 * This function returns a string with the authToken based on the basic login endpoint of
 * the powerwall in combination with the credentials from the secrets.h
 * @returns authToken to be used in an authCookie
 */
String Powerwall::getAuthCookie() {
    AddLog(PWL_LOGLVL, PSTR("PWL: requesting new auth Cookie from %s"), powerwall_ip);
    String apiLoginURL = "/api/login/Basic";

#ifdef USE_BEAR
#ifdef ESP32
    //WiFiClientSecure *httpsClient = new WiFiClientSecure;
    BearSSL::WiFiClientSecure_light *httpsClient = new BearSSL::WiFiClientSecure_light(1024, 1024);
#else
   // BearSSL::WiFiClientSecure_light *httpsClient = new BearSSL::WiFiClientSecure_light(1024,1024);
    WiFiClientSecure *httpsClient = new WiFiClientSecure;
#endif
    httpsClient->setInsecure();
    httpsClient->setTimeout(1000);
    int retry = 0;
    while ((!httpsClient->connect(powerwall_ip, 443)) && (retry < PW_RETRIES)) {
        delay(100);
        //Serial.print(".");
        retry++;
    }
    if (retry >= PW_RETRIES) {
        delete httpsClient;
        return ("CONN-FAIL");
    }
#else

    ssl_client.setInsecure();
    //ssl_client.setBufferSizes(1024 /* rx */, 512 /* tx */);
    ssl_client.setClient(&basic_client);

    int retry = 0;
    while (retry < PW_RETRIES) {
        int32_t res = ssl_client.connect(powerwall_ip, 443);
        if (res) {
            break;
        }
        delay(100);
        retry++;
    }

    if (retry >= PW_RETRIES) {
        return ("CONN-FAIL");
    }

#endif

    AddLog(PWL_LOGLVL, PSTR("PWL: connected"));

    String dataString = "{\"username\":\"customer\",\"email\":\"" + tesla_email + "\",\"password\":\"" + tesla_password + "\",\"force_sm_off\":false}";

    String payload = String("POST ") + apiLoginURL + " HTTP/1.1\r\n" +
                      "Host: " + powerwall_ip + "\r\n" +
                      "Connection: close" + "\r\n" +
                      "Content-Type: application/json" + "\r\n" +
                      "Content-Length: " + dataString.length() + "\r\n" +
                      "\r\n" + dataString + "\r\n\r\n";

#if USE_BEAR
    httpsClient->println(payload);
    uint32_t timeout = 500;
    while (httpsClient->connected()) {
        String response = httpsClient->readStringUntil('\n');
        if (response == "\r") {
            break;
        }
        timeout--;
        delay(10);
        if (!timeout) {
            break;
        }
    }
    String jsonInput = httpsClient->readStringUntil('\n');
#else

    AddLog(PWL_LOGLVL, PSTR("PWL: payload: %s"),payload.c_str());

    ssl_client.println(payload);
    uint32_t timeout = 100;

    delay(1000);

    uint8_t string[1200];

    while (ssl_client.connected()) {
        if (ssl_client.available()) {
            uint32_t dlen = ssl_client.available();
            AddLog(PWL_LOGLVL, PSTR("PWL: available: %d"), dlen);
            String response = "";
            //response = ssl_client.readStringUntil('\n');
            //AddLog(PWL_LOGLVL, PSTR("PWL: response: %s"), response.c_str());
            uint32_t cnt = 0;
            while (ssl_client.available()) {
                string[cnt] = ssl_client.read();
                cnt++;
            }
            string[cnt] = 0;
            pHexdump(string, dlen);

            
            char *cp = (char*)response.c_str();
            if (!strncmp_P(cp, PSTR("HTTP"), 4)) {
                char *sp = strchr(cp, ' ');
                if (sp) {
                    sp++;
                    uint16_t result = strtol(sp, 0, 10);
                    if (result != 200) {
                        ssl_client.stop();
                        return "";
                    } else {
                       // break;
                    }
                }
            }
            if (response == "\r") {
                break;
            }
        }
        timeout--;
        delay(10);
        AddLog(PWL_LOGLVL, PSTR("PWL: timeout: %d"), timeout);
        if (!timeout) {
            ssl_client.stop();
            return "";
        }
    }

    String jsonInput;
    if (ssl_client.connected() && ssl_client.available()) {
        jsonInput = ssl_client.readStringUntil('\n');
        AddLog(PWL_LOGLVL, PSTR("PWL: jsonInput %s"),jsonInput.c_str());
    }

#endif

/*


08:03:48.983 PWL: response HTTP/1.1 200 OK
08:03:48.995 PWL: response Cache-Control: no-cache, no-store
08:03:49.007 PWL: response Set-Cookie: AuthCookie=9N8pmBS8IQ4BKeg4oVTOQzhsL_pnQyk-ShJLx98gM7X8RlvBh6TY7m63L6tfJ6_97jQx7PXqnCzHtGhjg-LD7A==; Path=/
08:03:49.034 PWL: response Set-Cookie: UserRecord=eyJlbWFpbCI6ImdtdXR6MjAxMEBnb29nbGVtYWlsLmNvbSIsImZpcnN0bmFtZSI6IlRlc2xhIiwibGFzdG5hbWUiOiJFbmVyZ3kiLCJyb2xlcyI6WyJIb21lX093bmVyIl0sInRva2VuIjoiOU44cG1CUzhJUTRCS2VnNG9WVE9RemhzTF9wblF5ay1TaEpMeDk4Z003WDhSbHZCaDZUWTdtNjNMNnRmSjZfOTdqUXg3UFhxbkN6SHRHaGpnLUxEN0E9PSIsInByb3ZpZGVyIjoiQmFzaWMiLCJsb2dpblRpbWUiOiIyMDI0LTExLTI4VDA4OjAzOjQ5LjA2OTgyMzk1MyswMTowMCJ9; Path=/
08:03:49.076 PWL: response Date: Thu, 28 Nov 2024 07:03:49 GMT
08:03:49.087 PWL: response Content-Length: 267
08:03:49.098 PWL: response Content-Type: text/plain; charset=utf-8
08:03:49.109 PWL: response Connection: close
08:03:49.120 PWL: response 

{
	"email": "",
	"firstname": "Tesla",
	"lastname": "Energy",
	"roles": [
		"Home_Owner"
	],
	"token": "OgiGHjoNvwx17SRIaYFIOWPJSaKBYwmMGc5K4...qaWbWjTuI3fa_8QW32ED5zg1A==",
	"provider": "Basic",
	"loginTime": "2023-03-25T13:10:48.9029581+01:00"
}
*/
    char str_value[128];
    str_value[0] = 0;
    float fv;
    JsonParser parser((char*)jsonInput.c_str());
    JsonParserObject obj = parser.getRootObject();
    uint32_t res = JsonParsePath(&obj, "token", '#', &fv, str_value, sizeof(str_value));

    AddLog(PWL_LOGLVL, PSTR("PWL: token: %s"), str_value);

    authCookie = str_value;

#ifdef USE_BEAR
    httpsClient->stop();
    delete httpsClient;
#else
    ssl_client.stop();
#endif
    
    return authCookie;
}

/**
 * This function does a GET-request on the local powerwall web server.
 * This is mainly used here to do API requests.
 * HTTP/1.0 is used because some responses are so big that this would encounter
 * chunked transfer encoding in HTTP/1.1 (https://en.wikipedia.org/wiki/Chunked_transfer_encoding)
 *
 * @param url relative URL on the Powerwall
 * @param authCookie optional, but recommended
 * @returns content of request
 */
String Powerwall::GetRequest(String url, String authCookie) {
    
#ifdef USE_BEAR
#ifdef ESP32
    //WiFiClientSecure *httpsClient = new WiFiClientSecure;
    BearSSL::WiFiClientSecure_light *httpsClient = new BearSSL::WiFiClientSecure_light(1024, 1024);
#else
    //BearSSL::WiFiClientSecure_light *httpsClient = new BearSSL::WiFiClientSecure_light(1024,1024);
    WiFiClientSecure *httpsClient = new WiFiClientSecure;
#endif
    httpsClient->setInsecure();
    httpsClient->setTimeout(1000);
#else
    ssl_client.setInsecure();
    ssl_client.setTimeout(1000);
    ssl_client.setClient(&basic_client);
#endif
    
    if (authCookie == "") {
        getAuthCookie();
    }

    AddLog(PWL_LOGLVL, PSTR("PWL: doing GET-request to %s - %s"), powerwall_ip, url.c_str());

    int retry = 0;

#ifdef USE_BEAR
    while ((!httpsClient->connect(powerwall_ip, 443)) && (retry < 5)) {
        delay(100);
        //Serial.print(".");
        retry++;
    }

    if (retry >= 15) {
        delete httpsClient;
        return ("CONN-FAIL");
    }

    // HTTP/1.0 is used because of Chunked transfer encoding
    String request = "GET " + url + " HTTP/1.0" + "\r\n" +
                      "Host: " + powerwall_ip + "\r\n" +
                      "Cookie: " + "AuthCookie" + "=" + authCookie + "\r\n" +
                      "Connection: close\r\n\r\n"

    httpsClient->print(String("GET ") + url + " HTTP/1.0" + "\r\n" +
                      "Host: " + powerwall_ip + "\r\n" +
                      "Cookie: " + "AuthCookie" + "=" + authCookie + "\r\n" +
                      "Connection: close\r\n\r\n");

    uint32_t timeout = 500;
    while (httpsClient->connected()) {
        String response = httpsClient->readStringUntil('\r');
        char *cp =  (char*)response.c_str();
        if (!strncmp_P(cp, PSTR("HTTP"), 4)) {
            char *sp = strchr(cp, ' ');
            if (sp) {
                sp++;
                uint16_t result = strtol(sp, 0, 10);
                AddLog(PWL_LOGLVL, PSTR("PWL: result %d"), result);
                // in case of error 401, get new cookie
                if (result == 401) {
                    authCookie = "";
                } else if (result != 200) {
                    httpsClient->stop();
                    return "\n";
                }
            }
        }
        if (response == "\r") {
            break;
        }
        timeout--;
        delay(10);
        if (!timeout) {
            break;
        }
    }

    String result = httpsClient->readStringUntil('\n');
    httpsClient->stop();
    delete httpsClient;
#else

    while ((!ssl_client.connect(powerwall_ip, 443)) && (retry < PW_RETRIES)) {
        delay(100);
        //Serial.print(".");
        retry++;
    }

    if (retry >= PW_RETRIES) {
        return ("CONN-FAIL");
    }

    AddLog(PWL_LOGLVL, PSTR("PWL: connected"));

    // HTTP/1.0 is used because of Chunked transfer encoding
    String request = "GET " + url + " HTTP/1.0" + "\r\n" +
                      "Host: " + powerwall_ip + "\r\n" +
                      "Cookie: " + "AuthCookie" + "=" + authCookie + "\r\n" +
                      "Connection: close\r\n\r\n";
    ssl_client.print(request);
    AddLog(PWL_LOGLVL, PSTR("PWL: request: %s"), request.c_str());

    uint32_t timeout = 500;
    while (ssl_client.connected()) {
        String response = ssl_client.readStringUntil('\n');
        char *cp =  (char*)response.c_str();
        if (!strncmp_P(cp, PSTR("HTTP"), 4)) {
            char *sp = strchr(cp, ' ');
            if (sp) {
                sp++;
                uint16_t result = strtol(sp, 0, 10);
                AddLog(PWL_LOGLVL, PSTR("PWL: result %d"), result);
                // in case of error 401, get new cookie
                if (result == 401) {
                    authCookie = "";
                } else if (result != 200) {
                    ssl_client.stop();
                    return "\n";
                }
            }
        }
        if (response == "\r") {
            break;
        }
        timeout--;
        delay(10);
        if (!timeout) {
            break;
        }
    }

    String result = ssl_client.readStringUntil('\n');
    ssl_client.stop();
#endif

    return result;
}

/**
 * this is getting called if there was no provided authCookie in powerwallGetRequest(String url, String authCookie)
 */
String Powerwall::GetRequest(String url) {
    if (url[0] == '@') {
        url = url.substring(1);
        return Pwl_test(url);
    }
    return (GetRequest(url, getAuthCookie()));
}

#endif
