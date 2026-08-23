// Waveshare ESP32-S3 1.43inch AMOLED Display Development Board, 466×466, QSPI

// GFX examples that work!: 
//https://github.com/moononournation/Arduino_GFX/blob/master/examples/LVGL/LvglBenchmark/LvglBenchmark.ino
//https://github.com/moononournation/Arduino_GFX/blob/master/examples/LVGL/LVGL_Arduino_v9/LVGL_Arduino_v9.ino

/*
Build options:
    Select board ESP32S3 Dev Module
    Select USB CDC On Boot "Enabled"
    Select Flash Size 16M
    Select Partition Scheme "custom" - partitions.csv in sketch folder will be used
    Select PSRAM "OPI PSRAM"
*/


#include <Arduino.h>
#include "config.h"

#include "pins_config.h"

#include <Arduino_GFX_Library.h>
#include <ESP32Time.h>
//#include <TFT_eSPI.h>

#include "lvgl.h"
//#include "lcd_bsp.h"

#include <WiFi.h>
#include <esp_now.h>

#include <Wire.h>
//#include "XPowersLib.h"
// #include "bigFont.h"
// #include "middleFont.h"
// #include "smallFont.h"
// #include "valueFont.h"
// #include "FreeMono8pt7b.h"

#include "driver/i2c.h"
#include "esp_err.h"

void draw(void);
bool getRawTouch(int &x, int &y);
void processTouch(void);
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len);

// double rad=0.01745;

// float x[360]; //outer point
// float y[360];
// float px[360]; //ineer point
// float py[360];
// float lx[360]; //long line 
// float ly[360];
// float shx[360]; //short line 
// float shy[360];
// float tx[360]; //text
// float ty[360];

// int PPgraph[20]={0};

// int angle=0;
// int value=0;
// int chosenFont;
// int chosenColor;
// int r=118;
// int sx=-2;
// int sy=120;
// int inc=18;
// int a=0;
// int prev=0;
// String secs="00";
// int second1=0;
// int second2=0;

// int deb=0;
// int deb2=0;
// int fase=0; //stoped
// bool playing=0;

// Used only to create sprite for GFX library to draw
// TFT_eSPI tft = TFT_eSPI();
// TFT_eSprite sprite = TFT_eSprite(&tft);

// Replace with own time routines
ESP32Time rtc(0); 

// ==== LCD PINS ====
#define LCD_SDIO0 11    // From pins_config.h - works :-)
#define LCD_SDIO1 12
#define LCD_SDIO2 13
#define LCD_SDIO3 14
#define LCD_SCLK  10
#define LCD_RESET 21
#define LCD_CS    9

#define LCD_WIDTH  466
#define LCD_HEIGHT 466

// ==== TOUCH (I2C) ====
#define IIC_SDA 47  // pins_config and bsp_config
#define IIC_SCL 48
#define FT3168_I2C_ADDRESS 0x38

// Touch Screen
const int SWIPE_THRESHOLD = 40;     // Minimum pixels moved to count as a swipe
const int MAX_TAP_DURATION = 150;   // Milliseconds to differentiate tap vs swipe

int touchStartX = 0;
int touchStartY = 0;
unsigned long touchStartTime = 0;
bool isTouching = false;

// // ==== DISPLAY BUS ====
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, 
  LCD_SCLK, 
  LCD_SDIO0, 
  LCD_SDIO1,
  LCD_SDIO2, 
  LCD_SDIO3
);

// ==== DISPLAY DRIVER ====
Arduino_CO5300 *gfx = new Arduino_CO5300(
  bus, 
  LCD_RESET, 
  0,              // rotation
  LCD_WIDTH, 
  LCD_HEIGHT, 
  6, 0, 0, 0
);


// int n=0;
// int xt = 0, yt = 0;


#define BUTTON 0

// unsigned short grays[13];
// #define red  0xD041
// #define blue 0x0105
// //#define bck TFT_BLACK
// char dd[7]={'r','u','n','t','i','m','e'};

// ESP-NOW message
typedef struct struct_message {
  uint16_t recordings;
  float disk_space;
} struct_message;

// Masters's MAC address
uint8_t masterMac[] = {0xE0, 0x72, 0xA1, 0xE7, 0xE2, 0x98};

// PMK and LMK keys, must be the same both sides
static const char* PMK_KEY_STR = PMK
static const char* LMK_KEY_STR = LMK

// End ESP-NOW

// LVGL
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void lv_vertical_line (void);

    // 1. Define screen resolution
static const uint16_t screenWidth  = 466;
static const uint16_t screenHeight = 466;

// 2. Allocate buffer memory for LVGL rendering
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10]; 

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n##################################");
    Serial.println(F("ESP32 Information:"));
    Serial.printf("Core Version %s, SDK Version %s\n", ESP.getCoreVersion(), ESP.getSdkVersion());
    Serial.printf("Internal Total Heap %d, Internal Used Heap %d, Internal Free Heap %d\n", ESP.getHeapSize(), ESP.getHeapSize()-ESP.getFreeHeap(), ESP.getFreeHeap()); 
    Serial.printf("Sketch Size %d, Free Sketch Space %d\n", ESP.getSketchSize(), ESP.getFreeSketchSpace()); 
    Serial.printf("SPIRam Total heap %d, SPIRam Free Heap %d\n", ESP.getPsramSize(), ESP.getFreePsram()); 
    Serial.printf("Chip Model %s, ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion()); 
    Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
    Serial.println("##################################\n");

    /**
        ##################################
        ESP32 Information:
        Core Version 3.3.11, SDK Version v5.5.5
        Internal Total Heap 335240, Internal Used Heap 40912, Internal Free Heap 294328
        Sketch Size 1775536, Free Sketch Space 3342336
        SPIRam Total heap 8388608, SPIRam Free Heap 8386096
        Chip Model ESP32-S3, ChipRevision 2, Cpu Freq 240, SDK Version v5.5.5
        Flash Size 16777216, Flash Speed 80000000
        ##################################
    */

    WiFi.mode(WIFI_STA);
    while (WiFi.status()==WL_STOPPED){}

    Serial.println("=== ESP32 MAC Address ===");
    Serial.print("STA MAC:  ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW initialization failed");
        return;
    }

    // Set the PMK key
    esp_now_set_pmk((uint8_t *)PMK_KEY_STR);

    // Register the master as peer
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo)); // Clear junk data

    memcpy(peerInfo.peer_addr, masterMac, 6);
    peerInfo.channel = 0;
    // Setting the master device LMK key
    memcpy(peerInfo.lmk, LMK_KEY_STR, 16); 

    // for (uint8_t i = 0; i < 16; i++) {
    //     peerInfo.lmk[i] = LMK_KEY_STR[i];
    // }
    // Set encryption to true
    peerInfo.encrypt = true;
    
    // Add master as peer       
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer");
        return;
    }    
    
    Serial.println("ESP-NOW receiver ready");    

    // Register receive callback
    esp_now_register_recv_cb(OnDataRecv);


    pinMode(BUTTON, INPUT_PULLUP); 
    rtc.setTime(0,0,0,10,23,2026,0); 
    // sprite.createSprite(400,240);
    


    if (!gfx->begin()) {
        Serial.println("Display Init Failed!");
    }

    gfx->setBrightness(140);
    gfx->fillScreen(RGB565_TEAL);
    delay(1000);
    gfx->fillScreen(RGB565_BLACK);

    // Touch screen init
    Wire.setPins(IIC_SDA, IIC_SCL);
    Wire.begin();


    // int co = 220;
    // for(int i = 0; i < 13; i++)
    // {
    //     grays[i]=tft.color565(co, co, co);
    //     co=co-20;
    // }

    // for(int i = 0; i < 360; i++)
    // {
    //     x[i]=(r*cos(rad*i))+sx;
    //     y[i]=(r*sin(rad*i))+sy;
    //     px[i]=((r-5)*cos(rad*i))+sx;
    //     py[i]=((r-5)*sin(rad*i))+sy;

    //     lx[i]=((r-24)*cos(rad*i))+sx;
    //     ly[i]=((r-24)*sin(rad*i))+sy;

    //     shx[i]=((r-12)*cos(rad*i))+sx;
    //     shy[i]=((r-12)*sin(rad*i))+sy;

    //     tx[i]=((r+28)*cos(rad*i))+sx;
    //     ty[i]=((r+28)*sin(rad*i))+sy;
    // }

    // for (int i=0;i<20;i++)
    //     PPgraph[i]=random(1,12);

    //draw();

    Serial.println("Initialising LVGL...");

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

    /* Initialize the display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.sw_rotate = 1;                 // Rotate the display
    disp_drv.rotated = LV_DISP_ROT_270;
    lv_disp_drv_register(&disp_drv);
    
    // Set background colour
    lv_obj_t * scr = lv_scr_act();
	lv_obj_set_style_bg_color(scr, lv_palette_main(LV_PALETTE_NONE), LV_PART_MAIN);

    /* 1. Create a simple text label */
    lv_obj_t * label1 = lv_label_create(lv_scr_act());

    // Set text colour to white (hex 0xFF0000)
    lv_obj_set_style_text_color(label1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    // Set font size/family (requires a declared font, e.g., montserrat font)
    lv_obj_set_style_text_font(label1, &lv_font_montserrat_28, LV_PART_MAIN);

    lv_label_set_text(label1, "Hello ESP32!");
    lv_obj_align(label1, LV_ALIGN_TOP_MID, 120, 80);
    
    lv_vertical_line();
}


void loop() {
    static uint32_t del_time = 0;
    for (;;) {
        del_time = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(del_time));
    }    
    // angle++;
    // if (angle>356)
    //     angle=0;

    // for (int i=0;i<20;i++)
    //     PPgraph[i]=random(1,12);

    // if (digitalRead(BUTTON)==0) {
    //     if (deb==0) {
    //         deb=1; 
    //         fase++;
    //         if (fase==1) {
    //             playing=1;
    //             rtc.setTime(0,0,0,10,23,2026,0);
    //         }

    //         if (fase==2)
    //             playing=0;

    //         if (fase==3) {
    //             fase=0;
    //         }
  
    //         draw();
    //     }
    // } else 
    //     deb=0;
 
    // second1=rtc.getSecond();

    // if (second1!=second2) {
    //     second2=second1;
    //     prev++;
    
    //     if(prev>6)
    //         prev=0;
    // }

    // if (playing)
    //     draw();  

    // processTouch();
    // delay(10);
}

void draw(void) {

//  sprite.fillSprite(0);
//   sprite.fillRect(54,120,24,4,TFT_RED);

//     for(int j=0;j<20;j++)
//     for(int i=0;i<PPgraph[j];i++)
//     sprite.fillRect(190+(j*6),90-(i*4),4,3,grays[6]);
  
//   sprite.fillRect(180,136,120,3,grays[8]);
//   sprite.fillRect(186,130,3,34,grays[8]);
//   sprite.fillRect(136,0,40,135,blue);
//   sprite.fillRect(136,224,40,16,blue);

//   sprite.fillCircle(372,76,28,blue);

 
//   sprite.drawArc(372,76,24,10,0,angle,0x130F,blue);

//   sprite.setTextDatum(8);
//   sprite.loadFont(smallFont);
//   sprite.setTextColor(grays[7],bck);
//   sprite.drawString("AMOLED",342,124);
//   sprite.drawString("*HRS*",178,218);
//   sprite.unloadFont();

//    sprite.loadFont(midleFont);

//    for(int i=0;i<120;i++)
//  {
//    a=angle+(i*3);
//    if(a>359)
//    a=(angle+(i*3))-360;
   
//    sprite.drawPixel(x[a],y[a],grays[6]);

//    if(i%3==0)
//    sprite.drawWedgeLine(x[a],y[a],x[a]-6,y[a],1,2,grays[5],bck);

//    if(i%6==0)
//    sprite.drawWedgeLine(x[a],y[a],x[a]-18,y[a],2,3,grays[4],bck);
//    if(i%12==0){
//    sprite.drawWedgeLine(x[a],y[a],x[a]-30,y[a],2,4,grays[3],bck);
//    }

// }
    
//   sprite.setTextDatum(4);
//   sprite.setTextColor(grays[2],grays[9]);
  
//   for(int i=0;i<7;i++)
//   {
//     sprite.fillSmoothRoundRect(186+(i*30),2,26,26,3,grays[9],bck);
//     sprite.drawString(String(dd[i]),186+((i+1)*30)-17,16);
//   }
//   sprite.unloadFont();


//   sprite.drawWedgeLine(199+(prev*30),35,199+(prev*30),40,1,3,grays[3],bck); ////////////
//   sprite.setTextDatum(0);
//   sprite.setTextColor(grays[1],bck);
//   sprite.loadFont(bigFont);
//   if(fase==0)
//   sprite.drawString("00:00",196,150);
//   else
//   sprite.drawString(rtc.getTime().substring(3,8),196,150);
//   sprite.unloadFont();

//   sprite.setTextDatum(0);
//   sprite.setTextColor(grays[4],bck);
//   sprite.loadFont(midleFont);
//   sprite.drawString("BADGER",190,104);  ////////////////////////date hard coded
//   sprite.setTextDatum(4);
//   sprite.fillRect(0,145,50,30,grays[10]);
//   sprite.setTextColor(grays[3],grays[10]);
//   sprite.drawString("mil",25,162); 
   
//   sprite.unloadFont();


//   sprite.setTextDatum(4);
//   sprite.setTextColor(grays[2],bck);
//   sprite.loadFont(valueFont);
//   if(fase==0)
//   sprite.drawString("00",24,124);
//   else
//   sprite.drawString(String(rtc.getMillis()/10),24,124);
//   sprite.setTextColor(grays[4],bck);
//      if(fase==0)
//      sprite.drawString("00",154,174);
//    else
//    sprite.drawString(rtc.getTime().substring(0,2),154,174);   /// /////////////////////////////////seconds
//   sprite.unloadFont();

//   sprite.setTextColor(grays[8],bck);
//   sprite.drawString("CAN YOU READ THIS",346,128);
  
//  gfx->draw16bitBeRGBBitmap(40,120,(uint16_t*)sprite.getPointer(),400,240);
}

// Read touch screen
bool getRawTouch(int &x, int &y) {
    Wire.beginTransmission(FT3168_I2C_ADDRESS);
    Wire.write(0x02); // Touch points status register
    Wire.endTransmission(false);    
    Wire.requestFrom(FT3168_I2C_ADDRESS, 5);
    if (Wire.available() >= 5) {
        uint8_t touch_points = Wire.read() & 0x0F;
        uint8_t high_x = Wire.read();
        uint8_t low_x = Wire.read();
        uint8_t high_y = Wire.read();
        uint8_t low_y = Wire.read();

        if (touch_points > 0) {
            x = ((high_x & 0x0F) << 8) | low_x;
            y = ((high_y & 0x0F) << 8) | low_y;
            Serial.printf("Touch detected! X: %d, Y: %d\n", x, y);

            return true;
        }

    }
        
    return false; 
}

// Needs work for when we stop touching the screen!
void processTouch(void) {
    int currentX = 0;
    int currentY = 0;
    bool currentTouchState = getRawTouch(currentX, currentY);

    // Detect Touch Start (Finger down)
    if (currentTouchState && !isTouching) {
        touchStartX = currentX;
        touchStartY = currentY;
        touchStartTime = millis();
        isTouching = true;
        Serial.printf("Touch Start X: %d, Y: %d\n", currentX, currentY);
    }
    
    // Detect Touch Release (Finger lifted)
    else if (!currentTouchState && isTouching) {
        isTouching = false;
        unsigned long duration = millis() - touchStartTime;

        // Fetch last known positions or use standard final register
        // For a swipe, calculate delta from start to end
        int deltaX = 0;
        int deltaY = 0;
        float distance = 0;
        if (currentX != 0 && currentY != 0) {
            deltaX = currentX - touchStartX;
            deltaY = currentY - touchStartY;

            // Calculate total linear movement distance
            distance = sqrt((deltaX * deltaX) + (deltaY * deltaY));
        }
        

        Serial.printf("Touch Start X: %d, Y: %d / Touch End X: %d, Y: %d, Distance: %f\n", touchStartX, touchStartY, deltaX, deltaY, distance);

        // Validation Check
        if (distance > SWIPE_THRESHOLD) {
            
            // Check if horizontal movement dominates vertical movement
            if (abs(deltaX) > abs(deltaY)) {
                if (deltaX > 0) {
                    Serial.println("GESTURE: Swipe Right");
                } else {
                    Serial.println("GESTURE: Swipe Left");
                }
            } 
            // Vertical movement dominates
            else {
                if (deltaY > 0) {
                    Serial.println("GESTURE: Swipe Down"); // Inverted depending on driver setup
                } else {
                    Serial.println("GESTURE: Swipe Up");
                }
            }
        } else if (duration < MAX_TAP_DURATION) {
            Serial.println("GESTURE: Simple Tap");
        }
    }
}

/**
 * Recieve data from ESP-NOW server callback
 */
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    struct_message *data = (struct_message *)incomingData;
    Serial.printf("Data received: counter=%d, temp=%.2f\n", data->recordings, data->disk_space);
}

/* Display flushing callback: Copies LVGL's internal buffer to your TFT screen */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    #ifndef DIRECT_RENDER_MODE
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);

        #if (LV_COLOR_16_SWAP != 0)
            gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
        #else
            gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
        #endif
    #endif // #ifndef DIRECT_RENDER_MODE

    lv_disp_flush_ready(disp_drv);
}

void lv_vertical_line (void) {
    /*Create an array for the points of the line*/
    static lv_point_t line_points[] = { {10, 10}, {10, 270} };

    /*Create style*/
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 8);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_TEAL));
    lv_style_set_line_rounded(&style_line, true);

    /*Create a line and apply the new style*/
    lv_obj_t * line1;
    line1 = lv_line_create(lv_scr_act());
    lv_line_set_points(line1, line_points, 2);     /*Set the points*/
    lv_obj_add_style(line1, &style_line, 0);
    lv_obj_center(line1);
}