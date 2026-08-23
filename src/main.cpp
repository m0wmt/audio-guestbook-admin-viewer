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

#include "lvgl.h"

#include <WiFi.h>
#include <esp_now.h>

#include <Wire.h>
//#include "XPowersLib.h"


#include "driver/i2c.h"
#include "esp_err.h"

void draw(void);
bool getRawTouch(int &x, int &y);
void processTouch(void);
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len);

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


// PMK and LMK keys, must be the same both sides
static const char* PMK_KEY_STR = PMK
static const char* LMK_KEY_STR = LMK

// End ESP-NOW

// LVGL
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void lv_vertical_line (void);
void lv_application_name (void);
void lv_demo_data (void);

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

    /* 1. Status */
    lv_obj_t * label1 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label1, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label1, &lv_font_montserrat_34, LV_PART_MAIN);
    lv_label_set_text(label1, "Status");
    lv_obj_align(label1, LV_ALIGN_TOP_LEFT, 210, 35);

    /* 2. Recordings */
    lv_obj_t * label2 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label2, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label2, &lv_font_montserrat_34, LV_PART_MAIN);
    lv_label_set_text(label2, "Recordings");
    lv_obj_align(label2, LV_ALIGN_TOP_LEFT, 210, 140);

    /* 3. Disk Space */
    lv_obj_t * label3 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label3, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label3, &lv_font_montserrat_34, LV_PART_MAIN);
    lv_label_set_text(label3, "Disk Space");
    lv_obj_align(label3, LV_ALIGN_TOP_LEFT, 210, 245);

    /* 2. Up Time */
    lv_obj_t * label4 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label4, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label4, &lv_font_montserrat_34, LV_PART_MAIN);
    lv_label_set_text(label4, "Up Time");
    lv_obj_align(label4, LV_ALIGN_TOP_LEFT, 210, 350);
    
    lv_demo_data();

    lv_vertical_line();

    lv_application_name();
}


void loop() {
    static uint32_t del_time = 0;
    for (;;) {
        del_time = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(del_time));
    }    
   
    // processTouch();
    // delay(10);
}

void draw(void) {

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

/* Display flushing callback: Copies LVGL's internal buffer to your screen */
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

void lv_application_name (void) {
    lv_obj_t * app_name = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(app_name, lv_color_hex(0x008080), LV_PART_MAIN);
    lv_obj_set_style_text_font(app_name, &lv_font_montserrat_38, LV_PART_MAIN);
    lv_label_set_text(app_name, "Audio");
    lv_obj_align(app_name, LV_ALIGN_TOP_LEFT, 30, 150);

    lv_obj_t * app_name1 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(app_name1, lv_color_hex(0x008080), LV_PART_MAIN);
    lv_obj_set_style_text_font(app_name1, &lv_font_montserrat_38, LV_PART_MAIN);
    lv_label_set_text(app_name1, "Guest");
    lv_obj_align(app_name1, LV_ALIGN_TOP_LEFT, 35, 195);

    lv_obj_t * app_name2 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(app_name2, lv_color_hex(0x008080), LV_PART_MAIN);
    lv_obj_set_style_text_font(app_name2, &lv_font_montserrat_38, LV_PART_MAIN);
    lv_label_set_text(app_name2, "Book");
    lv_obj_align(app_name2, LV_ALIGN_TOP_LEFT, 40, 240);
}


void lv_vertical_line (void) {
    /*Create an array for the points of the line*/
    static lv_point_t line_points[] = { {180, 36}, {180, 430} };

    /*Create style*/
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 8);
    //lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_TEAL));
    lv_style_set_line_color(&style_line, lv_color_hex(0xFFFFFF));
    lv_style_set_line_rounded(&style_line, true);

    /*Create a line and apply the new style*/
    lv_obj_t * line1;
    line1 = lv_line_create(lv_scr_act());
    lv_line_set_points(line1, line_points, 2);     /*Set the points*/
    lv_obj_add_style(line1, &style_line, 0);
    
    //lv_obj_center(line1);
}

void lv_demo_data (void) {
    /* 1. Status */
    lv_obj_t * label1 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label1, lv_color_hex(0xD3D3D3), LV_PART_MAIN);
    lv_obj_set_style_text_font(label1, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_label_set_text(label1, "Ready");
    lv_obj_align(label1, LV_ALIGN_TOP_LEFT, 210, 80); // 45 down

    /* 2. Recordings */
    lv_obj_t * label2 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label2, lv_color_hex(0xD3D3D3), LV_PART_MAIN);
    lv_obj_set_style_text_font(label2, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_label_set_text(label2, "3");
    lv_obj_align(label2, LV_ALIGN_TOP_LEFT, 210, 185);

    /* 3. Disk Space */
    lv_obj_t * label3 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label3, lv_color_hex(0xD3D3D3), LV_PART_MAIN);
    lv_obj_set_style_text_font(label3, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_label_set_text(label3, "14.82 GB");
    lv_obj_align(label3, LV_ALIGN_TOP_LEFT, 210, 290);

    /* 2. Up Time */
    lv_obj_t * label4 = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(label4, lv_color_hex(0xD3D3D3), LV_PART_MAIN);
    lv_obj_set_style_text_font(label4, &lv_font_montserrat_26, LV_PART_MAIN);
    lv_label_set_text(label4, "01:32:21");
    lv_obj_align(label4, LV_ALIGN_TOP_LEFT, 210, 395);
}
