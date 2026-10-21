// Screen driver library
#include <TFT_eSPI.h>
// SPI driver library
#include <SPI.h>
// Image decoding library
#include <TJpg_Decoder.h>
// JSON library for web parsing
#include "ArduinoJson.h"
// Time library for clock functionality
#include <TimeLib.h>
// WiFi library
#include <WiFi.h>
// HTTP request library
#include <HTTPClient.h>
// Library for creating WiFi UDP service
#include <WiFiUdp.h>
// Static storage for saving fixed parameters
#include <EEPROM.h>
#include <Arduino.h>
#include "lvgl.h"
// Weather icons
#include "weathernum.h"

// Butterfly GIF
#include "AW4.h"
#include <AnimatedGIF.h>

// Temperature and humidity icons
#include "img/temperature.h"
#include "img/humidity.h"

#include "img/wx.h"

// Font files for clock display
#include "font/sj38.h"
// Font files for weekdays, date, and follower count display
#include "font/zkyyt12.h"
// Font files for city name display
#include "font/ALBB17.h"
// Font files for weather display
#include "font/tq17.h"

#include "font/ALBB10.h"

#include "font/ALBB18.h"

//-------------------------- Build-time configuration --------------------------
// Settings live in config.h in the project root (-I. in platformio.ini). Copy
// config.h.example to config.h and edit it; that file is git-ignored and holds
// the WiFi credentials and the API key. Every value has a fallback below, so the
// sketch also compiles without it.
#if defined(__has_include)
#if __has_include("config.h")
#include "config.h"
#endif
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "spotpear"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "12345678"
#endif
#ifndef WEATHER_API_KEY
#define WEATHER_API_KEY "fbf5a0e942e6fea3ff18103b9fd46ed9"
#endif
#ifndef CITY_AUTO_DETECT
#define CITY_AUTO_DETECT 1
#endif
#ifndef CITY_CODE
#define CITY_CODE "101020100"
#endif
#ifndef NTP_SERVER
#define NTP_SERVER "time.nist.gov"
#endif
#ifndef NTP_SYNC_MINUTES
#define NTP_SYNC_MINUTES 20
#endif
#ifndef FOLLOWER_TEXT
#define FOLLOWER_TEXT "Hello"
#endif
#ifndef TIMEZONE_SOURCE
#define TIMEZONE_SOURCE 0
#endif
#ifndef TIMEZONE_STD_OFFSET_HOURS
#define TIMEZONE_STD_OFFSET_HOURS 1
#endif
#ifndef TIMEZONE_EU_DST
#define TIMEZONE_EU_DST 1
#endif

//-------------- WiFi credentials (see config.h) --------------
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
//-------------------------------------------------------------

// Modify the text content inside ""
String follower = FOLLOWER_TEXT;

// City ID can be found in the CityCode folder's JavaScript files
//--------------------

WeatherNum wrat;  // Weather object
int prevTime = 0;
int AprevTime = 0;
int Anim = 0;
// Background color
uint16_t bgColor = 0x0000;

// Font colors

// Font color for hours and minutes
uint16_t timehmfontColor = 0xFFFF;
// Font color for seconds
uint16_t timesfontColor = 0xFFFF;
// Font color for weekdays
uint16_t weekfontColor = 0xFFFF;
// Font color for date
uint16_t monthfontColor = 0xFFFF;
// Font color for temperature and humidity
uint16_t thfontColor = 0xFFFF;
// Font color for top-left scrolling text
uint16_t tipfontColor = 0xFFFF;
// Font color for city name
uint16_t cityfontColor = 0xFFFF;
// Font color for air quality
uint16_t airfontColor = 0xFFFF;
// Red color
uint16_t fontColor = 0xFF00;
uint16_t bilifontColor = 0xF81F;
// Border color
uint16_t xkColor = 0xFFFF;

//=================

AnimatedGIF gif;
#define DISPLAY_WIDTH  tft.width()
#define DISPLAY_HEIGHT tft.height()
#define BUFFER_SIZE 256            // Optimum is >= GIF width or integral division 

uint16_t usTemp[1][BUFFER_SIZE];    // Global to support DMA use

bool     dmaBuf = 0;

//=================

// Clock parameters ---------------------------------
// NTP server
static const char ntpServerName[] = NTP_SERVER;
float timeZone;

// 1 ==> obtain the city from the public IP address, 0 ==> use CITY_CODE
static String cityCode = CITY_CODE;
const char *api_key = WEATHER_API_KEY;

WiFiUDP Udp;
unsigned int localPort = 8000;
WiFiClient wificlient; 

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
String num2str(int digits);
void sendNTPpacket(IPAddress &address);

byte setNTPSyncTime = NTP_SYNC_MINUTES;  // NTP resync interval in minutes
int backLight_hour = 0;
time_t prevDisplay = 0;  // Display time

//---------------------- Temperature and humidity parameters ------------------
unsigned long wdsdTime = 0;
byte wdsdValue = 0;
String wendu1 = "", wendu2 = "", shidu = "", yaqiang = "", tianqi = "", kjd = "";

//----------------------------------------------------

//------------------ LVGL Astronaut Parameters ------------------

static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 240;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenHeight * screenWidth / 10];
TFT_eSPI tft = TFT_eSPI(240,240);  // Configure the pins in the User_Setup.h file of the tft_espi library
TFT_eSprite clk = TFT_eSprite(&tft);

LV_IMG_DECLARE(TKR_A);
static lv_obj_t *logo_imga = NULL;


//==========================
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette;
  int x, y, iWidth, iCount;

  // pDraw->iX = 50;
  // pDraw->iY = 50;

  // Displ;ay bounds chech and cropping
  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > DISPLAY_WIDTH)
    iWidth = DISPLAY_WIDTH - pDraw->iX;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line
  if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
    return;

  // Old image disposal
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < iWidth)
    {
      c = ucTransparent - 1;
      d = &usTemp[0][0];
      while (c != ucTransparent && s < pEnd && iCount < BUFFER_SIZE )
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        // DMA would degrtade performance here due to short line segments
        tft.setAddrWindow(pDraw->iX + x, y, iCount, 1);
        tft.pushPixels(usTemp, iCount);
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
          x++;
        else
          s--;
      }
    }
  }
  else
  {
    s = pDraw->pPixels;

    // Unroll the first pass to boost DMA performance
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    if (iWidth <= BUFFER_SIZE)
      for (iCount = 0; iCount < iWidth; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];
    else
      for (iCount = 0; iCount < BUFFER_SIZE; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];

#ifdef USE_DMA // 71.6 fps (ST7796 84.5 fps)
    tft.dmaWait();
    tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
    tft.pushPixelsDMA(&usTemp[dmaBuf][0], iCount);
    dmaBuf = !dmaBuf;
#else // 57.0 fps
    tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
    tft.pushPixels(&usTemp[0][0], iCount);
#endif

    iWidth -= iCount;
    // Loop if pixel buffer smaller than width
    while (iWidth > 0)
    {
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      if (iWidth <= BUFFER_SIZE)
        for (iCount = 0; iCount < iWidth; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];
      else
        for (iCount = 0; iCount < BUFFER_SIZE; iCount++) usTemp[dmaBuf][iCount] = usPalette[*s++];

#ifdef USE_DMA
      tft.dmaWait();
      tft.pushPixelsDMA(&usTemp[dmaBuf][0], iCount);
      dmaBuf = !dmaBuf;
#else
      tft.pushPixels(&usTemp[0][0], iCount);
#endif
      iWidth -= iCount;
    }
  }
} /* GIFDraw() */

//==========================


//------------------------------------------------------------
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {

  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  // Return 1 to decode next block
  return 1;
}

// LVGL flush callback
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

void tkr()
{
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenHeight * screenWidth / 10);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = 240;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;  
  lv_disp_drv_register(&disp_drv);

  static lv_style_t style;  
  lv_style_init(&style);
  lv_style_set_bg_color(&style, lv_color_black());
  lv_obj_add_style(lv_scr_act(), &style, 0);

  logo_imga = lv_gif_create(lv_scr_act());
  lv_obj_center(logo_imga);
  lv_obj_align(logo_imga, LV_ALIGN_LEFT_MID, 80, 0);
  lv_gif_set_src(logo_imga, &TKR_A);

  lv_timer_handler();
  delay(1);
}


//--------------------------- City code lookup and display
// Look up the city
void getCityCode() {
String URL = "http://ip-api.com/json/?fields=city,lat,lon";

HTTPClient httpClient;
httpClient.begin(wificlient,URL);

// Open the connection and send the HTTP request
  int httpCode = httpClient.GET();
  Serial.print("Send GET request to URL: ");
  Serial.println(URL);

  if (httpCode == HTTP_CODE_OK) {
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc,httpClient.getString());
    String city = doc["city"];
    if(CITY_AUTO_DETECT==1)
      cityCode=city; // Detected city
  }
  httpClient.end();
}

void getCityTime(){
  getCityCode();
  String URL = "http://api.openweathermap.org/data/2.5/weather?q=" + cityCode + "&appid=" + String(api_key) + "&units=metric"; //524901
  HTTPClient httpClient;
  httpClient.begin(URL);

  int httpCode = httpClient.GET();
   if (httpCode == HTTP_CODE_OK) {
      const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(13) + JSON_OBJECT_SIZE(40) + 470;
      DynamicJsonDocument doc(capacity);
      deserializeJson(doc,httpClient.getString());

      long timezone1 = doc["timezone"]; // Time zone offset in seconds
      timeZone = timezone1 / 3600.0;    // keep fractional zones such as +05:30

      Serial.println("City information received");
   }else {
    Serial.println("Failed to get city information");
    Serial.print(httpCode);
  }
  httpClient.end();
}

int temp_i1,temp_i2;
String scrollText[5];

// Fetch the city weather
void getCityWeater(){

  float temp_f,temp_min_f,temp_max_f;
  getCityCode();
  String URL = "http://api.openweathermap.org/data/2.5/weather?q=" + cityCode + "&appid=" + String(api_key) + "&units=metric"; //524901
 
  // Create the HTTPClient object
 HTTPClient httpClient;
 httpClient.begin(URL);


  int httpCode = httpClient.GET();
  Serial.println("Fetching weather data");
  Serial.println(URL);

  // On HTTP 200 read the response body and log it to serial
  if (httpCode == HTTP_CODE_OK) {

    const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(13) + JSON_OBJECT_SIZE(40) + 470;
    DynamicJsonDocument doc(capacity);
    deserializeJson(doc,httpClient.getString());
    // JsonObject sk = doc.as<JsonObject>();
    // String str1 = httpClient.getString();
 
    float temp = doc["main"]["temp"]; // Temperature in Celsius
    int humidity = doc["main"]["humidity"]; // Humidity in percentage
    int pressure = doc["main"]["pressure"];
    String description = doc["weather"][0]["description"]; // Weather description
    String icon = doc["weather"][0]["icon"];
    float temp_min = doc["main"]["temp_min"];
    float temp_max = doc["main"]["temp_max"];
    int visibility = doc["visibility"];
    
    temp_f = 32 + temp*1.8;
    temp_min_f = 32 + temp_min*1.8;
    temp_max_f = 32 + temp_max*1.8;

    temp_i1 = int(temp_f);
    temp_i2 = int(temp);

    wendu1 = String(temp_i1); // Temperature in F
    wendu2 = String(temp_i2); // Temperature in C
    shidu = String(humidity); // Humidity
    yaqiang = String(pressure); // Pressure
    tianqi = String(description); // Weather
    kjd = String(visibility/1000); // Visibility
    
    clk.setColorDepth(8);
    clk.loadFont(tq17);
  // City name
  clk.createSprite(140, 18);
  clk.fillSprite(bgColor);
  clk.setTextDatum(ML_DATUM);
  clk.setTextColor(cityfontColor, bgColor);
  clk.drawString(cityCode,1,8); //
  clk.pushSprite(1,5);
  clk.deleteSprite();
  clk.unloadFont();

  scrollText[0] = "MIN T "+String(temp_min_f)+ " F / "+String(temp_min)+"℃";
  scrollText[1] = "MAX T "+String(temp_max_f)+ " F / "+String(temp_max)+"℃";
  scrollText[2] = "Weather "+String(tianqi);
  scrollText[3] = "pressure "+String(yaqiang)+"hPa";
  scrollText[4] = "visibility "+String(kjd)+"km";
  wrat.printfweather1(1,130,icon);

  Serial.println("Weather data received");

  } else {
    Serial.println("City weather request failed: ");
    Serial.print(httpCode);
  }

  // Close the connection to the server
  httpClient.end();
}


//---------------- Temperature and humidity cycle -------------------------

void weatherWarning() {  // Cycle temperature and humidity every 5 s, outdoor values
  if (millis() - wdsdTime > 5000) {
    wdsdValue = wdsdValue + 1;
    //Serial.println("wdsdValue0" + String(wdsdValue));
    clk.setColorDepth(8);
    clk.loadFont(tq17);
    switch (wdsdValue) {
      case 1:
        //Serial.println("wdsdValue1" + String(wdsdValue));
        TJpgDec.drawJpg(165, 150, temperature, sizeof(temperature));  // Temperature icon
        for (int i = 20; i > 0; i--) {
          clk.createSprite(70, 16);
          clk.fillSprite(bgColor);
          clk.setTextDatum(ML_DATUM);
          clk.setTextColor(thfontColor, bgColor);
          clk.drawString(wendu1 + " F", 1, i + 8);
          clk.pushSprite(185, 150);
          clk.deleteSprite();
          delay(10);
        }
        break;
      case 2:
        //Serial.println("wdsdValue2" + String(wdsdValue));
        TJpgDec.drawJpg(165, 150, temperature, sizeof(temperature));  // Temperature icon
        for (int i = 20; i > 0; i--) {
          clk.createSprite(70, 16);
          clk.fillSprite(bgColor);
          clk.setTextDatum(ML_DATUM);
          clk.setTextColor(thfontColor, bgColor);
          clk.drawString(wendu2+"℃", 1, i + 8);
          clk.pushSprite(185, 150);
          clk.deleteSprite();
          delay(10);
        }
        break;
        case 3:
      //Serial.println("wdsdValue2" + String(wdsdValue));
          TJpgDec.drawJpg(165,150,humidity, sizeof(humidity));  // Humidity icon
          for(int i=20;i>0;i--) {
          clk.createSprite(70, 16);
          clk.fillSprite(bgColor);
          clk.setTextDatum(ML_DATUM);
          clk.setTextColor(thfontColor, bgColor);
          clk.drawString(shidu+"%",1,i+8);
          clk.pushSprite(185, 150);
          clk.deleteSprite();
          delay(10);
        }
        wdsdValue = 0;
        break;
    }
    wdsdTime = millis();
    clk.unloadFont();
  }
}

//---------------------------- Weather info banner -------------------------
int currentIndex = 0;

TFT_eSprite clkb = TFT_eSprite(&tft);

void scrollBanner() {
  if (millis() - prevTime > 3500) {  // Switch every 3.5 s

    if (scrollText[currentIndex]) {

      clkb.setColorDepth(8);
      clkb.loadFont(tq17);

      for (int pos = 20; pos > 0; pos--) {
        scrollTxt(pos);
      }

      clkb.deleteSprite();
      clkb.unloadFont();

      if (currentIndex >= 4) {
        currentIndex = 0;  // Back to the first entry
      } else {
        currentIndex += 1;  // Advance to the next entry
      }
    }
    prevTime = millis();
  }
}

void scrollTxt(int pos) {
  clkb.createSprite(240, 16);
  clkb.fillSprite(bgColor);
  clkb.setTextWrap(false);
  clkb.setTextDatum(ML_DATUM);
  clkb.setTextColor(tipfontColor, bgColor);
  clkb.drawString(scrollText[currentIndex], 1, pos + 8);
  clkb.pushSprite(35, 215);
}

//----------------------------------------------

void get_wifi() {
  // Join the network
  WiFi.begin(ssid, password);
  // Wait for the connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(10);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  // Connected
  Serial.print("IP address: ");      // Print the IP address
  Serial.println(WiFi.localIP());
}

void fanspush() { // Draw the follower text
  TJpgDec.drawJpg(1, 215, wx, sizeof(wx));
  clk.createSprite(60, 18);
  clk.fillSprite(bgColor);
  clk.loadFont(ALBB18);
  clk.setTextColor(fontColor, bgColor);
  clk.drawString(String(follower), 1, 9);
  clk.pushSprite(170, 3);
  clk.deleteSprite();
  clk.unloadFont();
}


void setup() {
  pinMode(42,OUTPUT);
  digitalWrite(42,LOW);

  Serial.begin(115200);
  tft.begin();           /* TFT init */ 
  tft.invertDisplay(1);  // Invert all colours: 1 inverted, 0 normal
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, 0x0000);

  tft.setRotation(4);

  // LVGL pushes its first frame from tkr(), so the TFT bus has to be ready
  tkr();

  gif.begin(BIG_ENDIAN_PIXELS);
  if (gif.open((uint8_t *)AW4, sizeof(AW4), GIFDraw))
  {
      Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
      tft.startWrite(); // The TFT chip slect is locked low
      while (gif.playFrame(true, NULL))
      {
        yield();
      }
      gif.close();
      tft.endWrite(); // Release TFT chip select for other SPI devices
  }
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,8,2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // Print the hint text, \n starts a new line
  tft.println("---------------------------------------");
  tft.println("Configure mobile WLAN information:");
  tft.println("WIFI name:" + String(ssid));
  tft.println("WIFI password: " + String(password) + "\n---------------------------------------");
  tft.println("If there is a city information error,execute==>After turning on the phone WLAN,turn off the phone data,and then connect to the router WIFI.");
  tft.println("---------------------------------------");

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  get_wifi();

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);


  setSyncProvider(getNtpTime);
  setSyncInterval(setNTPSyncTime * 60);  // NTP resync interval in seconds



  tft.fillScreen(TFT_BLACK);
  tft.fillRoundRect(0, 0, 128, 128, 0, bgColor);  // Solid rectangle


  // Draw the frame lines
  tft.drawFastHLine(0, 0, 240, xkColor);


  tft.drawFastHLine(0, 25, 240, xkColor);
  // tft.drawFastHLine(0, 120, 240, xkColor);

  tft.drawFastVLine(150, 0, 25, xkColor);

  tft.drawFastHLine(0, 170, 240, xkColor);

  tft.drawFastVLine(150, 170, 36, xkColor);
  // tft.drawFastVLine(60, 170, 36, xkColor);
  tft.drawFastHLine(0,205,240,xkColor);
  int TCityCODE = 0;

  if (TCityCODE >= 101000000 && TCityCODE <= 102000000)
    cityCode = String(TCityCODE);
  else{
    getCityCode();  // Resolve the city, for example 101020200 or 101280601
    // delay(10);
    }
  getCityWeater();
  fanspush();
}



unsigned long weaterTime = 0;
void loop() {
  
  if (now() != prevDisplay) {
    prevDisplay = now();
    digitalClockDisplay();
  }

  if (millis() - weaterTime > 300000) { // Refresh the weather every 5 minutes
    weaterTime = millis(); 
    getCityWeater();
  }
  
  digitalClockDisplay();
  scrollBanner();
  weatherWarning();
  lv_timer_handler();
  delay(1);
}




// Clock display ----------------------------------------------------------------------

// Sprite geometry, scaled from the 240 px layout

void digitalClockDisplay() {

  clk.setColorDepth(8);

  /*** Time area ***/
  // Hours and minutes
  clk.createSprite(100, 50);
  clk.fillSprite(bgColor);
  clk.loadFont(sj38);
  clk.setTextDatum(ML_DATUM);
  clk.setTextColor(timehmfontColor, bgColor);
  clk.drawString(hourMinute(), 1, 16, 7);  // Draw hours and minutes
  clk.unloadFont();
  clk.pushSprite(50, 45);
  clk.deleteSprite();

  // Seconds
  clk.createSprite(50, 50);
  clk.fillSprite(bgColor);

  clk.loadFont(sj38);
  clk.setTextDatum(ML_DATUM);
  clk.setTextColor(timesfontColor, bgColor);
  clk.drawString(num2str(second()), 1, 16);

  clk.unloadFont();
  clk.pushSprite(150, 45);
  clk.deleteSprite();
  /*** Time area ***/

  /*** Bottom area ***/
  clk.loadFont(ALBB18);
  clk.createSprite(120, 25);
  clk.fillSprite(bgColor);

  // Weekday
  clk.setTextDatum(ML_DATUM);
  clk.setTextColor(weekfontColor, bgColor);
  clk.drawString(week(), 1, 10);
  clk.pushSprite(1, 180);
  clk.deleteSprite();

  // Month and day
  clk.createSprite(78, 25);
  clk.fillSprite(bgColor);
  clk.setTextDatum(ML_DATUM);
  clk.setTextColor(monthfontColor, bgColor);
  clk.drawString(monthDay(), 1, 10);
  clk.pushSprite(165, 180); // Position
  clk.deleteSprite();

  clk.unloadFont();
  /*** Bottom area ***/
}

// Weekday
String week() {
  String wk[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  String s = wk[weekday() - 1];
  return s;
}

// Month and day
String monthDay() {
  String s = String(month());
  s = s + "-" + day();
  return s;
}
// Hours and minutes
String hourMinute() {
  String s = num2str(hour());
  backLight_hour = s.toInt();
  s = s + ": " + num2str(minute());
  return s;
}

String num2str(int digits) {
  String s = "";
  delay(9); // Tunes the clock speed
  if (digits < 10)
    s = s + "0";
  s = s + digits;
  return s;
}

void printDigits(int digits) {
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
//------------------------------------------------------------------------------------


// NTP code, two functions ------------------------------------------------------------

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;      // NTP time is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming & outgoing packets

//------------------------------ Time zone ------------------------------------
// Day of month of the last Sunday in the given month (Sakamoto's weekday formula).
static uint8_t lastSundayDay(int year, int month) {
  static const uint8_t monthLength[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  static const uint8_t weekdayOfFirst[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

  uint8_t last = monthLength[month - 1];
  if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
    last = 29;

  int y = (month < 3) ? year - 1 : year;
  int sunday = (y + y / 4 - y / 100 + y / 400 + weekdayOfFirst[month - 1] + last) % 7;  // 0 = Sunday
  return last - sunday;
}

// UTC timestamp of the summer time boundary, last Sunday of March/October at 01:00 UTC.
static time_t euDstBoundary(int year, int month) {
  tmElements_t tm;
  tm.Year = year - 1970;
  tm.Month = month;
  tm.Day = lastSundayDay(year, month);
  tm.Hour = 1;
  tm.Minute = 0;
  tm.Second = 0;
  return makeTime(tm);
}

// True while European summer time is in effect at the given UTC timestamp.
static bool isEuSummerTime(time_t utc) {
  int y = year(utc);
  return utc >= euDstBoundary(y, 3) && utc < euDstBoundary(y, 10);
}

// UTC offset in hours that the clock applies at the given UTC timestamp.
float currentTimeZone(time_t utc) {
  if (TIMEZONE_SOURCE == 0)
    return timeZone;  // offset reported by the weather API, 0 if that request failed

  float offset = TIMEZONE_STD_OFFSET_HOURS;
  if (TIMEZONE_EU_DST && isEuSummerTime(utc))
    offset += 1.0;
  return offset;
}

time_t getNtpTime() {
  getCityTime();
  IPAddress ntpServerIP;  // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ;  // discard any previously received packets
  //Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  //Serial.print(ntpServerName);
  //Serial.print(": ");
  //Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("NTP sync succeeded");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      //Serial.println(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
      time_t utc = (time_t)(secsSince1900 - 2208988800UL);
      timeZone = currentTimeZone(utc);
      // Shift in whole seconds, a float would round a 2026 timestamp to minutes
      return utc + (time_t)(timeZone * SECS_PER_HOUR);
    }
  }
  //ESP.restart(); // Reboot when the time cannot be fetched
  Serial.println("NTP sync failed");
  return 0;  // Return 0 when the time is unknown
}

// Send a request to the NTP server
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

//--------------------------------------------------------------------------
