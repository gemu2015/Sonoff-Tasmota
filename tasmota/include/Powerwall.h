
// inspred by https://github.com/MoritzLerch/tesla-pv-display
#ifndef Powerwall_h
#define Powerwall_h

// include libraries
#include "WiFiClientSecureLightBearSSL.h"

class Powerwall {
   private:
    char tesla_email[32];
    char tesla_password[32];
    String authCookie;
    
   public:
    Powerwall();
    String getAuthCookie();
    String GetRequest(String url, String authCookie);
    String GetRequest(String url);
    String AuthCookie();
    void resetAuthCookie();
    char powerwall_ip[64];
};


Powerwall::Powerwall() {
    strcpy(powerwall_ip, POWERWALL_IP_CONFIG);
    strcpy(tesla_email, TESLA_EMAIL);
    strcpy(tesla_password, TESLA_PASSWORD);
    //tesla_email    = TESLA_EMAIL;
    //tesla_password = TESLA_PASSWORD;
    authCookie     = "";
}

String Powerwall::AuthCookie() {
    return authCookie;
}
void Powerwall::resetAuthCookie() {
    authCookie = "";
}

/**
 * This function returns a string with the authToken based on the basic login endpoint of
 * the powerwall in combination with the credentials from the secrets.h
 * @returns authToken to be used in an authCookie
 */
String Powerwall::getAuthCookie() {
    AddLog(LOG_LEVEL_DEBUG, PSTR("PWL: requesting new auth Cookie from %s"), powerwall_ip);
    String apiLoginURL = "/api/login/Basic";

#ifdef ESP32
    //WiFiClientSecure *httpsClient = new WiFiClientSecure;
    BearSSL::WiFiClientSecure_light *httpsClient = new BearSSL::WiFiClientSecure_light(1024,1024);
#else
   // BearSSL::WiFiClientSecure_light *httpsClient = new BearSSL::WiFiClientSecure_light(1024,1024);
    WiFiClientSecure *httpsClient = new WiFiClientSecure;
#endif
    httpsClient->setInsecure();
    httpsClient->setTimeout(2000);

    int retry = 0;

#define PW_RETRIES 5
    while ((!httpsClient->connect(powerwall_ip, 443)) && (retry < PW_RETRIES)) {
        delay(100);
        //Serial.print(".");
        retry++;
    }

    if (retry >= PW_RETRIES) {
        //String response = httpsClient->readStringUntil('\n');
        AddLog(LOG_LEVEL_DEBUG, PSTR("PWL: connection failed"));
        delete httpsClient;
        return ("CONN-FAIL");
    }

    AddLog(LOG_LEVEL_DEBUG, PSTR("PWL: connected"));

    String s_tesla_email = tesla_email;
    String s_tesla_password = tesla_password;

    String dataString = "{\"username\":\"customer\",\"email\":\"" + s_tesla_email + "\",\"password\":\"" + s_tesla_password + "\",\"force_sm_off\":false}";

    String payload = String("POST ") + apiLoginURL + " HTTP/1.1\r\n" +
                      "Host: " + powerwall_ip + "\r\n" +
                      "Connection: close" + "\r\n" +
                      "Content-Type: application/json" + "\r\n" +
                      "Content-Length: " + dataString.length() + "\r\n" +
                      "\r\n" + dataString + "\r\n\r\n";

    httpsClient->println(payload);

    while (httpsClient->connected()) {
        String response = httpsClient->readStringUntil('\n');
        if (response == "\r") {
            break;
        }
    }

    String jsonInput = httpsClient->readStringUntil('\n');

    char str_value[128];
    str_value[0] = 0;
    float fv;
    JsonParser parser((char*)jsonInput.c_str());
    JsonParserObject obj = parser.getRootObject();
    uint32_t res = JsonParsePath(&obj, "token", '#', &fv, str_value, sizeof(str_value));

    AddLog(LOG_LEVEL_DEBUG, PSTR("PWL: token: %s"), str_value);

    authCookie = str_value;

    delete httpsClient;
    
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
#ifdef ESP32
   // WiFiClientSecure *httpsClient = new WiFiClientSecure;
    BearSSL::WiFiClientSecure_light *httpsClient = new BearSSL::WiFiClientSecure_light(1024,1024);
#else
    //BearSSL::WiFiClientSecure_light *httpsClient = new BearSSL::WiFiClientSecure_light(1024,1024);
    WiFiClientSecure *httpsClient = new WiFiClientSecure;
#endif
    httpsClient->setInsecure();
    httpsClient->setTimeout(10000);
    
    if (authCookie == "") {
        getAuthCookie();
    }

    AddLog(LOG_LEVEL_DEBUG, PSTR("PWL: doing GET-request to %s%s"), powerwall_ip, url.c_str());

    int retry = 0;

    while ((!httpsClient->connect(powerwall_ip, 443)) && (retry < 15)) {
        delay(100);
        //Serial.print(".");
        retry++;
    }

    if (retry >= 15) {
        //String response = httpsClient->readStringUntil('\n');
        AddLog(LOG_LEVEL_DEBUG, PSTR("PWL: connection failed"));
        delete httpsClient;
        return ("CONN-FAIL");
    }

    // HTTP/1.0 is used because of Chunked transfer encoding
    httpsClient->print(String("GET ") + url + " HTTP/1.0" + "\r\n" +
                      "Host: " + powerwall_ip + "\r\n" +
                      "Cookie: " + "AuthCookie" + "=" + authCookie + "\r\n" +
                      "Connection: close\r\n\r\n");

    while (httpsClient->connected()) {
        String response = httpsClient->readStringUntil('\n');
        char *cp =  (char*)response.c_str();
        if (!strncmp_P(cp, PSTR("HTTP"), 4)) {
            char *sp = strchr(cp, ' ');
            if (sp) {
                sp++;
                uint16_t result = strtol(sp, 0, 10);
                AddLog(LOG_LEVEL_DEBUG, PSTR("PWL: result %d"), result);
                // in case of error 401, get new cookie
                if (result == 401) {
                    authCookie = "";
                    resetAuthCookie();
                }
            }
        }
        if (response == "\r") {
            break;
        }
    }

    String result = httpsClient->readStringUntil('\n');
    delete httpsClient;
    AddLog(LOG_LEVEL_DEBUG, PSTR("PWL: result: %s"),result.c_str());
    return result;
}

/**
 * this is getting called if there was no provided authCookie in powerwallGetRequest(String url, String authCookie)
 */
String Powerwall::GetRequest(String url) {
    return (GetRequest(url, getAuthCookie()));
}

#endif
