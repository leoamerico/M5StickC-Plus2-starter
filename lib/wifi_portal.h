#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_http_server.h>
#include <Preferences.h>

class WifiPortal {
private:
    DNSServer dnsServer;
    httpd_handle_t httpd = nullptr;
    bool saved = false;

    static String urlDecode(const String& in) {
        String out;
        out.reserve(in.length());
        for (int i = 0; i < (int)in.length(); i++) {
            if (in[i] == '+') out += ' ';
            else if (in[i] == '%' && i + 2 < (int)in.length()) {
                char h[3] = { in[i+1], in[i+2], 0 };
                out += (char)strtol(h, nullptr, 16);
                i += 2;
            } else out += in[i];
        }
        return out;
    }

    static bool parseForm(const char* body, String& ssid, String& pass) {
        String data(body);
        ssid = "";
        pass = "";
        int start = 0;
        while (start < (int)data.length()) {
            int amp = data.indexOf('&', start);
            if (amp < 0) amp = data.length();
            String pair = data.substring(start, amp);
            int eq = pair.indexOf('=');
            if (eq >= 0) {
                String key = pair.substring(0, eq);
                String val = urlDecode(pair.substring(eq + 1));
                if (key == "s") ssid = val;
                else if (key == "p") pass = val;
            }
            start = amp + 1;
        }
        return ssid.length() > 0 && ssid.length() <= 32 && pass.length() <= 64;
    }

    // Serve setup form
    static esp_err_t handleRoot(httpd_req_t* req) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, SETUP_HTML, strlen(SETUP_HTML));
        return ESP_OK;
    }

    // Save credentials from POST form
    static esp_err_t handleSave(httpd_req_t* req) {
        char buf[256] = {0};
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len <= 0) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
            return ESP_FAIL;
        }
        buf[len] = 0;

        String ssid, pass;
        if (!parseForm(buf, ssid, pass)) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID/pass");
            return ESP_FAIL;
        }

        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        prefs.end();

        WifiPortal* self = (WifiPortal*)req->user_ctx;
        self->saved = true;

        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, OK_HTML, strlen(OK_HTML));
        return ESP_OK;
    }

    // Return WiFi scan results as JSON
    static esp_err_t handleScan(httpd_req_t* req) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "[]", 2);
            return ESP_OK;
        }
        if (n == WIFI_SCAN_FAILED || n < 0) {
            WiFi.scanNetworks(true);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, "[]", 2);
            return ESP_OK;
        }

        String json = "[";
        for (int i = 0; i < n && i < 15; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.isEmpty()) continue;
            if (json.length() > 1) json += ",";
            String esc;
            for (unsigned k = 0; k < ssid.length(); k++) {
                char c = ssid.charAt(k);
                if (c == '\\') esc += "\\\\";
                else if (c == '"') esc += "\\\"";
                else if ((uint8_t)c < 0x20) esc += ' ';
                else esc += c;
            }
            json += "{\"s\":\"" + esc + "\",\"r\":" + String(WiFi.RSSI(i)) + "}";
        }
        json += "]";
        WiFi.scanDelete();

        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json.c_str(), json.length());
        return ESP_OK;
    }

    // Set credentials via GET query: /set?s=SSID&p=PASSWORD
    static esp_err_t handleSet(httpd_req_t* req) {
        char qbuf[192] = {};
        if (httpd_req_get_url_query_str(req, qbuf, sizeof(qbuf)) != ESP_OK) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ?s=SSID&p=PASS");
            return ESP_FAIL;
        }
        char sv[64] = {}, pv[128] = {};
        httpd_query_key_value(qbuf, "s", sv, sizeof(sv));
        httpd_query_key_value(qbuf, "p", pv, sizeof(pv));
        String ssid = urlDecode(String(sv));
        String pass = urlDecode(String(pv));
        if (ssid.isEmpty() || ssid.length() > 32 || pass.length() > 64) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID/pass");
            return ESP_FAIL;
        }
        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        prefs.end();
        WifiPortal* self = (WifiPortal*)req->user_ctx;
        self->saved = true;
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, OK_HTML, strlen(OK_HTML));
        return ESP_OK;
    }

    // Redirect to root (captive portal detection)
    static esp_err_t handleRedirect(httpd_req_t* req) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // 404 catch-all → redirect to portal
    static esp_err_t handle404(httpd_req_t* req, httpd_err_code_t err) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    void regUri(const char* path, httpd_method_t method,
                esp_err_t (*handler)(httpd_req_t*)) {
        httpd_uri_t u = {};
        u.uri = path;
        u.method = method;
        u.handler = handler;
        u.user_ctx = this;
        httpd_register_uri_handler(httpd, &u);
    }

    static const char SETUP_HTML[];
    static const char OK_HTML[];

public:
    bool start() {
        saved = false;

        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP_STA);           // AP + STA so scan works
        WiFi.softAP("M5Stick-Setup");     // open AP
        delay(500);
        WiFi.scanNetworks(true);           // async scan

        dnsServer.start(53, "*", WiFi.softAPIP());

        httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
        cfg.server_port = 80;
        if (httpd_start(&httpd, &cfg) != ESP_OK) return false;

        regUri("/",                    HTTP_GET,  handleRoot);
        regUri("/save",                HTTP_POST, handleSave);
        regUri("/set",                 HTTP_GET,  handleSet);
        regUri("/scan",                HTTP_GET,  handleScan);
        regUri("/generate_204",        HTTP_GET,  handleRedirect);
        regUri("/hotspot-detect.html", HTTP_GET,  handleRoot);

        httpd_register_err_handler(httpd, HTTPD_404_NOT_FOUND, handle404);

        Serial.printf("[portal] AP ready  IP %s\n", WiFi.softAPIP().toString().c_str());
        return true;
    }

    void loop() { dnsServer.processNextRequest(); }

    void stop() {
        if (httpd) { httpd_stop(httpd); httpd = nullptr; }
        dnsServer.stop();
        WiFi.scanDelete();
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
    }

    bool credentialsSaved() const { return saved; }
    int  getClientCount()   const { return WiFi.softAPgetStationNum(); }
};

// ── Captive-portal HTML (dark theme, WiFi scan list) ────────────────
const char WifiPortal::SETUP_HTML[] = R"HTML(<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>M5Stick WiFi</title><style>
*{box-sizing:border-box}
body{font-family:system-ui,-apple-system,sans-serif;max-width:380px;margin:40px auto;padding:0 20px;background:#111;color:#eee}
h2{text-align:center;color:#0ff;margin-bottom:4px}
label{display:block;margin-top:14px;font-size:14px;color:#999}
input{width:100%;padding:12px;margin-top:4px;border:1px solid #333;border-radius:8px;background:#222;color:#fff;font-size:16px}
input:focus{outline:none;border-color:#0ff}
button{width:100%;padding:14px;margin-top:20px;border:none;border-radius:8px;background:#07f;color:#fff;font-size:18px;font-weight:600}
button:active{background:#05c}
#nets{margin:6px 0;max-height:180px;overflow-y:auto}
.n{padding:10px 12px;margin:4px 0;background:#222;border-radius:6px;border:1px solid #333}
.n:active{background:#333}.n small{color:#666;float:right}
</style></head><body>
<h2>WiFi Setup</h2>
<form method=POST action=/save>
<label>Network (SSID)</label>
<div id=nets><div style="color:#666;text-align:center;padding:12px">Scanning...</div></div>
<input name=s maxlength=32 required placeholder="Or type SSID">
<label>Password</label>
<input name=p type=password maxlength=64>
<button>Save</button></form>
<script>
(function(){var el=document.getElementById('nets'),si=document.querySelector('[name=s]'),nets=[];
function scan(){fetch('/scan').then(function(r){return r.json()}).then(function(a){
if(!a.length){setTimeout(scan,1000);return}nets=a;var h='',i;
for(i=0;i<a.length;i++)h+='<div class=n data-i='+i+'>'+a[i].s.replace(/</g,'&lt;')+' <small>'+a[i].r+' dBm</small></div>';
el.innerHTML=h}).catch(function(){el.innerHTML=''})}
el.onclick=function(e){var t=e.target.closest('.n');if(t&&nets[t.dataset.i])si.value=nets[t.dataset.i].s};
scan()})()
</script></body></html>)HTML";

const char WifiPortal::OK_HTML[] = R"HTML(<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:system-ui;max-width:380px;margin:60px auto;padding:0 20px;background:#111;color:#fff;text-align:center}</style>
</head><body><h2 style="color:#0f0;font-size:32px">&#10003; Saved!</h2>
<p style="color:#999">WiFi credentials saved.<br>Disconnect from <b>M5Stick-Setup</b><br>and return to the device.</p>
</body></html>)HTML";

#endif
