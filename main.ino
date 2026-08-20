#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <IRremote.hpp>
#include <TFT_eSPI.h>

const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASS     = "YOUR_WIFI_PASSWORD";

const char* CLIENT_ID     = "YOUR_SPOTIFY_CLIENT_ID";
const char* CLIENT_SECRET = "YOUR_SPOTIFY_CLIENT_SECRET";
const char* REFRESH_TOKEN = "YOUR_SPOTIFY_REFRESH_TOKEN";

#define IR_RECEIVE_PIN       26

#define IR_CMD_PLAY_PAUSE    0x68  
#define IR_CMD_NEXT          0x62  
#define IR_CMD_PREV          0x65  
#define IR_CMD_SHUFFLE       0x09  

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite uiSprite = TFT_eSprite(&tft);

String accessToken = "";
unsigned long tokenExpireTime = 0;
String currentTrackUri = "";
String songTitle = "No Track";
String artistName = "Spotify Idle";
bool isPlayingState = false;
bool isShuffleState = false;
uint32_t progressMs = 0;
uint32_t durationMs = 0;

unsigned long lastSpotifyCheck = 0;
const unsigned long SPOTIFY_POLL_INTERVAL = 2000;

unsigned long lastTick = 0;
int titleScrollPos = 0;
int artistScrollPos = 0;

bool refreshSpotifyToken() {
  HTTPClient http;
  http.begin("https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String authBody = "grant_type=refresh_token&refresh_token=" + String(REFRESH_TOKEN) +
                    "&client_id=" + String(CLIENT_ID) +
                    "&client_secret=" + String(CLIENT_SECRET);

  int httpCode = http.POST(authBody);
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    JsonDocument doc;
    deserializeJson(doc, response);
    
    accessToken = doc["access_token"].as<String>();
    int expiresIn = doc["expires_in"].as<int>();
    tokenExpireTime = millis() + ((expiresIn - 60) * 1000);
    http.end();
    return true;
  }
  http.end();
  return false;
}

void ensureValidToken() {
  if (accessToken == "" || millis() >= tokenExpireTime) {
    refreshSpotifyToken();
  }
}

void sendSpotifyCommand(const String& method, const String& endpoint, const String& payload = "") {
  ensureValidToken();
  HTTPClient http;
  http.begin("https://api.spotify.com/v1/me/player/" + endpoint);
  http.addHeader("Authorization", "Bearer " + accessToken);
  if (payload.length() > 0) {
    http.addHeader("Content-Type", "application/json");
  } else {
    http.addHeader("Content-Length", "0");
  }

  if (method == "PUT") http.PUT(payload);
  else if (method == "POST") http.POST(payload);
  else if (method == "DELETE") http.sendRequest("DELETE");
  http.end();
}

void drawScrollingText(TFT_eSprite& spr, const String& text, int x, int y, int maxW, int& scrollPos, uint16_t color, uint8_t font) {
  spr.setTextFont(font);
  int textW = spr.textWidth(text);
  
  if (textW <= maxW) {
    spr.setTextColor(color, TFT_BLACK);
    spr.drawString(text, x, y);
  } else {
    String scrollStr = text + "   " + text;
    int fullW = spr.textWidth(text + "   ");
    
    TFT_eSprite textClip = TFT_eSprite(&tft);
    textClip.setColorDepth(16);
    textClip.createSprite(maxW, 26);
    textClip.fillSprite(TFT_BLACK);
    textClip.setTextColor(color, TFT_BLACK);
    textClip.setTextFont(font);
    
    textClip.drawString(scrollStr, -scrollPos, 0);
    textClip.pushToSprite(&spr, x, y);
    textClip.deleteSprite();
    
    scrollPos += 2;
    if (scrollPos >= fullW) {
      scrollPos = 0;
    }
  }
}

void renderUI() {
  uiSprite.fillSprite(TFT_BLACK);

  uiSprite.fillRoundRect(10, 8, 80, 18, 4, 0x1DB9);
  uiSprite.setTextColor(TFT_BLACK, 0x1DB9);
  uiSprite.drawString("SPOTIFY", 18, 12, 1);

  drawScrollingText(uiSprite, songTitle, 10, 34, 220, titleScrollPos, TFT_WHITE, 4);
  drawScrollingText(uiSprite, artistName, 10, 64, 220, artistScrollPos, 0xBDF7, 2);

  int barX = 10;
  int barY = 92;
  int barW = 220;
  int barH = 6;
  uiSprite.fillRoundRect(barX, barY, barW, barH, 3, 0x2104);
  if (durationMs > 0) {
    int progressW = map(progressMs, 0, durationMs, 0, barW);
    progressW = constrain(progressW, 0, barW);
    uiSprite.fillRoundRect(barX, barY, progressW, barH, 3, 0x1DB9);
  }

  uint32_t sec = progressMs / 1000;
  uint32_t durSec = durationMs / 1000;
  char curTime[8], totalTime[8];
  snprintf(curTime, sizeof(curTime), "%02u:%02u", sec / 60, sec % 60);
  snprintf(totalTime, sizeof(totalTime), "%02u:%02u", durSec / 60, durSec % 60);

  uiSprite.setTextColor(0x8410, TFT_BLACK);
  uiSprite.drawString(curTime, 10, 104, 1);
  uiSprite.drawString(totalTime, 195, 104, 1);

  int badgeY = 104;
  if (isPlayingState) {
    uiSprite.fillRoundRect(80, badgeY, 48, 16, 4, 0x1DB9);
    uiSprite.setTextColor(TFT_BLACK, 0x1DB9);
    uiSprite.drawString("PLAY", 90, badgeY + 3, 1);
  } else {
    uiSprite.fillRoundRect(80, badgeY, 48, 16, 4, 0xFBE0);
    uiSprite.setTextColor(TFT_BLACK, 0xFBE0);
    uiSprite.drawString("PAUSE", 86, badgeY + 3, 1);
  }

  uint16_t shufBg = isShuffleState ? 0x1DB9 : 0x2104;
  uint16_t shufFg = isShuffleState ? TFT_BLACK : TFT_WHITE;
  uiSprite.fillRoundRect(134, badgeY, 50, 16, 4, shufBg);
  uiSprite.setTextColor(shufFg, shufBg);
  uiSprite.drawString("SHUFFLE", 138, badgeY + 3, 1);

  uiSprite.pushSprite(0, 0);
}

void updatePlaybackState() {
  ensureValidToken();

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  if (!http.begin(client, "https://api.spotify.com/v1/me/player/currently-playing")) return;

  http.addHeader("Authorization", "Bearer " + accessToken);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      isPlayingState = doc["is_playing"].as<bool>();
      progressMs     = doc["progress_ms"].as<uint32_t>();
      durationMs     = doc["item"]["duration_ms"].as<uint32_t>();
      
      String trackUri = doc["item"]["uri"].as<String>();
      String newTitle = doc["item"]["name"].as<String>();
      String newArtist = doc["item"]["artists"][0]["name"].as<String>();

      if (trackUri != currentTrackUri) {
        currentTrackUri = trackUri;
        songTitle       = newTitle;
        artistName      = newArtist;
        titleScrollPos  = 0;
        artistScrollPos = 0;
      }
    }
  } else if (httpCode == 204) {
    if (currentTrackUri != "") {
      currentTrackUri = "";
      songTitle       = "No Track";
      artistName      = "Spotify Idle";
      progressMs      = 0;
      durationMs      = 0;
      tft.fillScreen(TFT_BLACK);
    }
  }
  http.end();
}

void handleIRInput() {
  if (IrReceiver.decode()) {
    if (IrReceiver.decodedIRData.protocol != UNKNOWN) {
      uint32_t command = IrReceiver.decodedIRData.command;
      
      if (command == IR_CMD_PLAY_PAUSE) {
        if (isPlayingState) sendSpotifyCommand("PUT", "pause");
        else sendSpotifyCommand("PUT", "play");
        delay(100);
        updatePlaybackState();
      } 
      else if (command == IR_CMD_NEXT) {
        sendSpotifyCommand("POST", "next");
        delay(150);
        updatePlaybackState();
      } 
      else if (command == IR_CMD_PREV) {
        sendSpotifyCommand("POST", "previous");
        delay(150);
        updatePlaybackState();
      }
      else if (command == IR_CMD_SHUFFLE) {
        isShuffleState = !isShuffleState;
        sendSpotifyCommand("PUT", "shuffle?state=" + String(isShuffleState ? "true" : "false"));
      }
    }
    IrReceiver.resume();
  }
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  uiSprite.setColorDepth(16);
  uiSprite.createSprite(240, 135);

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  refreshSpotifyToken();
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  handleIRInput();

  if (millis() - lastSpotifyCheck >= SPOTIFY_POLL_INTERVAL) {
    lastSpotifyCheck = millis();
    updatePlaybackState();
  }

  if (millis() - lastTick >= 80) {
    lastTick = millis();
    if (isPlayingState && durationMs > 0) {
      progressMs += 80;
      if (progressMs > durationMs) progressMs = durationMs;
    }
    renderUI();
  }
}
