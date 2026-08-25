// Waveshare ESP32-S3 1.43inch AMOLED Display Development Board, 466×466, QSPI

// GFX examples that work!: 
//https://github.com/moononournation/Arduino_GFX/blob/master/examples/LVGL/LvglBenchmark/LvglBenchmark.ino
//https://github.com/moononournation/Arduino_GFX/blob/master/examples/LVGL/LVGL_Arduino_v9/LVGL_Arduino_v9.ino

// TODO: Watch to create task for each item
//https://www.youtube.com/watch?v=g9j8FfF2GJ8&t=273s

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

#include <TFT_eSPI.h>
#include "Free_Fonts.h" // Include the header file attached to this sketch

#include <Arduino_GFX_Library.h>

#include <WiFi.h>
#include <esp_now.h>

#include <Wire.h>


void draw(void);
bool getRawTouch(int &x, int &y);
void processTouch(void);
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len);
static void updateStatus(const uint8_t mode);

// Used only to create sprite for GFX library to draw
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// // ==== LCD PINS ====
// #define LCD_SDIO0 11    // From pins_config.h - works :-)
// #define LCD_SDIO1 12
// #define LCD_SDIO2 13
// #define LCD_SDIO3 14
// #define LCD_SCLK  10
// #define LCD_RESET 21
// #define LCD_CS    9

// #define LCD_WIDTH  466
// #define LCD_HEIGHT 466

// // ==== TOUCH (I2C) ====
// #define IIC_SDA 47  // pins_config and bsp_config
// #define IIC_SCL 48
// #define FT3168_I2C_ADDRESS 0x38

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

#define BUTTON 0


typedef enum { // State/mode of the audio guestbook
    ERROR,
    INITIALISING,
    READY,
    RECORDMESSAGEPROMPT,
    RECORDING,
    PLAYING,
    LEFT_OFF_HOOK
} button_mode_t;

// ESP-NOW message
typedef struct struct_message {
    uint8_t mode;
    uint16_t recordings;
    uint64_t disk_space;
    unsigned long last_time;
} struct_message;

struct_message myData = {INITIALISING, 0, 0, 0};

char status[24];

// PMK and LMK keys, must be the same both sides
static const char* PMK_KEY_STR = PMK
static const char* LMK_KEY_STR = LMK

// End ESP-NOW

#define TFT_TEAL 0x008080
#define OFF_WHITE 0xD3D3D3

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

    // Set encryption to true
    peerInfo.encrypt = true;
    
    // Add master as peer       
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer");
        return;
    }    
    
    Serial.println("ESP-NOW receiver ready");    

    // Register receive callback
    esp_now_register_recv_cb(onDataRecv);


    pinMode(BUTTON, INPUT_PULLUP); 
    
    sprite.createSprite(466,466);
    
    memset(status, '\0', sizeof(status));

    // Set up display
    if (!gfx->begin()) {
        Serial.println("Display Init Failed!");
    }

    gfx->setBrightness(100);
    gfx->fillScreen(RGB565_TEAL);
    delay(1000);
    gfx->fillScreen(RGB565_BLACK);

    // Touch screen init
    Wire.setPins(IIC_SDA, IIC_SCL);
    Wire.begin();

    draw();
}


void loop() {

   
    // processTouch();
    // delay(10);
}

void draw(void) {
    sprite.fillSprite(0);
    sprite.fillRoundRect(177, 6, 6, 460, 4, RGB565_WHITE);
    
    // sprite.loadFont(valueFont);
    sprite.setFreeFont(FSB24);     
    sprite.setTextColor(RGB565_TEAL);
    sprite.drawString("Audio", 23, 140);
    sprite.drawString("Guest", 33, 200);
    sprite.drawString("Book", 43, 260);
    sprite.unloadFont();

    sprite.setFreeFont(FSSB18);
    sprite.setTextColor(RGB565_WHITE);
    sprite.drawString("Status", 207, 30);
    sprite.drawString("Recordings", 207, 140);
    sprite.drawString("Disk Space", 207, 255);
    sprite.drawString("Up Time", 207, 365);
    sprite.unloadFont();

    // sprite.loadFont(middleFont);
    sprite.setFreeFont(FSS18);     
    sprite.setTextColor(RGB565_LIGHTGREY);

    updateStatus(myData.mode);
    sprite.drawString(status, 207, 70);
    memset(status, '\0', sizeof(status));

    char recordings_buffer[10];
    sprintf(recordings_buffer, "%u", myData.recordings);
    sprite.drawString(recordings_buffer, 207, 180);

    uint64_t bytes = myData.disk_space;
    double humanBytes;
    int i = 0;
	char *suffix[] = {"B", "KB", "MB", "GB", "TB"};
    char length = sizeof(suffix) / sizeof(suffix[0]);
    if (myData.disk_space > 1024) {
		for (i = 0; (bytes / 1024) > 0 && i<length-1; i++, bytes /= 1024)
			humanBytes = bytes / 1024.0;
    }

    char diskspace_buffer[25];
    sprintf(diskspace_buffer, "%.02lf %s", humanBytes, suffix[i]);
    sprite.drawString(diskspace_buffer, 207, 295);

    char uptime_buffer[10];
    sprintf(uptime_buffer, "%02d:%02d:%02d", (myData.last_time / 1000) / 3600, ((myData.last_time / 1000) % 3600) / 60,
            ((myData.last_time / 1000) % 3600) % 60);
    sprite.drawString(uptime_buffer, 207, 405);
    sprite.unloadFont();

    gfx->draw16bitBeRGBBitmap(0, 0, (uint16_t*)sprite.getPointer(), 466, 466);  
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
 * Recieve data from ESP-NOW server callback and update the display
 */
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    struct_message *data = (struct_message *)incomingData;
    Serial.printf("Data received: recordings=%u, disk_space=%llu\n", data->recordings, data->disk_space);
    Serial.printf("Data received: mode=%d, last_time=%lu\n", data->mode, data->last_time);

    myData.disk_space = data->disk_space;
    myData.last_time = data->last_time;
    myData.mode = data->mode;
    myData.recordings = data->recordings;

    draw();
}


/**
 * @brief 
 */
 static void updateStatus(const uint8_t mode) {
    switch (mode) {
        case ERROR:
            sprintf(status, "Error");
            break;

        case INITIALISING:
            sprintf(status, "Initialising");
            break;

        case READY:
            sprintf(status, "Ready");
            break;

        case RECORDMESSAGEPROMPT:
            sprintf(status, "Record Prompt");
            break;

        case RECORDING:
            sprintf(status, "Recording");
            break;

        case PLAYING:
            sprintf(status, "Playing");
            break;

        case LEFT_OFF_HOOK:
            sprintf(status, "Off The Hook!");
            break;

        default:
            sprintf(status, "Undefined!");
            break;
    }
}