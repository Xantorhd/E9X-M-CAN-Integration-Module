// Centre console section

// Enable use of M3 centre console switch block.POWER and DSC OFF switches. Control POWER LED when MDrive is on.
// 1. Request MDrive when POWER button is pressed.
// 2. Illuminate POWER LED when MDrive is on.
// 3. Request full DSC OFF after holding DSC OFF button. Turn everything back on with a short press.


// PT-CAN section

// 1. Toggles sport mode in the IKM0S MSD81 DME in the absence of a DSCM90 or DSCM80 ZB by re-creating 0x1D9 message.
// 2. Toggle DTC mode through long press of M key. Short press turns DTC off again.
// 3. Monitor steering wheel switches and request MDrive when Source/M is pressed.
// 4. Monitor DSC program status.
// 5. Monitor Ignition status.
// 6. Monitor and indicate front foglight status.
// 7. Monitor and indicate FTM status in KOMBI by flashing tyre icon when initializing.


// K-CAN section

// 1. Create missing 0x206 message to animate DCT KOMBI shift lights.
// 2. Monitor MDrive status as broadcast by 1M DME.
// 3. Create the two missing messages required to use an F series ZBE (KKCAN) in an E6,7,8,9X car.
//    *Should* work with:
//    6582 9267955 - 4-pin MEDIA button. **Tested - controller build date 19.04.13
//    6131 9253944 - 4-pin CD button
//    6582 9206444 - 10-pin CD button
//    6582 9212449 - 10-pin CD button
// 4. Turn on driver's seat heating when ignition is turned on and below configured temperature treshold.
// Credit to Trevor for providing insight into 0x273, 0x277, 0x2CA and 0x0AA http://www.loopybunny.co.uk/CarPC/k_can.html


// Using a slightly streamlined version of Cory's library https://github.com/coryjfowler/MCP_CAN_lib
// Hardware used: CANBED V1.2c http://docs.longan-labs.cc/1030008/ (32U4+MCP2515+MCP2551, LEDs removed) and Generic 16MHz MCP2515 CAN shield.

#include "src/mcp_can.h"
#include <avr/power.h>

/***********************************************************************************************************************************************************************************************************************************************
  Board adjustment section.
***********************************************************************************************************************************************************************************************************************************************/

MCP_CAN PTCAN(17), KCAN(9);                                                                                                         // CS pins. Adapt to your board
#define PTCAN_INT_PIN 7                                                                                                             // INT pins. Adapt to your board
#define KCAN_INT_PIN 8                                                                              
#define POWER_LED_PIN 4
#define POWER_SWITCH_PIN 5
#define DSC_SWITCH_PIN 6
#define FOG_LED_PIN 12
const int MCP2515_PTCAN = 1;                                                                                                        // Set 1 for 16MHZ or 2 for 8MHZ
const int MCP2515_KCAN = 1;

/***********************************************************************************************************************************************************************************************************************************************
  Program adjustment section.
***********************************************************************************************************************************************************************************************************************************************/

#pragma GCC optimize ("-O3")                                                                                                        // Compiler optimisation level. For this file only. Edit platform.txt for all files.
#define DEBUG_MODE 0                                                                                                                // Toggle serial debug messages. Disable in production.
#define FTM_INDICATOR 1                                                                                                             // Indicate FTM status when using M3 RPA hazards switch.
#define FRONT_FOG_INDICATOR 1                                                                                                       // Turn on an LED when front fogs are on. M3 clusters lack this.
#define F_ZBE_WAKE 0                                                                                                                // Enable/disable F CIC ZBE wakeup functions
#define DTC_WITH_M_BUTTON 1                                                                                                         // Toggle DTC mode with M MFL button
#define AUTO_SEAT_HEATING 1                                                                                                         // Enable automatic heated seat for driver in low temperatures
const uint8_t AUTO_SEAT_HEATING_TRESHOLD = 10 * 2 + 80;                                                                             // Degrees Celsius temperature * 2 + 80
const uint8_t DTC_SWITCH_TIME = 7;                                                                                                  // Set duration for Enabling/Disabling DTC mode on with long press of M key. 100ms increments.
const uint32_t START_UPSHIFT_WARN_RPM = 6000*4;                                                                                     // RPM setpoints (warning = desired RPM * 4).
const uint32_t MID_UPSHIFT_WARN_RPM = 6500*4;
const uint32_t MAX_UPSHIFT_WARN_RPM = 6700*4;

/***********************************************************************************************************************************************************************************************************************************************
***********************************************************************************************************************************************************************************************************************************************/

unsigned long int rxId;
unsigned char rxBuf[8], len;

bool ignition = false;
uint8_t ignition_last_state = 0xEA;

#if FRONT_FOG_INDICATOR
  bool front_fog_state = false;
  uint8_t last_light_state = 0;
#endif

#if FTM_INDICATOR
  bool ftm_indicator_state = false;
  byte ftm_indicator_flash[] = {0x40, 0x50, 0x01, 0x69, 0xFF, 0xFF, 0xFF, 0xFF};
  byte ftm_indicator_off[] = {0x40, 0x50, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};
#endif

#if F_ZBE_WAKE
  byte f_wakeup[] = {0, 0, 0, 0, 0x57, 0x2F, 0, 0x60};                                                                              // Network management kombi, F-series
  byte zbe_response[] = {0xE1, 0x9D, 0, 0xFF};
  long zbe_wakeup_last_sent;
#endif

byte mbutton_idle[] = {0xFF, 0x3F, 0}, mbutton_pressed[] = {0xBF, 0x7F, 0};
uint8_t mbutton_checksum = 0xF0, mbutton_hold_counter = 0;
unsigned long mbutton_idle_timer;

byte dtc_key_pressed[] = {0xFD, 0xFF}, dtc_key_released[] = {0xFC, 0xFF};
byte dsc_off_fake_status[] = {0x40, 0x24, 0, 0x1D, 0xFF, 0xFF, 0xFF, 0xFF};

byte shiftlights_start[] = {0x86, 0x3E};
byte shiftlights_mid_buildup[] = {0xF6, 0};
byte shiftlights_max_flash[] = {0x0A, 0};
byte shiftlights_off[] = {0x05, 0};
bool shiftlights_segments_active = false;

bool mdrive_status = false;                                                                                                         // false = off, true = on
uint8_t mdrive_last_state = 0xCF;                                                                                                   // OFF by default
uint8_t dsc_status = 0;                                                                                                             // 0 = on, 1 = DTC, 2 = DSC OFF
uint8_t dsc_last_state = 0;
bool holding_dsc = false;
unsigned long power_switch_debounce_timer, dsc_switch_debounce_timer, dsc_switch_hold_timer;
const uint16_t power_debounce_time_ms = 300, dsc_debounce_time_ms = 200, dsc_hold_time_ms = 400;

#if AUTO_SEAT_HEATING
  uint8_t ambient_temperature_can = 255;
  bool sent_seat_heating_request = false;
  byte seat_heating_button_press[] = {0xFD, 0xFF}, seat_heating_button_release[] = {0xFC, 0xFF};
#endif

#if DEBUG_MODE
  char serial_debug_string[128];
#endif


void setup() 
{
  pinMode(PTCAN_INT_PIN, INPUT);                                                                                                    // Configure pins
  pinMode(KCAN_INT_PIN, INPUT);
  pinMode(POWER_SWITCH_PIN, INPUT_PULLUP);                                                                                                 
  pinMode(DSC_SWITCH_PIN, INPUT_PULLUP);
  pinMode(POWER_LED_PIN, OUTPUT);
  #if FRONT_FOG_INDICATOR
    pinMode(FOG_LED_PIN, OUTPUT);
  #endif
  disable_unused_peripherals();
  SPI.setClockDivider(SPI_CLOCK_DIV2);         																						                                          // Set SPI to run at 8MHz (16MHz / 2 = 8 MHz) from default 4 

  #if DEBUG_MODE
    Serial.begin(115200);
    while(!Serial);                                                                                                                 // 32U4, wait until virtual port initialized
  #endif
  
  while (CAN_OK != PTCAN.begin(MCP_STDEXT, CAN_500KBPS, MCP2515_PTCAN) || 
         CAN_OK != KCAN.begin(MCP_STDEXT, CAN_100KBPS, MCP2515_KCAN)) {
    #if DEBUG_MODE
      Serial.println(F("Error initializing MCP2515s. Re-trying."));
    #endif
    delay(5000);
  }

  #if DEBUG_MODE
    Serial.println(F("MCP2515s initialized successfully."));
  #endif 

  PTCAN.init_Mask(0, 0x07FFFFFF);                                                                                                   // Mask matches: 07FFFFFF (standard ID) and first two bytes
  PTCAN.init_Filt(0, 0x05A940B8);                                                                                                   // Filter DSC statuses
  PTCAN.init_Filt(1, 0x05A94024);
  PTCAN.init_Mask(1, 0x07FF0000);                                                                                                   // Mask matches: 07FF (standard ID) and all bytes
  PTCAN.init_Filt(2, 0x019E0000);                                                                                                   // Get ignition state from DSC status msg
  PTCAN.init_Filt(3, 0x01D60000);                                                                                                   // Filter MFL button status.
  #if FRONT_FOG_INDICATOR
    PTCAN.init_Filt(4, 0x021A0000);                                                                                                 // Filter light status
  #endif
  #if FTM_INDICATOR
     PTCAN.init_Filt(5, 0x031D0000);                                                                                                // Filter FTM status broadcast by DSC
  #endif
  PTCAN.setMode(MCP_NORMAL);
  
  KCAN.init_Mask(0, 0x07FF0000);                                                                                                    // Mask matches: 07FF (standard ID) and all bytes
  KCAN.init_Mask(1, 0x07FF0000);                                                                                                    // Mask matches: 07FF (standard ID) and all bytes 
  KCAN.init_Filt(0, 0x00AA0000);                                                                                                    // Filter RPM, throttle pos.
  KCAN.init_Filt(1, 0x03990000);                                                                                                    // Filter MDrive status.
  #if AUTO_SEAT_HEATING
    KCAN.init_Filt(2, 0x02320000);                                                                                                  // Driver's seat heating status
    KCAN.init_Filt(3, 0x02CA0000);                                                                                                  // Ambient temperature
  #endif
  #if F_ZBE_WAKE
    KCAN.init_Filt(4, 0x02730000);                                                                                                  // Filter CIC status.
    KCAN.init_Filt(5, 0x04E20000);                                                                                                  // Filter CIC Network management (sent when CIC is on)
  #endif
  KCAN.setMode(MCP_NORMAL);
  
  #if F_ZBE_WAKE
    power_switch_debounce_timer = dsc_switch_debounce_timer = mbutton_idle_timer = zbe_wakeup_last_sent = millis();
  #else
     power_switch_debounce_timer = dsc_switch_debounce_timer = mbutton_idle_timer = millis();
  #endif
}

void loop()
{

/***********************************************************************************************************************************************************************************************************************************************
  Centre console button section.
***********************************************************************************************************************************************************************************************************************************************/

  if (ignition) {
    if (!digitalRead(POWER_SWITCH_PIN)) {
      if ((millis() - power_switch_debounce_timer) > power_debounce_time_ms) {
        #if DEBUG_MODE
          Serial.println("Console: POWER button pressed. Requesting MDrive.");
        #endif 
        
        send_mbutton_message(mbutton_pressed);                                                                                      // Emulate key press
        delay(100);
        send_mbutton_message(mbutton_idle);
        power_switch_debounce_timer = millis();
      }
    }
    else if (!digitalRead(DSC_SWITCH_PIN)) {
      if (dsc_status == 0) {
        if (!holding_dsc) {
          holding_dsc = true;
          dsc_switch_hold_timer = millis();
        } else {
          if ((millis() - dsc_switch_hold_timer) > dsc_hold_time_ms) {                                                              // DSC OFF sequence should only be sent after user holds key for a configured time
            #if DEBUG_MODE
              Serial.println("Console: DSC OFF button held. Sending DSC OFF.");
            #endif
            send_dsc_off_sequence();
            dsc_switch_debounce_timer = millis();
          }
        }      
      } else {
        if ((millis() - dsc_switch_debounce_timer) > dsc_debounce_time_ms) {                                                        // A quick tap re-enables everything
          #if DEBUG_MODE
            Serial.println("Console: DSC button tapped. Re-enabling DSC.");
          #endif
          dsc_switch_debounce_timer = millis();
          send_dtc_button_press();
        }
      }
    } else {
      holding_dsc = false;
    }
  } 

/***********************************************************************************************************************************************************************************************************************************************
  PT-CAN section.
***********************************************************************************************************************************************************************************************************************************************/
  
  if (!digitalRead(PTCAN_INT_PIN)) {                                                                                                // If INT pin is pulled low, read PT-CAN receive buffer
    PTCAN.readMsgBuf(&rxId, &len, rxBuf);                                                                                           // Read data: rxId = CAN ID, buf = data byte(s)
    
    if (rxId == 0x19E) {
      if (rxBuf[1] != ignition_last_state) {
        if (rxBuf[1] == 0xEA) {
          ignition = false;
          reset_runtime_variables();
          #if DEBUG_MODE
            Serial.println("PT-CAN: Ignition OFF. Reset values.");
          #endif
        } else {
          ignition = true;
          #if DEBUG_MODE
            Serial.println("PT-CAN: Ignition ON.");
          #endif
        }
        ignition_last_state = rxBuf[1];          
      }                                                                                       
    } 

    else if (ignition) {
      if (rxId == 0x1D6) {       
        if (ignition) {
          if (rxBuf[1] == 0x4C) {                                                                                                   // M button is pressed
            send_mbutton_message(mbutton_pressed);
            #if DTC_WITH_M_BUTTON
              mbutton_hold_counter++;
              if (mbutton_hold_counter == DTC_SWITCH_TIME) {
                if (dsc_status == 0 && mdrive_status) {                                                                             // Check to make sure DSC/DTC are off and Mdrive is on
                  send_dtc_button_press();
                }
                mbutton_hold_counter = 0;
              }
            #endif
          } else if (rxBuf[0] == 0xC0 && rxBuf[1] == 0x0C) {                                                                        // MFL buttons released, send alive ping.
            send_mbutton_message(mbutton_idle);                                                                                       
            #if DTC_WITH_M_BUTTON
              mbutton_hold_counter = 0;
            #endif
          } else {
            if ((millis() - mbutton_idle_timer) > 999) {                                                                            // keep sending mdrive idle messages when other buttons are pressed/held
              send_mbutton_message(mbutton_idle);
            }
            #if DTC_WITH_M_BUTTON
              mbutton_hold_counter = 0;
            #endif
          }
        }
    }
     
    #if FRONT_FOG_INDICATOR
      else if (rxId == 0x21A) {
        if (rxBuf[0] != last_light_state) {
          if ((rxBuf[0] & 32) != 0) {                                                                                               // Check the third bit of the first byte represented in binary
            front_fog_state = true;
            digitalWrite(FOG_LED_PIN, HIGH);
            #if DEBUG_MODE
              Serial.println("PT-CAN: Front fogs on. Turned on FOG LED");
            #endif
          } else {
            front_fog_state = false;
            digitalWrite(FOG_LED_PIN, LOW);
            #if DEBUG_MODE
              Serial.println("PT-CAN: Front fogs off. Turned off FOG LED");
            #endif
          }
          last_light_state = rxBuf[0];
        }
      }
    #endif

      #if FTM_INDICATOR
        else if (rxId == 0x31D) {
          if (rxBuf[0] == 0x03 && !ftm_indicator_state) {
            KCAN.sendMsgBuf(0x5A0, 8, ftm_indicator_flash);
            ftm_indicator_state = true;
            #if DEBUG_MODE
              Serial.println("K-CAN: Activated FTM indicator.");
            #endif
          } else if (rxBuf[0] == 0x00 && ftm_indicator_state) {
            KCAN.sendMsgBuf(0x5A0, 8, ftm_indicator_off);
            ftm_indicator_state = false;
            #if DEBUG_MODE
              Serial.println("K-CAN: Deactivated FTM indicator.");
            #endif
          }
        }
      #endif

      #if F_ZBE_WAKE
        else if (rxId == 0x4E2){
            send_zbe_wakeup();
        }
      #endif
  
      else {                                                                                                                       // Monitor 0x5A9 DSC status on PT-CAN       
        // When DTC mode is toggled on or off, the disable/enable byte (1D/1C) is preceded with 0xB8 in rxBuf[1]
        // When DSC OFF mode is toggled on or off, the disable/enable byte (1D/1C) is preceded with 0x24 in rxBuf[1]
        // Must keep track of this state because DTC is actually toggled off before DSC OFF engages in a normal 2.5s sequence.
        if (rxBuf[1] == 0xB8) {
          if (rxBuf[3] == 0x1D && dsc_last_state == 0) {
            dsc_status = 1;
            dsc_last_state = rxBuf[1];
            #if DEBUG_MODE
              Serial.println("PT-CAN: Status DTC on");
            #endif
          } else if (rxBuf[3] == 0x1C && dsc_last_state == 0xB8) {
            dsc_status = dsc_last_state = 0;
            #if DEBUG_MODE
              Serial.println("PT-CAN: Status all back on (from DTC mode)");
            #endif
          }
        } else {                                                                                                                   // rxBuf[1] == 0x24
          if (rxBuf[3] == 0x1D) {
            dsc_status = 2;
            dsc_last_state = rxBuf[1];
            #if DEBUG_MODE
              Serial.println("PT-CAN: Status DSC off");
            #endif
          } else if (rxBuf[3] == 0x1C && dsc_last_state == rxBuf[1]) {
            dsc_status = dsc_last_state = 0;
            #if DEBUG_MODE
              Serial.println("PT-CAN: Status all back on (from DSC OFF mode)");
            #endif
          }
        }
      } 
    }
  }

/***********************************************************************************************************************************************************************************************************************************************
  K-CAN section.
***********************************************************************************************************************************************************************************************************************************************/
  
  if(!digitalRead(KCAN_INT_PIN)) {                                                                                                  // If INT pin is pulled low, read K-CAN receive buffer
    KCAN.readMsgBuf(&rxId, &len, rxBuf);

    if (rxId == 0xAA) {                                                                                                             // Monitor 0xAA (throttle status) and calculate shiftlight status
      if (ignition) {
        evaluate_shiftlight_display();
      }
    }
    
    else if (rxId == 0x399) {                                                                                                       // Monitor MDrive status on K-CAN and control centre console POWER LED
      if (mdrive_last_state != rxBuf[4]) {
        if (rxBuf[4] == 0xDF) {
          mdrive_status = true;
          digitalWrite(POWER_LED_PIN, HIGH);
          #if DEBUG_MODE
            Serial.println("K-CAN: Status MDrive on. Turned on POWER LED");
          #endif
        } else {
          mdrive_status = false;
          digitalWrite(POWER_LED_PIN, LOW);
          #if DEBUG_MODE
            Serial.println("K-CAN: Status MDrive off. Turned off POWER LED");
          #endif
          if (dsc_status == 1) {                                                                                                    // Turn off DTC together with MDrive
            send_dtc_button_press();
            #if DEBUG_MODE
              Serial.println("K-CAN: Turned off DTC with MDrive off.");
            #endif
          }
        }
        mdrive_last_state = rxBuf[4];
      }
    }

    #if AUTO_SEAT_HEATING
      else if (rxId == 0x2CA){                                                                                                      // Monitor and update ambient temperature
        ambient_temperature_can = rxBuf[0];
      } 
      else if (rxId == 0x232) {                                                                                                     // Driver's seat heating status message is only sent with ignition on.
        if (!rxBuf[0]) {                                                                                                            // Check if seat heating is already on.
          //This will be ignored if already on and cycling ignition. Press message will be ignored by IHK anyway.
          if (!sent_seat_heating_request && (ambient_temperature_can <= AUTO_SEAT_HEATING_TRESHOLD)) {
            send_seat_heating_request();
          }
        }
      }
    #endif
    
    #if F_ZBE_WAKE
      else if (rxId == 0x273) {                                                                                                     // Monitor CIC challenge request and respond
        zbe_response[2] = rxBuf[7];
        KCAN.sendMsgBuf(0x277, 4, zbe_response);                                                                                    // Acknowledge must be sent three times
        KCAN.sendMsgBuf(0x277, 4, zbe_response);
        KCAN.sendMsgBuf(0x277, 4, zbe_response);
        #if DEBUG_MODE
          sprintf(serial_debug_string, "K-CAN: Sent ZBE response to CIC with counter: 0x%X\n", rxBuf[7]);
          Serial.print(serial_debug_string);
        #endif
      }
    #endif
  }
}

/***********************************************************************************************************************************************************************************************************************************************
  Helper functions
***********************************************************************************************************************************************************************************************************************************************/

void reset_runtime_variables() 
{
  dsc_status = dsc_last_state = mbutton_hold_counter = 0;
  mdrive_status = false;
  mdrive_last_state = 0xCF;  
  ignition_last_state = 0xEA;
  digitalWrite(POWER_LED_PIN, LOW);
  #if FRONT_FOG_INDICATOR
    front_fog_state = false;
    last_light_state = 0;
    digitalWrite(FOG_LED_PIN, LOW);
  #endif
  #if FTM_INDICATOR
    ftm_indicator_state = false;
  #endif
}


#if F_ZBE_WAKE
void send_zbe_wakeup()
{
  KCAN.sendMsgBuf(0x560, 8, f_wakeup);
  zbe_wakeup_last_sent = millis();
  #if DEBUG_MODE
    Serial.println("K-CAN: Sent F-ZBE wake-up message");
  #endif
}
#endif


void send_mbutton_message(byte message[]) 
{
  message[2] = mbutton_checksum;

  byte send_stat = PTCAN.sendMsgBuf(0x1D9, 3, message);
  #if DEBUG_MODE
    if (send_stat != CAN_OK) {
      Serial.print("PT-CAN: Error sending mbutton message. Re-trying. Error: ");
      Serial.println(send_stat);
    } else {
      message[0] == 0xFF ? Serial.println("PT-CAN: Sent mbutton idle.") : Serial.println("PT-CAN: Sent mbutton press.");
    }
  #else
  if (send_stat != CAN_OK) {
    delay(100);                                                                                                                     // Attempt to send again
    PTCAN.sendMsgBuf(0x1D9, 3, message);
  }
  #endif
  mbutton_idle_timer = millis();
  mbutton_checksum < 0xFF ? mbutton_checksum++ : mbutton_checksum = 0xF0;                                                           // mbutton_checksum is between F0..FF
}


void evaluate_shiftlight_display()
{
  uint32_t RPM = ((uint32_t)rxBuf[5] << 8) | (uint32_t)rxBuf[4];
  
  if (RPM > START_UPSHIFT_WARN_RPM && RPM < MID_UPSHIFT_WARN_RPM) {                                                               // First segment                                                              
    if (rxBuf[2] != 3) {                                                                                                          // Send the warning only if the throttle pedal is pressed
      activate_shiftlight_segments(shiftlights_start);
      #if DEBUG_MODE
        sprintf(serial_debug_string, "Displaying first warning at RPM: %ld\n", RPM / 4);
        Serial.print(serial_debug_string);
      #endif                     
    } else {
      deactivate_shiftlight_segments();
    }
  } else if (RPM > MID_UPSHIFT_WARN_RPM && RPM < MAX_UPSHIFT_WARN_RPM) {                                                          // Buildup from second segment to reds
    if (rxBuf[2] != 3) {
      activate_shiftlight_segments(shiftlights_mid_buildup);
      #if DEBUG_MODE
        sprintf(serial_debug_string, "Displaying increasing warning at RPM: %ld\n", RPM / 4);
        Serial.print(serial_debug_string);
      #endif
    } else {
      deactivate_shiftlight_segments();
    }
  } else if (RPM >= MAX_UPSHIFT_WARN_RPM) {                                                                                       // Flash all segments
    if (rxBuf[2] != 3) {
      activate_shiftlight_segments(shiftlights_max_flash);
      #if DEBUG_MODE
        sprintf(serial_debug_string, "Flash max warning at RPM: %ld\n", RPM / 4);
        Serial.print(serial_debug_string);
      #endif
    } else {
      deactivate_shiftlight_segments();
    }
  } else {                                                                                                                        // RPM dropped. Disable lights
    deactivate_shiftlight_segments();
  }
}


#if AUTO_SEAT_HEATING
void send_seat_heating_request()
{
  delay(10);
  KCAN.sendMsgBuf(0x1E7, 2, seat_heating_button_press);
  delay(20);
  KCAN.sendMsgBuf(0x1E7, 2, seat_heating_button_release);
  delay(20);
  #if DEBUG_MODE
    sprintf(serial_debug_string, "K-CAN: Sent dr seat heating request at ambient %dC, treshold %dC\n", (ambient_temperature_can - 80) / 2, (AUTO_SEAT_HEATING_TRESHOLD - 80) / 2);
    Serial.print(serial_debug_string);
  #endif
  sent_seat_heating_request = true;
}
#endif


void activate_shiftlight_segments(byte* data)
{
    PTCAN.sendMsgBuf(0x206, 2, data);                                                                     
    shiftlights_segments_active = true;
}


void deactivate_shiftlight_segments()
{
  if (shiftlights_segments_active) {
    PTCAN.sendMsgBuf(0x206, 2, shiftlights_off);                                                                                
    shiftlights_segments_active = false;
    #if DEBUG_MODE
      Serial.println("PTCAN: Deactivated shiftlights segments");
    #endif 
  }
}


void send_dtc_button_press() 
// Correct timing sequence as per trace is: 
// key press -> delay(100) -> key press -> delay(50) -> key release -> delay(160) -> key release -> delay(160)
// However, that interferes with program timing. A small delay will still be accepted.
{
  PTCAN.sendMsgBuf(0x316, 2, dtc_key_pressed);                                                                                      // Two messages are sent during a quick press of the button (DTC mode).
  delay(5);
  PTCAN.sendMsgBuf(0x316, 2, dtc_key_pressed);
  delay(5);
  PTCAN.sendMsgBuf(0x316, 2, dtc_key_released);                                                                                     // Send one DTC released to indicate end of DTC key press.
  #if DEBUG_MODE                        
    Serial.println("PT-CAN: Sent single DTC key press.");
  #endif
} 


void send_dsc_off_sequence() 
{
  PTCAN.sendMsgBuf(0x5A9, 8, dsc_off_fake_status);                                                                                  // Trigger DSC OFF CC in Kombi, iDrive as soon as sequence starts
  for (int i = 0; i < 26; i++) {                                                                                                    // >2.5s to send full DSC OFF sequence.
    if ((millis() - mbutton_idle_timer) > 999) {                                                                                    // keep sending mdrive idle message
      send_mbutton_message(mbutton_idle);
    }
    PTCAN.sendMsgBuf(0x316, 2, dtc_key_pressed);
    delay(100); 
  }
  PTCAN.sendMsgBuf(0x316, 2, dtc_key_released);
  #if DEBUG_MODE
    Serial.println("PT-CAN: Sent DSC OFF sequence.");
  #endif
}


void disable_unused_peripherals()
// 32U4 Specific!
{
  power_usart0_disable();                                                                                                           // Disable UART
  power_usart1_disable();                                                                       
  power_twi_disable();                                                                                                              // Disable I2C
  power_timer1_disable();                                                                                                           // Disable unused timers. 0 still on.
  power_timer2_disable();
  power_timer3_disable();
  ADCSRA = 0;
  power_adc_disable();                                                                                                              // Disable Analog to Digital converter
  #if !DEBUG_MODE                                                                                                                   // In production operation the USB interface is not needed
    if (digitalRead(POWER_SWITCH_PIN)) {                                                                                            // Bypass USB disable by holding POWER when powering module. This pin should be LOW when holding
        power_usb_disable();
        USBCON |= (1 << FRZCLK);
        PLLCSR &= ~(1 << PLLE);
        USBCON &=  ~(1 << USBE); 
    }
  #endif
}
/***********************************************************************************************************************************************************************************************************************************************
  EOF
***********************************************************************************************************************************************************************************************************************************************/
