// Program settings and global variables go here.


#include <FlexCAN_T4.h>
#include <EEPROM.h>
#include "src/wdt4/Watchdog_t4.h"
#include "src/queue/cppQueue.h"
#include "usb_dev.h" 
extern "C" uint32_t set_arm_clock(uint32_t frequency);
FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> KCAN;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> PTCAN;
FlexCAN_T4<CAN3, RX_SIZE_256, TX_SIZE_16> DCAN;
WDT_T4<WDT1> wdt;
const uint8_t wdt_timeout_sec = 10;

/***********************************************************************************************************************************************************************************************************************************************
  Board configuration section.
***********************************************************************************************************************************************************************************************************************************************/

#define PTCAN_STBY_PIN 15                                                                                                           // These are optional but recommended to save power when the car prepares to deep sleep.
#define DCAN_STBY_PIN 14

#define EXHAUST_FLAP_SOLENOID_PIN 17
#define DSC_BUTTON_PIN 16
#define POWER_BUTTON_PIN 2
#define POWER_LED_PIN 3
#define FOG_LED_PIN 4

/***********************************************************************************************************************************************************************************************************************************************
  Program configuration section.
***********************************************************************************************************************************************************************************************************************************************/

#define DEBUG_MODE 1                                                                                                                // Toggle serial debug messages. Disable in production.
#if !DEBUG_MODE
  #define USB_DISABLE 1                                                                                                             // USB can be disabled if not using serial for security reasons. Use caution when enabling this.
    #if USB_DISABLE                                                                                                                 // USB can be activated by holding POWER while turning on ignition.
      // To make this work, the following changes need to be made to startup.c 
      // (Linux path: /home/**Username**/.arduino15/packages/teensy/hardware/avr/**Version**/cores/teensy4/startup.c)
      // Comment these lines:
      // usb_pll_start();
      // while (millis() < TEENSY_INIT_USB_DELAY_BEFORE) ;
      // usb_init();
      // while (millis() < TEENSY_INIT_USB_DELAY_AFTER + TEENSY_INIT_USB_DELAY_BEFORE) ; // wait
    #endif
#endif

#define CKM 1                                                                                                                       // Persistently remember POWER when set in iDrive.
#define EDC_CKM_FIX 0                                                                                                               // Sometimes the M Key setting for EDC is not recalled correctly - especially with CA.
#define DOOR_VOLUME 1                                                                                                               // Reduce audio volume on door open. Also disables the door open with ignition warning CC.
#define RHD 1                                                                                                                       // Where does the driver sit?
#define FTM_INDICATOR 1                                                                                                             // Indicate FTM (Flat Tyre Monitor) status when using M3 RPA hazards button cluster.
#define HOOD_OPEN_GONG 1                                                                                                            // Plays CC gong warning when opening hood.
#define FRM_HEADLIGHT_MODE 1                                                                                                        // Switches FRM AHL mode from Komfort and Sport.
#define HDC 1                                                                                                                       // Gives a function to the HDC console button in non 4WD cars.
#define FAKE_MSA 1                                                                                                                  // Display Auto Start-Stop OFF CC message when the Auto Start-Stop button is pressed. Must be coded in IHK.
#if !FAKE_MSA
#define MSA_RVC 0                                                                                                                   // Turn on the OEM rear view camera (TRSVC, E84 PDC) when pressing MSA button. E70 button from 61319202037.
#endif                                                                                                                              // RVC can be controlled independently of PDC with this button.
#define WIPE_AFTER_WASH 1                                                                                                           // One more wipe cycle after washing the windscreen.
#define AUTO_MIRROR_FOLD 1                                                                                                          // Fold/Un-fold mirrors when locking. Can be done with coding but this integrates nicer.
#if AUTO_MIRROR_FOLD
#define UNFOLD_WITH_DOOR 1                                                                                                          // Un-fold with door open event instead of unlock button.
#endif
#define ANTI_THEFT_SEQ 1                                                                                                            // Disable fuel pump until the steering wheel M button is pressed a number of times.
#if ANTI_THEFT_SEQ                                                                                                                  // If this is not deactivated before start, DME will store errors!
const uint8_t ANTI_THEFT_SEQ_NUMBER = 3;                                                                                            // Number of times to press the button for the EKP to be re-activated.
#define ANTI_THEFT_SEQ_ALARM 1                                                                                                      // Sound the alarm if engine is started without disabling anti-theft.
#endif
#if ANTI_THEFT_SEQ_ALARM
const uint8_t ANTI_THEFT_SEQ_ALARM_NUMBER = 5;                                                                                      // Number of times to press the button for the alarm to be silenced and EKP re-activated.
#endif
#define REVERSE_BEEP 1                                                                                                              // Play a beep throught he speaker closest to the driver when engaging reverse.
#define FRONT_FOG_LED_INDICATOR 1                                                                                                   // Turn ON an external LED when front fogs are ON. M3 clusters lack this indicator.
#define FRONT_FOG_CORNER 1                                                                                                          // Turn ON/OFF corresponding fog light when turning.
#if FRONT_FOG_CORNER
const int8_t FOG_CORNER_STEERTING_ANGLE = 80;                                                                                       // Steering angle at which to activate fog corner function.
const int8_t STEERTING_ANGLE_HYSTERESIS = 15;
const int8_t FOG_CORNER_STEERTING_ANGLE_INDICATORS = 45;
const int8_t STEERTING_ANGLE_HYSTERESIS_INDICATORS = 10;
#endif
#define DIM_DRL 1                                                                                                                   // Dims DLR ON the side that the indicator is ON.
#define SERVOTRONIC_SVT70 1                                                                                                         // Control steering assist with modified SVT70 module.
#define EXHAUST_FLAP_CONTROL 1                                                                                                      // Take control of the exhaust flap solenoid.
#if EXHAUST_FLAP_CONTROL
#define QUIET_START 1                                                                                                               // Close the exhaust valve as soon as Terminal R is turned ON.
#endif
#define LAUNCH_CONTROL_INDICATOR 1                                                                                                  // Show launch control indicator (use with MHD lauch control, 6MT).
#define CONTROL_SHIFTLIGHTS 1                                                                                                       // Display shiftlights, animation and sync with the variable redline of M3 KOMBI.
#define NEEDLE_SWEEP 1                                                                                                              // Needle sweep animation with engine ON. Calibrated for M3 speedo with 335i tacho.
#define AUTO_SEAT_HEATING 1                                                                                                         // Enable automatic heated seat for driver in low temperatures.
#if AUTO_SEAT_HEATING
#define AUTO_SEAT_HEATING_PASS 1                                                                                                    // Enable automatic heated seat for passenger in low temperatures.
#endif
#define RTC 1                                                                                                                       // Set the time/date if power is lost. Requires external battery.
#define F_ZBE_WAKE 0                                                                                                                // Enable/disable FXX CIC ZBE wakeup functions. Do not use with an EXX ZBE.


#if SERVOTRONIC_SVT70
  const uint16_t SVT_FAKE_EDC_MODE_CANID = 0x327;                                                                                   // New CAN-ID replacing 0x326 in SVT70 firmware bin. This stops it from changing modes together with EDC.
#endif
const uint16_t DME_FAKE_VEH_MODE_CANID = 0x31F;                                                                                     // New CAN-ID replacing 0x315 in DME [Program] section of the firmware.
const double AUTO_SEAT_HEATING_TRESHOLD = 10.0;                                                                                     // Degrees Celsius temperature.
#if CONTROL_SHIFTLIGHTS
  const uint16_t START_UPSHIFT_WARN_RPM = 5500 * 4;                                                                                 // RPM setpoints (warning = desired RPM * 4).
  const uint16_t MID_UPSHIFT_WARN_RPM = 6000 * 4;
  const uint16_t MAX_UPSHIFT_WARN_RPM = 6500 * 4;
  const uint16_t GONG_UPSHIFT_WARN_RPM = 7000 * 4;
  const int16_t VAR_REDLINE_OFFSET_RPM = -300;                                                                                      // RPM difference between DME requested redline and KOMBI displayed redline. Varies with cluster.
#endif
#if EXHAUST_FLAP_CONTROL
  const uint16_t EXHAUST_FLAP_QUIET_RPM = 3000 * 4;                                                                                 // RPM setpoint to open the exhaust flap in normal mode (desired RPM * 4).
#endif
#if LAUNCH_CONTROL_INDICATOR
  const uint16_t LC_RPM = 4000 * 4;                                                                                                 // RPM setpoint to display launch control flag CC (desired RPM * 4). Match with MHD setting.
  const uint16_t LC_RPM_MIN = LC_RPM - (250 * 4);                                                                                   // RPM hysteresis when bouncing off the limiter.
  const uint16_t LC_RPM_MAX = LC_RPM + (250 * 4);
#endif
const float MAX_THRESHOLD = 72.0;                                                                                                   // CPU temperature thresholds for the processor clock scaling function.
const float HIGH_THRESHOLD = 68.0;
const float MEDIUM_THRESHOLD = 64.0;
const float MILD_THRESHOLD = 60.0;
const float HYSTERESIS = 2.0;
const unsigned long MAX_UNDERCLOCK = 24 * 1000000;                                                                                  // temperature <= 90 (Tj) should be ok for more than 100,000 Power on Hours at 1.15V (freq <= 528 MHz).
unsigned long HIGH_UNDERCLOCK = 150 * 1000000;
unsigned long MEDIUM_UNDERCLOCK = 396 * 1000000;
unsigned long MILD_UNDERCLOCK = 450 * 1000000;

/***********************************************************************************************************************************************************************************************************************************************
***********************************************************************************************************************************************************************************************************************************************/

#if !USB_DISABLE                                                                                                                    // Not needed if startup.c is modified.
  #if AUTO_MIRROR_FOLD
    extern "C" void startup_middle_hook(void);
    extern "C" volatile uint32_t systick_millis_count;
    void startup_middle_hook(void) {
      // Force millis() to be 300 to skip USB startup delays. The module needs to boot very quickly to revceive the unlock message.
      // This makes receiving early boot serial messages difficult.
      systick_millis_count = 300;
    }
  #endif
#endif

CAN_message_t pt_msg, k_msg, d_msg;
typedef struct delayed_can_tx_msg {
	CAN_message_t	tx_msg;
	unsigned long	transmit_time;
} delayed_can_tx_msg;

uint32_t cpu_speed_ide;
bool terminal_r = false, ignition = false,  vehicle_awake = false;
bool engine_cranking = false, engine_running = false;
elapsedMillis vehicle_awake_timer = 0, vehicle_awakened_time = 0;
CAN_message_t dsc_on_buf, dsc_mdm_dtc_buf, dsc_off_buf;
cppQueue dsc_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
uint16_t RPM = 0;
#if CKM
  uint8_t dme_ckm[4][2] = {{0, 0xFF}, {0, 0xFF}, {0, 0xFF}, {0xF1, 0xFF}};
#endif
#if EDC_CKM_FIX
  uint8_t edc_ckm[] = {0, 0, 0, 0xF1};
  uint8_t edc_mismatch_check_counter = 0;
  CAN_message_t edc_button_press_buf;
  cppQueue edc_ckm_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if CKM || EDC_CKM_FIX
  uint8_t cas_key_number = 0;                                                                                                       // 0 = Key 1, 1 = Key 2...
#endif
uint8_t mdrive_dsc, mdrive_power, mdrive_edc, mdrive_svt;
bool mdrive_status = false, mdrive_settings_save_to_eeprom = false, mdrive_power_active = false;
bool console_power_mode, restore_console_power_mode = false;
uint8_t power_mode_only_dme_veh_mode[] = {0xE8, 0xF1};                                                                              // E8 is the last checksum. Start will be from 0A.
uint8_t dsc_program_status = 0;                                                                                                     // 0 = ON, 1 = DTC, 2 = DSC OFF
bool holding_dsc_off_console = false;
elapsedMillis mdrive_message_timer = 0;
uint8_t mfl_pressed_count = 0;
CAN_message_t idrive_mdrive_settings_a_buf, idrive_mdrive_settings_b_buf;
elapsedMillis power_button_debounce_timer = 0, dsc_off_button_debounce_timer = 0, dsc_off_button_hold_timer = 0;
const uint16_t power_debounce_time_ms = 300, dsc_debounce_time_ms = 500, dsc_hold_time_ms = 300;
bool ignore_m_press = false, ignore_m_hold = false;
uint8_t clock_mode = 0;
float last_cpu_temp = 0;
CAN_message_t cc_gong_buf;

#if SERVOTRONIC_SVT70
  uint8_t servotronic_message[] = {0, 0xFF};
  CAN_message_t servotronic_cc_on_buf;
  bool diagnose_svt = false;
  #if DEBUG_MODE
    uint16_t dcan_forwarded_count = 0, ptcan_forwarded_count = 0;
  #endif
#endif
#if CONTROL_SHIFTLIGHTS
  CAN_message_t shiftlights_start_buf, shiftlights_mid_buildup_buf, shiftlights_startup_buildup_buf;
  CAN_message_t shiftlights_max_flash_buf, shiftlights_off_buf;
  bool shiftlights_segments_active = false;
  uint8_t ignore_shiftlights_off_counter = 0;
  uint16_t START_UPSHIFT_WARN_RPM_ = START_UPSHIFT_WARN_RPM;
  uint16_t MID_UPSHIFT_WARN_RPM_ = MID_UPSHIFT_WARN_RPM;
  uint16_t MAX_UPSHIFT_WARN_RPM_ = MAX_UPSHIFT_WARN_RPM;
  uint16_t GONG_UPSHIFT_WARN_RPM_ = GONG_UPSHIFT_WARN_RPM;
  bool engine_coolant_warmed_up = false;
  uint16_t var_redline_position;
  uint8_t last_var_rpm_can = 0;
  uint8_t mdrive_message[] = {0, 0, 0, 0, 0, 0x97};                                                                                   // byte 5: shiftlights always on
#else
  uint8_t mdrive_message[] = {0, 0, 0, 0, 0, 0x87};                                                                                   // byte 5: shiftlights always on
#endif
#if NEEDLE_SWEEP
  CAN_message_t speedo_needle_max_buf, speedo_needle_min_buf, speedo_needle_release_buf;
  CAN_message_t tacho_needle_max_buf, tacho_needle_min_buf, tacho_needle_release_buf;
  CAN_message_t fuel_needle_max_buf, fuel_needle_min_buf, fuel_needle_release_buf;
  CAN_message_t oil_needle_max_buf, oil_needle_min_buf, oil_needle_release_buf;
  CAN_message_t any_needle_max_b_buf;
  cppQueue kombi_needle_txq(sizeof(delayed_can_tx_msg), 32, queue_FIFO);
#endif
#if FRONT_FOG_LED_INDICATOR || FRONT_FOG_CORNER
  bool front_fog_status = false;
#endif
#if FRONT_FOG_CORNER
  bool indicators_on = false;
  bool dipped_beam_status = false, left_fog_on = false, right_fog_on = false;
  int16_t steering_angle = 0;
  CAN_message_t front_left_fog_on_a_buf, front_left_fog_on_b_buf, front_left_fog_on_c_buf, front_left_fog_on_d_buf;
  CAN_message_t front_left_fog_on_e_buf, front_left_fog_on_f_buf, front_left_fog_on_g_buf, front_left_fog_on_h_buf;
  CAN_message_t front_right_fog_on_a_buf, front_right_fog_on_b_buf, front_right_fog_on_c_buf, front_right_fog_on_d_buf;
  CAN_message_t front_right_fog_on_e_buf, front_right_fog_on_f_buf, front_right_fog_on_g_buf, front_right_fog_on_h_buf;
  CAN_message_t front_left_fog_off_buf, front_right_fog_off_buf, front_fogs_all_off_buf;
  cppQueue fog_corner_txq(sizeof(delayed_can_tx_msg), 32, queue_FIFO);
#endif
#if DIM_DRL
  bool drl_status = false, left_dimmed = false, right_dimmed = false;
  CAN_message_t left_drl_dim_off, left_drl_dim_buf, left_drl_bright_buf;
  CAN_message_t right_drl_dim_off, right_drl_dim_buf, right_drl_bright_buf;
#endif
#if FTM_INDICATOR
  bool ftm_indicator_status = false;
  CAN_message_t ftm_indicator_flash_buf, ftm_indicator_off_buf;
#endif
#if HOOD_OPEN_GONG
  uint8_t last_hood_status = 0;
#endif
#if FRM_HEADLIGHT_MODE
  CAN_message_t frm_ckm_komfort_buf, frm_ckm_sport_buf;
#endif
#if WIPE_AFTER_WASH
  bool wipe_scheduled = false;
  uint8_t wash_message_counter = 0;
  CAN_message_t wipe_single_buf;
  cppQueue wiper_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if AUTO_MIRROR_FOLD
  bool mirrors_folded, frm_status_requested = false;
  bool lock_button_pressed  = false, unlock_button_pressed = false;
  uint8_t last_lock_status_can = 0;
  CAN_message_t frm_status_request_a_buf, frm_status_request_b_buf;
  CAN_message_t frm_toggle_fold_mirror_a_buf, frm_toggle_fold_mirror_b_buf;
  cppQueue mirror_fold_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if UNFOLD_WITH_DOOR
  bool unfold_with_door_open = false;
#endif
#if ANTI_THEFT_SEQ
  bool anti_theft_released = false;
  unsigned long anti_theft_send_interval = 0;                                                                                       // Skip the first interval delay.
  elapsedMillis anti_theft_timer = 0;
  uint8_t anti_theft_pressed_count = 0;
  CAN_message_t key_cc_on_buf, key_cc_off_buf;
  CAN_message_t start_cc_on_buf, start_cc_off_buf;
  CAN_message_t ekp_pwm_off_buf, ekp_return_to_normal_buf;
  cppQueue anti_theft_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
  cppQueue ekp_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if ANTI_THEFT_SEQ_ALARM
  bool alarm_after_engine_stall = false, alarm_active = false, alarm_led = false, lock_led = false;
  uint8_t led_message_counter = 60;
  CAN_message_t alarm_led_on_buf, alarm_led_off_buf;
  CAN_message_t alarm_siren_on_buf, alarm_siren_off_buf;
  cppQueue alarm_siren_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if REVERSE_BEEP
  CAN_message_t pdc_beep_buf, pdc_quiet_buf;
  bool pdc_beep_sent = false;
  cppQueue pdc_beep_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if REVERSE_BEEP || LAUNCH_CONTROL_INDICATOR || FRONT_FOG_CORNER || MSA_RVC
  bool reverse_gear_status = false;
#endif
#if F_ZBE_WAKE
  CAN_message_t f_wakeup_buf;
  uint8_t zbe_response[] = {0xE1, 0x9D, 0, 0xFF};
#endif
#if EXHAUST_FLAP_CONTROL
  bool exhaust_flap_sport = false, exhaust_flap_open = true;
  elapsedMillis exhaust_flap_action_timer;                                                                                          // Initialized when the engine is started.
  unsigned long exhaust_flap_action_interval = 1000;
#endif
#if LAUNCH_CONTROL_INDICATOR
  CAN_message_t lc_cc_on_buf, lc_cc_off_buf;
  bool lc_cc_active = false, mdm_with_lc = false, clutch_pressed = false;
#endif
float ambient_temperature_real = 87.5;
#if AUTO_SEAT_HEATING
  bool driver_seat_heating_status = false;
  bool driver_sent_seat_heating_request = false;
  CAN_message_t seat_heating_button_pressed_dr_buf, seat_heating_button_released_dr_buf;
  cppQueue seat_heating_dr_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if AUTO_SEAT_HEATING_PASS
  bool passenger_seat_heating_status = false;
  uint8_t passenger_seat_status = 0;                                                                                                // 0 - Not occupied not belted, 1 - not occupied and belted, 8 - occupied not belted, 9 - occupied and belted
  bool passenger_sent_seat_heating_request = false;
  CAN_message_t seat_heating_button_pressed_pas_buf, seat_heating_button_released_pas_buf;
  cppQueue seat_heating_pas_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if RTC
  #include <TimeLib.h>
  CAN_message_t set_time_cc_buf;
  bool rtc_valid = true;
#endif
#if DOOR_VOLUME
  bool volume_reduced = false;
  bool default_volume_sent = false;
  uint8_t volume_restore_offset = 0, volume_changed_to;
  CAN_message_t vol_request_buf, default_vol_set_buf, door_open_cc_off_buf;
  cppQueue idrive_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if DOOR_VOLUME || AUTO_MIRROR_FOLD || ANTI_THEFT_SEQ
  bool left_door_open = false, right_door_open = false;
  uint8_t last_door_status = 0;
#endif
#if CKM || DOOR_VOLUME || REVERSE_BEEP
  elapsedMillis idrive_alive_timer = 0;
  bool idrive_died = false;
#endif
#if HDC || ANTI_THEFT_SEQ || FRONT_FOG_CORNER
  uint16_t vehicle_speed = 0;
  bool speed_mph = false;
#endif
#if HDC
  uint8_t min_hdc_speed = 20, max_hdc_speed = 35;
  bool cruise_control_status = false, hdc_button_pressed = false, hdc_requested = false, hdc_active = false;
  CAN_message_t set_hdc_cruise_control_buf, cancel_hdc_cruise_control_buf;
  CAN_message_t hdc_cc_activated_on_buf, hdc_cc_unavailable_on_buf, hdc_cc_deactivated_on_buf;
  CAN_message_t hdc_cc_activated_off_buf, hdc_cc_unavailable_off_buf, hdc_cc_deactivated_off_buf;
#endif
#if LAUNCH_CONTROL_INDICATOR || HDC || ANTI_THEFT_SEQ || FRONT_FOG_CORNER || HOOD_OPEN_GONG
  bool vehicle_moving = false;
#endif
#if FAKE_MSA
  CAN_message_t msa_deactivated_cc_on_buf, msa_deactivated_cc_off_buf;
#endif
#if MSA_RVC
  uint8_t pdc_status = 0x80;
  bool pdc_button_pressed = false, pdc_with_rvc_requested = false;
  CAN_message_t camera_off_buf, camera_on_buf, pdc_off_camera_on_buf, pdc_on_camera_on_buf, pdc_off_camera_off_buf;
#endif
#if FAKE_MSA || MSA_RVC
  bool msa_button_pressed = false;
  uint8_t msa_fake_status_counter = 0;
  CAN_message_t msa_fake_status_buf;
#endif
#if HDC || FAKE_MSA
  cppQueue ihk_extra_buttons_cc_txq(sizeof(delayed_can_tx_msg), 16, queue_FIFO);
#endif
#if DEBUG_MODE
  float battery_voltage = 0;
  extern float tempmonGetTemp(void);
  char serial_debug_string[512];
  elapsedMillis debug_print_timer = 0;
  unsigned long max_loop_timer = 0, loop_timer = 0, setup_time;
  uint32_t kcan_error_counter = 0, ptcan_error_counter = 0, dcan_error_counter = 0;
  bool serial_commands_unlocked = false;
#endif
#if DEBUG_MODE || ANTI_THEFT_SEQ
  #include "secrets.h"
#endif
bool diag_transmit = true;
elapsedMillis diag_deactivate_timer = 0;
