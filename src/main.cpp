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

#define DEBUG true      // to turn on/off printf statements

void draw(void);
bool get_raw_touch(int &x, int &y);
void process_touch(void);
void esp_now_data_recv(const esp_now_recv_info_t *info, const uint8_t *incoming_data, int len);
static uint16_t update_status(const uint8_t status);
void draw_clock(void);
bool gett_touch(uint16_t *x, uint16_t *y, uint8_t *gesture);
uint8_t i2c_read(uint8_t addr);
uint8_t i2c_read_continuous(uint8_t addr, uint8_t *data, uint32_t length);

// Used only to create sprite for GFX library to draw
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

// Touch Screen
const int SWIPE_THRESHOLD = 40;     // Minimum pixels moved to count as a swipe
const int MAX_TAP_DURATION = 150;   // Milliseconds to differentiate tap vs swipe

int touchStartX = 0;
int touchStartY = 0;
unsigned long touchStartTime = 0;
bool isTouching = false;

volatile bool touchPending = false;

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

enum GESTURE
{
    None = 0x00,       
    SlideDown = 0x08,  
    SlideUp = 0x04,    
    SlideLeft = 0x01,  
    SlideRight = 0x02, 
    SingleTap = 0x00,  
    DoubleTap = 0x10,  
    LongPress = 0x00   
};

#define BUTTON 0


typedef enum { // State/mode of the audio guestbook
    ERROR,
    INITIALISING,
    READY,
    RECORDMESSAGEPROMPT,
    RECORDING,
    PLAYING,
    LEFT_OFF_HOOK,
    OFFLINE
} button_mode_t;

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


// Set only a flag in the ISR. Perform the I2C transaction in the main loop.
void IRAM_ATTR on_touch_interrupt() {
  touchPending = true;
  Serial.println("touched");
}

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

    sprite.createSprite(466,466);
    
    memset(phone_status, '\0', sizeof(phone_status));

    // Set up display
    if (!gfx->begin()) {
        if (DEBUG) {
            Serial.println("Display Init Failed!");
        }
    }

    gfx->setBrightness(100);
    gfx->fillScreen(RGB565_TEAL);
    delay(1000);
    gfx->fillScreen(RGB565_BLACK);

    // //----------------
    // int8_t _sda, _scl, _rst, _int;    
    // // FT3168 touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);
    // _sda = IIC_SDA;
    // _scl = IIC_SCL;
    // _rst = TOUCH_RST;
    // _int = TOUCH_INT;

    // // Initialize I2C
    // if (_sda != -1 && _scl != -1)
    // {
    //     Wire.setPins(_sda, _scl);
    //     Wire.begin();
    //     Serial.println("Wire begin");
    // }
    // else
    // {
    //     Wire.begin();
    // }

    // // Int Pin Configuration
    // if (_int != -1)
    // {
    //     pinMode(_int, OUTPUT);
    //     digitalWrite(_int, HIGH); // 高电平
    //     delay(1); 
    //     digitalWrite(_int, LOW); // 低电平
    //     delay(1);
    // }

    // // Reset Pin Configuration
    // if (_rst != -1)
    // {
    //     pinMode(_rst, OUTPUT);
    //     digitalWrite(_rst, LOW);
    //     delay(10);
    //     digitalWrite(_rst, HIGH);
    //     delay(300);
    // }

    // // Initialize Touch
    // Wire.beginTransmission(FT3168_I2C_ADDRESS);
    // Wire.write(0x00);
    // Wire.write(0x00);
    // Wire.endTransmission();
    // //-------------

    // Touch screen init
    Wire.setPins(IIC_SDA, IIC_SCL);
    Wire.begin();

    // uint8_t data = 0x00;
    // I2C_writr_buff(FT3168_I2C_ADDRESS,0x00,&data,1); //Switch to normal mode


    draw();    

  // Configure the interrupt pin for a falling-edge trigger.
  //pinMode(TOUCH_RST, LOW);
  // pinMode(TOUCH_INT, INPUT_PULLUP);
  // attachInterrupt(digitalPinToInterrupt(TOUCH_INT), onTouchInterrupt, FALLING);    

    inactive_timer = millis();  // start inactivity timer 
}


void loop() {

//   bool touched;
//   uint8_t gesture;
//   uint16_t x, y;

//   touched = getTouch(&x, &y, &gesture);

//     if(touched != 0) {
//         Serial.print("getTouch: "); Serial.println(gesture);
//     }

    // processTouch();
    // delay(100);

    // Check for audio guestbook admin server binactivity
    if (millis() >= inactive_timer + HEARTBEAT) {  // We've been inactive for 'n' minutes, show on display
        // Update the display after setting status=inactive
        esp_now_message.status = OFFLINE;
        
        // Draw screen to show offline
        draw();
        
        // Reset inactive time just in case it come back
        inactive_timer = millis();
    }    
}

bool get_touch(uint16_t *x, uint16_t *y, uint8_t *gesture)
{
    bool FingerIndex = false;
    FingerIndex = (bool)i2c_read(0x02);
    //Serial.printf("FingerIndex: %d\n",FingerIndex);
    *gesture = i2c_read(0xD1);
    // if (!(*gesture == SlideUp || *gesture == SlideDown))
    // {
    //     // printf("AA\n");
    //     *gesture = None;
    // }

    uint8_t data[4];
    i2c_read_continuous(0x03, data, 4);
    *x = ((data[0] & 0x0F) << 8) | data[1];
    // i2c_read_continuous(0x02, data, 4);
    // Wire.endTransmission(false);
    // *y = ((data[2] & 0x0F) << 16) | data[3]; //读取的y值反向
    *y = ((data[2] & 0x0F) << 8) | data[3];

    // *x = 240 - *x;

    return FingerIndex;
}

uint8_t i2c_read(uint8_t addr)
{
    uint8_t rdData;
    uint8_t rdDataCount;
    do
    {
        Wire.beginTransmission(FT3168_I2C_ADDRESS);
        Wire.write(addr);
        Wire.endTransmission(false); // Restart
        rdDataCount = Wire.requestFrom(FT3168_I2C_ADDRESS, 1);
    } while (rdDataCount == 0);
    while (Wire.available())
    {
        rdData = Wire.read();
    }
    return rdData;
}

uint8_t i2c_read_continuous(uint8_t addr, uint8_t *data, uint32_t length)
{
    Wire.beginTransmission(FT3168_I2C_ADDRESS);
    Wire.write(addr);
    if (Wire.endTransmission(true)) return -1;
    Wire.requestFrom(FT3168_I2C_ADDRESS, length);
    for (int i = 0; i < length; i++)
    {
        *data++ = Wire.read();
    }
    return 0;
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

// Read touch screen
bool get_raw_touch(int &x, int &y) {
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
            if (DEBUG) {
                Serial.printf("Touch detected! X: %d, Y: %d\n", x, y);
            }

            return true;
        }

    }
        
    return false; 
}

// Needs work for when we stop touching the screen!
void process_touch(void) {
    int currentX = 0;
    int currentY = 0;
    bool currentTouchState = get_raw_touch(currentX, currentY);

    // Detect Touch Start (Finger down)
    if (currentTouchState && !isTouching) {
        touchStartX = currentX;
        touchStartY = currentY;
        touchStartTime = millis();
        isTouching = true;
        if (DEBUG) {
            Serial.printf("Touch Start X: %d, Y: %d\n", currentX, currentY);
        }
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
        

        if (DEBUG) {
            Serial.printf("Touch Start X: %d, Y: %d / Touch End X: %d, Y: %d, Distance: %f\n", touchStartX, touchStartY, deltaX, deltaY, distance);
        }

        // Validation Check
        if (distance > SWIPE_THRESHOLD) {
            
            // Check if horizontal movement dominates vertical movement
            if (abs(deltaX) > abs(deltaY)) {
                if (deltaX > 0) {
                    if (DEBUG) {
                        Serial.println("GESTURE: Swipe Right");
                    }
                } else {
                    if (DEBUG) {
                        Serial.println("GESTURE: Swipe Left");
                    }
                }
            } 
            // Vertical movement dominates
            else {
                if (deltaY > 0) {
                    if (DEBUG) {
                        Serial.println("GESTURE: Swipe Down"); // Inverted depending on driver setup
                    }
                } else {
                    if (DEBUG) {
                        Serial.println("GESTURE: Swipe Up");
                    }
                }
            }
        } else if (duration < MAX_TAP_DURATION) {
            if (DEBUG) {
                Serial.println("GESTURE: Simple Tap");
            }
        }
    }
}

/**
 * Recieve data from ESP-NOW server callback and update the display
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

    draw();

    // When we get a message from the phone reset the inactive_timer to now
    inactive_timer = millis();    
}


/**
 * @brief 
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

void draw_clock(void) {
    const int CENTER_X = 233;
    const int CENTER_Y = 233;
    const int RADIUS = 233;
    const int HOUR_LEN = 30;
    const int MIN_LEN = 40;
    const int SEC_LEN = 55;    

    sprite.fillSprite(0);

  for (int i = 0; i < 60; i++) {
    float angle = i * 6 * M_PI / 180.0;
    int x1 = CENTER_X + (RADIUS - 8) * cos(angle - M_PI / 2);
    int y1 = CENTER_Y + (RADIUS - 8) * sin(angle - M_PI / 2);
    int x2 = CENTER_X + (RADIUS - (i % 5 == 0 ? 22 : 14)) * cos(angle - M_PI / 2);
    int y2 = CENTER_Y + (RADIUS - (i % 5 == 0 ? 22 : 14)) * sin(angle - M_PI / 2);
    sprite.drawLine(x1, y1, x2, y2, RGB565_WHITE);
  }

  sprite.setTextColor(RGB565_WHITE, RGB565_BLACK);
  sprite.setTextSize(2);
  for (int h = 1; h <= 12; h++) {
    float angle = (h * 30) * M_PI / 180.0;
    int tx = CENTER_X + (RADIUS - 38) * cos(angle - M_PI / 2) - 10;
    int ty = CENTER_Y + (RADIUS - 38) * sin(angle - M_PI / 2) - 8;
    sprite.setCursor(tx, ty);
    sprite.print(h);
  }
  
  gfx->draw16bitBeRGBBitmap(0, 0, (uint16_t*)sprite.getPointer(), 466, 466);  
  sprite.setTextSize(0);
}    
