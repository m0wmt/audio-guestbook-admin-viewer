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

#include <driver/i2c.h>

#define DEBUG false      // to turn on/off printf statements

void draw_logo(void);
void draw_display(void);
void esp_now_data_recv(const esp_now_recv_info_t *info, const uint8_t *incoming_data, int len);
static uint16_t update_status(const uint8_t status);

// Library used only to create sprite for GFX library to draw
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

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

#define BUTTON 0    // Not used currently

// State of the audio guestbook
typedef enum { 
    ERROR,
    INITIALISING,
    READY,
    RECORDMESSAGEPROMPT,
    RECORDING,
    PLAYING,
    LEFT_OFF_HOOK,
    OFFLINE
} guestbook_status_t;

// ESP-NOW message
typedef struct {
    uint64_t disk_space;
    unsigned long last_time;
    uint16_t recordings;
    uint8_t status;
} struct_message_t;

struct_message_t esp_now_message = {.disk_space = 0, .last_time = 0, .recordings = 0, .status = OFFLINE};

char phone_status[24];

// PMK and LMK keys, must be the same both sides
static const char* PMK_KEY_STR = PMK
static const char* LMK_KEY_STR = LMK

// End ESP-NOW

#define HEARTBEAT 120000                // 2 minutes
static uint32_t inactive_timer = 0;     // inactivity run time timer
static uint8_t counter = 0;


void setup() {
    if (DEBUG) {
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
    }

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

    sprite.createSprite(466,466);

    // Set up display
    if (!gfx->begin()) {
        if (DEBUG) {
            Serial.println("Display Init Failed!");
        }
    }

    gfx->setBrightness(100);
    gfx->fillScreen(RGB565_BLACK);
    draw_logo();

    WiFi.mode(WIFI_STA);
    while (WiFi.status()==WL_STOPPED){}

    if (DEBUG) {
        Serial.println("=== ESP32 MAC Address ===");
        Serial.print("STA MAC:  ");
        Serial.println(WiFi.macAddress());
    }

    if (esp_now_init() != ESP_OK) {
        if (DEBUG) {
            Serial.println("ESP-NOW initialization failed");
        }
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
        if (DEBUG) {
            Serial.println("Failed to add peer");
        }
        return;
    }    
    
    if (DEBUG) {
        Serial.println("ESP-NOW receiver ready");    
    }

    // Register receive callback
    esp_now_register_recv_cb(esp_now_data_recv);


    pinMode(BUTTON, INPUT_PULLUP); 

    memset(phone_status, '\0', sizeof(phone_status));

    // Touch screen init
    Wire.setPins(IIC_SDA, IIC_SCL);
    Wire.begin();

    delay(2000);    // small display so we can see the logo :-)
    
    draw_display();    

    inactive_timer = millis();  // start inactivity timer 
}


void loop() {
    // Check for audio guestbook admin server binactivity
    if (millis() >= inactive_timer + HEARTBEAT) {  // We've been inactive for 'n' minutes, show on display
        // Update the display after setting status=inactive
        esp_now_message.status = OFFLINE;
        
        // Draw screen to show offline
        draw_display();
        
        // Reset inactive time just in case it come back
        inactive_timer = millis();
    }    
}

/**
 * Draw splash-screen at start of program
 */
void draw_logo(void) {
    sprite.fillSprite(0);
    
    sprite.setFreeFont(FSB24);     
    sprite.setTextColor(RGB565_TEAL);
    sprite.drawString("Audio", 161, 167);
    sprite.drawString("Guestbook", 123, 250);
    sprite.unloadFont();

    gfx->draw16bitBeRGBBitmap(0, 0, (uint16_t*)sprite.getPointer(), 466, 466);  
}

/**
 * @brief Draw the screeen
 */
void draw_display(void) {
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

    uint16_t status_colour = update_status(esp_now_message.status);
    sprite.setTextColor(status_colour);
    sprite.drawString(phone_status, 207, 70);
    memset(phone_status, '\0', sizeof(phone_status));

    // Reset colour just in case status changed it
    sprite.setTextColor(RGB565_LIGHTGREY);

    char recordings_buffer[10];
    sprintf(recordings_buffer, "%u", esp_now_message.recordings);
    sprite.drawString(recordings_buffer, 207, 180);

    uint64_t bytes = esp_now_message.disk_space;
    double humanBytes;
    int i = 0;
	char *suffix[] = {"B", "KB", "MB", "GB", "TB"};
    char length = sizeof(suffix) / sizeof(suffix[0]);
    if (esp_now_message.disk_space > 1024) {
		for (i = 0; (bytes / 1024) > 0 && i<length-1; i++, bytes /= 1024)
			humanBytes = bytes / 1024.0;
    }

    char diskspace_buffer[25];
    sprintf(diskspace_buffer, "%.02lf %s", humanBytes, suffix[i]);
    sprite.drawString(diskspace_buffer, 207, 295);

    char uptime_buffer[10];
    sprintf(uptime_buffer, "%02d:%02d:%02d", (esp_now_message.last_time / 1000) / 3600, ((esp_now_message.last_time / 1000) % 3600) / 60,
            ((esp_now_message.last_time / 1000) % 3600) % 60);
    sprite.drawString(uptime_buffer, 207, 405);
    sprite.unloadFont();

    gfx->draw16bitBeRGBBitmap(0, 0, (uint16_t*)sprite.getPointer(), 466, 466);  
}

/**
 * @brief Receive data from ESP-NOW server callback and update the display
 */
void esp_now_data_recv(const esp_now_recv_info_t *info, const uint8_t *incoming_data, int len) {
    struct_message_t *data = (struct_message_t *)incoming_data;

    if (DEBUG) {
        Serial.printf("Data received: recordings=%u, disk_space=%llu\n", data->recordings, data->disk_space);
        Serial.printf("Data received: mode=%d, last_time=%lu\n", data->status, data->last_time);
    }

    esp_now_message.disk_space = data->disk_space;
    esp_now_message.last_time = data->last_time;
    esp_now_message.status = data->status;
    esp_now_message.recordings = data->recordings;

    draw_display();

    // When we get a message from the phone reset the inactive_timer to now
    inactive_timer = millis();    
}


/**
 * @brief Update the status buffer for printing to the screen and return what colour
 * to draw the text depending on status
 */
 static uint16_t update_status(const uint8_t status) {
    uint16_t status_colour = RGB565_LIGHTGREY;

    switch (status) {
        case ERROR:
            sprintf(phone_status, "Error");
            break;

        case INITIALISING:
            sprintf(phone_status, "Initialising");
            break;

        case READY:
            sprintf(phone_status, "Ready");
            break;

        case RECORDMESSAGEPROMPT:
            sprintf(phone_status, "Record Prompt");
            break;

        case RECORDING:
            sprintf(phone_status, "Recording");
            status_colour = RGB565_GREEN;
            break;

        case PLAYING:
            sprintf(phone_status, "Playing");
            break;

        case LEFT_OFF_HOOK:
            sprintf(phone_status, "Off The Hook");
            status_colour = RGB565_ORANGE;
            break;

        case OFFLINE:
            sprintf(phone_status, "Offline");
            status_colour = RGB565_RED;
            break;

        default:
            sprintf(phone_status, "Undefined!");
            break;
    }

    return status_colour;
}