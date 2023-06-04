// See program-notes.txt for details on how this sketch works.


#include "E9X-M-CAN-Integration-Module.h"
void setup() {
  if ( F_CPU_ACTUAL > (528 * 1000000)) {
    set_arm_clock(528 * 1000000);                                                                                                   // Prevent accidental overclocks. Remove if needed.
  }
  cpu_speed_ide = F_CPU_ACTUAL;
  configure_IO();
  configure_can_controllers();
  initialize_watchdog();
  cache_can_message_buffers();
  read_settings_from_eeprom();
  #if UNFOLD_WITH_DOOR
    unfold_with_door_open = EEPROM.read(11) == 1 ? true : false;
  #endif
  #if ANTI_THEFT_SEQ
    anti_theft_released = EEPROM.read(12) == 1 ? true : false;
  #endif
  #if RTC
    check_rtc_valid();
  #endif
  #if !USB_DISABLE
    // This code allows compatibility with unmodified Teensy cores. Do not enable USB_DISABLE without modifying startup.c!
    if (!(CCM_CCGR6 & CCM_CCGR6_USBOH3(CCM_CCGR_ON))){                                                                              // Check if USB is already ON.
      usb_pll_start();
      usb_init();
    }
  #endif
  #if DEBUG_MODE
    #if AUTO_MIRROR_FOLD && !USB_DISABLE
      setup_time = micros() - 300000;
    #else
      setup_time = micros();
    #endif
  #endif
}


void loop() {
/***********************************************************************************************************************************************************************************************************************************************
  General section.
***********************************************************************************************************************************************************************************************************************************************/
  
  if (vehicle_awake) {
    #if EXHAUST_FLAP_CONTROL
      control_exhaust_flap_user();
    #endif
    check_diag_transmit_status();
    #if DOOR_VOLUME
      check_idrive_queue();
    #endif
    #if NEEDLE_SWEEP
      check_kombi_needle_queue();
    #endif
    #if WIPE_AFTER_WASH
      check_wiper_queue();
    #endif
    #if AUTO_MIRROR_FOLD
      check_mirror_fold_queue();
    #endif
    #if ANTI_THEFT_SEQ
      check_anti_theft_status();
    #endif
    #if FRONT_FOG_CORNER
      check_fog_corner_queue();
    #endif
    #if CKM || DOOR_VOLUME || REVERSE_BEEP
      check_idrive_alive_monitor();
    #endif
  }

  if (ignition) {
    check_teensy_cpu_temp();                                                                                                        // Monitor processor temperature to extend lifetime.
    check_dsc_queue();
    check_console_buttons();
    send_mdrive_alive_message(10000);
    #if AUTO_SEAT_HEATING
      check_seatheating_queue();
    #endif
    #if REVERSE_BEEP
      check_reverse_beep_queue();
    #endif
    #if HDC || FAKE_MSA
      check_ihk_buttons_cc_queue();
    #endif
    #if EDC_CKM_FIX
      check_edc_ckm_queue();
    #endif
  } else {
    if (vehicle_awake) {
      if (vehicle_awake_timer >= 2000) {
        vehicle_awake = false;                                                                                                      // Vehicle must now be asleep. Stop monitoring .
        serial_log("Vehicle Sleeping.");
        toggle_transceiver_standby();
        scale_mcu_speed();
        reset_sleep_variables();
      }
      send_mdrive_alive_message(15000);                                                                                             // Send this message while car is awake (but with ignition OFF) to populate the fields in iDrive.
    }
  }


/***********************************************************************************************************************************************************************************************************************************************
  K-CAN section.
***********************************************************************************************************************************************************************************************************************************************/
  
  if (KCAN.read(k_msg)) {
    if (ignition) {
      if (k_msg.id == 0xAA) {                                                                                                       // Monitor 0xAA (rpm/throttle status).
        evaluate_engine_rpm();

        // Shiftlights and LC depend on RPM. Since this message is cycled every 100ms, it makes sense to run the calculations now.
        #if CONTROL_SHIFTLIGHTS
          evaluate_shiftlight_display();
        #endif
        #if LAUNCH_CONTROL_INDICATOR
          evaluate_lc_display();
        #endif
        #if EXHAUST_FLAP_CONTROL
          control_exhaust_flap_rpm();
        #endif
      }

      #if LAUNCH_CONTROL_INDICATOR
      else if (k_msg.id == 0xA8) {                                                                                                  // Monitor clutch pedal status
        evaluate_clutch_status();
      }
      #endif

      #if HDC
      else if (k_msg.id == 0x193) {                                                                                                 // Monitor state of cruise control
        evaluate_cruise_control_status();
      }
      else if (k_msg.id == 0x31A) {                                                                                                 // Received HDC button press from IHKA.
        evaluate_hdc_button();
      }
      #endif

      #if FAKE_MSA || MSA_RVC
      else if (k_msg.id == 0x195) {                                                                                                 // Monitor state of MSA button
        evaluate_msa_button();
      }
      #endif

      #if LAUNCH_CONTROL_INDICATOR || HDC || ANTI_THEFT_SEQ || FRONT_FOG_CORNER || HOOD_OPEN_GONG
      else if (k_msg.id == 0x1B4) {                                                                                                 // Monitor if the car is stationary/moving
        evaluate_vehicle_moving();
      }
      #endif

      #if DIM_DRL || FRONT_FOG_CORNER
      else if (k_msg.id == 0x1F6) {                                                                                                 // Monitor indicator status
        evaluate_indicator_status_dim();
      }
      #endif

      #if FRONT_FOG_LED_INDICATOR || FRONT_FOG_CORNER || DIM_DRL
      else if (k_msg.id == 0x21A) {                                                                                                 // Light status sent by the FRM.
        #if FRONT_FOG_LED_INDICATOR || FRONT_FOG_CORNER
          evaluate_fog_status();
        #endif
        #if FRONT_FOG_CORNER
          evaluate_dipped_beam_status();
        #endif
        #if DIM_DRL
          evaluate_drl_status();
        #endif
      }
      #endif

      #if AUTO_SEAT_HEATING
      #if AUTO_SEAT_HEATING_PAS
      else if (k_msg.id == 0x22A || k_msg.id == 0x232) {                                                                            // Monitor passenger and driver's seat heating.
      #else
      else if (k_msg.id == 0x232) { 
      #endif
        evaluate_seat_heating_status();
      }
      #endif

      #if MSA_RVC
      else if (k_msg.id == 0x317) {                                                                                                 // Received PDC button press from IHKA.
        evaluate_pdc_button();
      }
      #endif

      #if CKM
      else if (k_msg.id == 0x3A8) {                                                                                                 // Received POWER M Key settings from iDrive.
        update_dme_power_ckm();
      }
      #endif

      #if MSA_RVC
      else if (k_msg.id == 0x3AF) {                                                                                                 // Monitor PDC bus status.
        evaluate_pdc_bus_status();
      }
      #endif

      #if REVERSE_BEEP || LAUNCH_CONTROL_INDICATOR || FRONT_FOG_CORNER || MSA_RVC
      else if (k_msg.id == 0x3B0) {                                                                                                 // Monitor reverse status.
        evaluate_reverse_gear_status();
        #if REVERSE_BEEP
          evaluate_pdc_beep();
        #endif
      }
      #endif

      #if EDC_CKM_FIX
      else if (k_msg.id == 0x3C5) {                                                                                                 // Received EDC M Key settings from iDrive.
        update_edc_ckm();
      }
      #endif

      else if (k_msg.id == 0x3CA) {                                                                                                 // Receive settings from iDrive.      
        update_mdrive_message_settings();
      }
    }

    if (k_msg.id == 0x130) {                                                                                                        // Monitor ignition status
      evaluate_ignition_status();
    }

    #if CKM
    else if (k_msg.id == 0x1AA) {                                                                                                   // Time POWER CKM message with iDrive ErgoCommander.
      send_dme_power_ckm();
    }
    #endif

    #if AUTO_MIRROR_FOLD || CKM
    else if (k_msg.id == 0x23A) {                                                                                                   // Monitor remote function status
      #if AUTO_MIRROR_FOLD
        evaluate_remote_button();
      #endif
      #if CKM
        evaluate_key_number();
      #endif
    }
    #endif

    #if F_ZBE_WAKE || CKM || DOOR_VOLUME || REVERSE_BEEP
    else if (k_msg.id == 0x273) {
      #if F_ZBE_WAKE
        send_zbe_acknowledge();
      #endif
      #if CKM || DOOR_VOLUME || REVERSE_BEEP
        update_idrive_alive_timer();
      #endif
    }
    #endif

    else if (k_msg.id == 0x2CA) {                                                                                                   // Monitor and update ambient temperature.
      evaluate_ambient_temperature();
    }

    #if AUTO_SEAT_HEATING_PASS
    else if (k_msg.id == 0x2FA) {                                                                                                   // Monitor and update seat status
      evaluate_passenger_seat_status();
    } 
    #endif

    #if HDC || ANTI_THEFT_SEQ || FRONT_FOG_CORNER
    else if (k_msg.id == 0x2F7) {
      evaluate_speed_units();
    }
    #endif

    #if RTC
    else if (k_msg.id == 0x2F8) {
      if (k_msg.buf[0] == 0xFD && k_msg.buf[7] == 0xFC) {                                                                           // Date/time not set
        update_car_time_from_rtc();
      }
    }

    #if DOOR_VOLUME || AUTO_MIRROR_FOLD || ANTI_THEFT_SEQ || HOOD_OPEN_GONG
    if (k_msg.id == 0x2FC) {
      evaluate_door_status();
    }
    #endif

    else if (k_msg.id == 0x39E) {                                                                                                   // Received new date/time from CIC.
      update_rtc_from_idrive();
    }
    #endif

    #if F_ZBE_WAKE
    else if (k_msg.id == 0x4E2) {
      #if F_ZBE_WAKE
        send_zbe_wakeup();
      #endif
    }
    #endif

    #if DOOR_VOLUME
    else if (k_msg.id == 0x5C0) {
      disable_door_ignition_cc();
    }

    else if (k_msg.id == 0x663) {
      evaluate_audio_volume();
    }
    #endif

    #if AUTO_MIRROR_FOLD || FRONT_FOG_CORNER
    else if (k_msg.id == 0x672) {
      #if AUTO_MIRROR_FOLD 
        evaluate_mirror_fold_status();
      #endif
    }
    #endif
  }


/***********************************************************************************************************************************************************************************************************************************************
  PT-CAN section.
***********************************************************************************************************************************************************************************************************************************************/
  
  if (vehicle_awake) {
    if (PTCAN.read(pt_msg)) {                                                                                                       // Read data.
      #if ANTI_THEFT_SEQ                                                                                                            // We need this button data with ignition off too
      if (pt_msg.id == 0x1D6) {                                                                                                     // A button was pressed on the steering wheel.
        evaluate_m_mfl_button_press();
      }
      #endif

      #if WIPE_AFTER_WASH
      else if (pt_msg.id == 0x2A6) {
        evaluate_wiping_request();
      }
      #endif

      #if DEBUG_MODE
      else if (pt_msg.id == 0x3B4) {                                                                                                // Monitor battery voltage from DME.
        evaluate_battery_voltage();
      }
      #endif
      
      if (ignition) {              
        if (pt_msg.id == 0x315) {                                                                                                   // Monitor EDC status message from the JBE.
          #if EDC_CKM_FIX
            evaluate_edc_ckm_mismatch();
          #endif
          send_power_mode();                                                                                                        // state_spt request from DME.   
          #if SERVOTRONIC_SVT70
            send_servotronic_message();
          #endif
        }

        #if FRONT_FOG_CORNER
        else if (pt_msg.id == 0xC8) {
          evaluate_steering_angle_fog();
        }
        #endif

        #if !ANTI_THEFT_SEQ
        else if (pt_msg.id == 0x1D6) {
          evaluate_m_mfl_button_press();
        }
        #endif
      
        #if FTM_INDICATOR
        else if (pt_msg.id == 0x31D) {                                                                                              // FTM initialization is ongoing.
          evaluate_indicate_ftm_status();
        }
        #endif  

        #if CONTROL_SHIFTLIGHTS
        else if (pt_msg.id == 0x332) {                                                                                              // Monitor variable redline broadcast from DME.
          evaluate_update_shiftlight_sync();
        }
        #endif

        #if SERVOTRONIC_SVT70
        else if (pt_msg.id == 0x58E) {                                                                                              // Since the JBE doesn't forward Servotronic errors from SVT70, we have to do it.
          send_svt_kcan_cc_notification();
        }
        else if (pt_msg.id == 0x60E) {                                                                                              // Forward Diagnostic responses from SVT module to DCAN
          if (diagnose_svt) {
            ptcan_to_dcan();
          }
        }
        #endif
      }
    }
  }


/***********************************************************************************************************************************************************************************************************************************************
  D-CAN section.
***********************************************************************************************************************************************************************************************************************************************/
    
  if (DCAN.read(d_msg)) {
    if (d_msg.id == 0x6F1) {
      // MHD monitoring exceptions:
      if (d_msg.buf[0] == 0x12){
        if ((d_msg.buf[3] == 0x34 && d_msg.buf[4] == 0x80)) {                                                                       // SID 34h requestDownload Service
          disable_diag_transmit_jobs();
        }
      }
      else if (d_msg.buf[0] == 0x60 && (d_msg.buf[1] == 0x30 || d_msg.buf[1] == 3)){}
      else if (d_msg.buf[0] == 0x40 && (d_msg.buf[1] == 0x30 || d_msg.buf[1] == 3)){}
      // End MHD exceptions.

      #if SERVOTRONIC_SVT70
      else if (d_msg.buf[0] == 0xE) {                                                                                               // SVT_70 address is 0xE
        if (diagnose_svt) {
          dcan_to_ptcan();                                                                                                          // Forward Diagnostic requests to the SVT module from DCAN to PTCAN
        }
      } 
      #endif
      
      #if RTC
      else if (d_msg.buf[0] == 0x60 && (d_msg.buf[1] == 0x10 || d_msg.buf[1] == 0x21)) {                                            // KOMBI is at address 0x60. ISTA sets time by sending it to KOMBI.
        update_rtc_from_dcan();
      }
      #endif

      else {
        disable_diag_transmit_jobs();                                                                                               // Implement a check so as to not interfere with other DCAN jobs sent to the car by an OBD tool.    
      }
      diag_deactivate_timer = 0;
    }
  }
  
/**********************************************************************************************************************************************************************************************************************************************/

  #if DEBUG_MODE && CDC2_STATUS_INTERFACE == 2                                                                                      // Check if Dual Serial is set.
    if (debug_print_timer >= 500) {
      print_current_state(SerialUSB1);                                                                                              // Print program status to the second Serial port.
    }
  #endif
  #if DEBUG_MODE
    loop_timer = micros();
    while (Serial.available()) {
      serial_interpreter();
    }
  #endif
  
  wdt.feed();                                                                                                                       // Reset the watchdog timer.
}
