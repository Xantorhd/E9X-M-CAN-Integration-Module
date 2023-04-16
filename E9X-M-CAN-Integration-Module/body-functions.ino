void evaluate_ignition_status()
{
  vehicle_awake_timer = millis();
  if (!vehicle_awake) {
    vehicle_awake = true;    
    toggle_transceiver_standby();                                                                                                   // Re-activate the transceivers.                                                                                         
    serial_log("Vehicle Awake.");
    #if DOOR_VOLUME
      send_default_startup_volume();
    #endif
  }  

  bool ignition_ = ignition;
  switch(k_msg.buf[0]) {
    case 0:
      ignition = terminal_r = engine_cranking = false;
      break;
    case 1:
      terminal_r = true;
      ignition = engine_cranking = false;
      break;
    case 5:
      ignition = terminal_r = engine_cranking = false;
      break;                                                                                                                        // 5 is sent in CA cars when the key is not detected, ignore.
    case 0x41:
      terminal_r = true;
      ignition = engine_cranking = false;
      break;
    case 0x45:
      ignition = terminal_r = true;
      engine_cranking = false;
      break;
    case 0x55:
      ignition = terminal_r = engine_cranking = true;
      break;
  }

  #if FAKE_MSA
    if (ignition) {
      if (msa_fake_status_counter == 5){
        kcan_write_msg(msa_fake_status_buf);                                                                                        // Send this message every 500ms to keep the IHKA module happy.
        msa_fake_status_counter = 0;
      }
      msa_fake_status_counter++;
    }
  #endif

  if (ignition && !ignition_) {                                                                                                     // Ignition changed from OFF to ON.
    scale_mcu_speed();
    #if SERVOTRONIC_SVT70
      if (!digitalRead(POWER_BUTTON_PIN)) {                                                                                         // If POWER button is being held when turning on ignition, allow SVT diagnosis.
        diagnose_svt = true;
        kcan_write_msg(servotronic_cc_on_buf);                                                                                      // Indicate that diagnosing is now possible.
      }
    #endif
    serial_log("Ignition ON.");    
  } else if (!ignition && ignition_) {
    reset_runtime_variables();
    scale_mcu_speed();                                                                                                              // Now that the ignition is off, underclock the MCU
    #if DEBUG_MODE
      sprintf(serial_debug_string, "(%X) Ignition OFF. Reset values.", k_msg.buf[0]);
      serial_log(serial_debug_string);
    #endif
  }
}


#if FRONT_FOG_INDICATOR || DIM_DRL
void evaluate_light_status()
{
  #if FRONT_FOG_INDICATOR 
    if (k_msg.buf[0] != last_light_status) {
      if ((k_msg.buf[0] & 32) == 32) {                                                                                              // Check the third bit of the first byte represented in binary for front fog status.
        if (!front_fog_status) {
          front_fog_status = true;
          digitalWrite(FOG_LED_PIN, HIGH);
          serial_log("Front fogs on. Turned on FOG LED");
        }
      } else {
        if (front_fog_status) {
          front_fog_status = false;
          digitalWrite(FOG_LED_PIN, LOW);
          serial_log("Front fogs off. Turned off FOG LED");
        }
      }
      last_light_status = k_msg.buf[0];
    }
  #endif
  
  #if DIM_DRL
    if (k_msg.buf[1] == 0x32) {
      if (!drl_status) {
        drl_status = true;
        serial_log("DRLs on.");
      }
    } else {
      if (drl_status) {
        drl_status = false;
        serial_log("DRLs off");
      }
    }
  #endif
}
#endif


#if DIM_DRL
void evaluate_indicator_status_dim()
{
  if (drl_status && diag_transmit) {
    if(k_msg.buf[0] == 0x80 || k_msg.buf[0] == 0xB1) {                                                                              // Off or Hazards
      if (right_dimmed) {
        kcan_write_msg(right_drl_bright_buf);
        right_dimmed = false;
        serial_log("Restored right DRL brightness.");
      } else if (left_dimmed) {
        kcan_write_msg(left_drl_bright_buf);
        left_dimmed = false;
        serial_log("Restored left DRL brightness.");
      }
    } else if (k_msg.buf[0] == 0x91) {                                                                                              // Left
      if (right_dimmed) {
        kcan_write_msg(right_drl_bright_buf);
        right_dimmed = false;
        serial_log("Restored right DRL brightness.");
      }
      kcan_write_msg(left_drl_dim_buf);
      #if DEBUG_MODE
        if (!left_dimmed) {
          serial_log("Dimmed left DRL.");
        }
      #endif
      left_dimmed = true;
    } else if (k_msg.buf[0] == 0xA1) {                                                                                              // Right
      if (left_dimmed) {
        kcan_write_msg(left_drl_bright_buf);
        left_dimmed = false;
        serial_log("Restored left DRL brightness.");
      }
      kcan_write_msg(right_drl_dim_buf);
      #if DEBUG_MODE
        if (!right_dimmed) {
          serial_log("Dimmed right DRL.");
        }
      #endif
      right_dimmed = true;
    }
  }
}
#endif


#if AUTO_SEAT_HEATING
void evaluate_seat_heating_status()
{
  if (k_msg.id == 0x22A) {                                                                                                          // Passenger's seat heating status message is only sent with ignition on.
    passenger_seat_heating_status = !k_msg.buf[0] ? false : true;
  } else {
    if (!k_msg.buf[0]) {                                                                                                            // Check if seat heating is already on.
      if (!driver_sent_seat_heating_request && (ambient_temperature_can <= AUTO_SEAT_HEATING_TRESHOLD)) {
        send_seat_heating_request_dr();
      }
    } else {
      driver_sent_seat_heating_request = true;                                                                                      // Seat heating already on. No need to request anymore.
    }
    driver_seat_heating_status = !k_msg.buf[0] ? false : true;
  }
}


void evaluate_passenger_seat_status()
{
  passenger_seat_status = k_msg.buf[1];
  if (ignition) {
    if (!passenger_seat_heating_status) {                                                                                           // Check if seat heating is already on.
      //This will be ignored if already on and cycling ignition. Press message will be ignored by IHK anyway.
      if (!passenger_sent_seat_heating_request && (ambient_temperature_can <= AUTO_SEAT_HEATING_TRESHOLD)) { 
        if (passenger_seat_status == 9) {                                                                                           // Passenger sitting and their seatbelt is on
          send_seat_heating_request_pas();                                                                                          // Execute heating request here so we don't have to wait 15s for the next 0x22A.
        }
      }
    } else {
      passenger_sent_seat_heating_request = true;                                                                                   // Seat heating already on. No need to request anymore.
    }
  }
}


void evaluate_ambient_temperature()
{
  ambient_temperature_can = k_msg.buf[0];
}


void send_seat_heating_request_dr()
{
  unsigned long timeNow = millis();
  kcan_write_msg(seat_heating_button_pressed_dr_buf);
  driver_sent_seat_heating_request = true;
  delayed_can_tx_msg m = {seat_heating_button_released_dr_buf, timeNow + 100};
  seat_heating_dr_txq.push(&m);
  m = {seat_heating_button_released_dr_buf, timeNow + 250};
  seat_heating_dr_txq.push(&m);
  m = {seat_heating_button_released_dr_buf, timeNow + 400};
  seat_heating_dr_txq.push(&m);
  #if DEBUG_MODE
    sprintf(serial_debug_string, "Sent driver's seat heating request at ambient %.1fC, treshold %.1fC.", 
          (ambient_temperature_can - 80) / 2.0, (AUTO_SEAT_HEATING_TRESHOLD - 80) / 2.0);
    serial_log(serial_debug_string);
  #endif
}


void send_seat_heating_request_pas() 
{
  unsigned long timeNow = millis();
  kcan_write_msg(seat_heating_button_pressed_pas_buf);
  passenger_sent_seat_heating_request = true;
  delayed_can_tx_msg m = {seat_heating_button_released_pas_buf, timeNow + 100};
  seat_heating_pas_txq.push(&m);
  m = {seat_heating_button_released_pas_buf, timeNow + 250};
  seat_heating_pas_txq.push(&m);
  m = {seat_heating_button_released_pas_buf, timeNow + 400};
  seat_heating_pas_txq.push(&m);
  #if DEBUG_MODE
    sprintf(serial_debug_string, "Sent passenger's seat heating request at ambient %.1fC, treshold %.1fC.", 
          (ambient_temperature_can - 80) / 2.0, (AUTO_SEAT_HEATING_TRESHOLD - 80) / 2.0);
    serial_log(serial_debug_string);
  #endif
}


void check_seatheating_queue() 
{
  if (!seat_heating_dr_txq.isEmpty()) {
    delayed_can_tx_msg delayed_tx;
    seat_heating_dr_txq.peek(&delayed_tx);
    if (millis() >= delayed_tx.transmit_time) {
      kcan_write_msg(delayed_tx.tx_msg);
      seat_heating_dr_txq.drop();
    }
  }
  if (!seat_heating_pas_txq.isEmpty()) {
    delayed_can_tx_msg delayed_tx;
    seat_heating_pas_txq.peek(&delayed_tx);
    if (millis() >= delayed_tx.transmit_time) {
      kcan_write_msg(delayed_tx.tx_msg);
      seat_heating_pas_txq.drop();
    }
  }
}
#endif


#if F_ZBE_WAKE
void send_zbe_wakeup()
{
  kcan_write_msg(f_wakeup_buf);
  serial_log("Sent F-ZBE wake-up message.");
}


void send_zbe_acknowledge()
{
  zbe_response[2] = k_msg.buf[7];
  kcan_write_msg(makeMsgBuf(0x277, 4, zbe_response));
  #if DEBUG_MODE
    sprintf(serial_debug_string, "Sent ZBE response to CIC with counter: 0x%X", k_msg.buf[7]);
    serial_log(serial_debug_string);
  #endif
}
#endif


#if EXHAUST_FLAP_CONTROL
void control_exhaust_flap_user()
{
  if (engine_running) {
    if (exhaust_flap_sport) {                                                                                                       // Flap always open in sport mode.
      if ((millis() - exhaust_flap_action_timer) >= 500) {
        if (!exhaust_flap_open) {
          actuate_exhaust_solenoid(LOW);
          serial_log("Opened exhaust flap with MDrive.");
        }
      }
    }
  } else {
    #if QUIET_START
      if (ignition || terminal_r || engine_cranking) {
        if (exhaust_flap_open) {
          actuate_exhaust_solenoid(HIGH);                                                                                           // Close the flap (if vacuum still available)
          serial_log("Quiet start enabled. Exhaust flap closed.");
        }
      } else {
        if (!exhaust_flap_open) {
          actuate_exhaust_solenoid(LOW);
          serial_log("Released exhaust flap from quiet start.");
        }
      }
    #endif
  }
}


void control_exhaust_flap_rpm()
{
  if (engine_running) {
    if (!exhaust_flap_sport) {
      if ((millis() - exhaust_flap_action_timer) >= exhaust_flap_action_interval) {                                                 // Avoid vacuum drain, oscillation and apply startup delay.
        if (RPM >= EXHAUST_FLAP_QUIET_RPM) {                                                                                        // Open at defined rpm setpoint.
          if (!exhaust_flap_open) {
            actuate_exhaust_solenoid(LOW);
            serial_log("Exhaust flap opened at RPM setpoint.");
          }
        } else {
          if (exhaust_flap_open) {
            actuate_exhaust_solenoid(HIGH);
            serial_log("Exhaust flap closed.");
          }
        }
      }
    }
  }
}


void actuate_exhaust_solenoid(bool activate)
{
  digitalWrite(EXHAUST_FLAP_SOLENOID_PIN, activate);
  exhaust_flap_action_timer = millis();
  exhaust_flap_open = !activate;                                                                                                    // Flap position is the inverse of solenoid state. When active, the flap is closed.
}
#endif


#if REVERSE_BEEP
void evaluate_pdc_beep()
{
  if (k_msg.buf[0] == 0xFE) {
    if (!pdc_beep_sent) {
      unsigned long timeNow = millis();
      delayed_can_tx_msg m = {pdc_beep_buf, timeNow};
      pdc_beep_txq.push(&m);
      m = {pdc_quiet_buf, timeNow + 150};
      pdc_beep_txq.push(&m);
      pdc_beep_sent = true;
      serial_log("Sending PDC beep.");
    }
  } else {
    if (pdc_beep_sent) {
      pdc_beep_sent = false;
    }
  }
}


void check_pdc_queue() 
{
  if (!pdc_beep_txq.isEmpty()) {
    delayed_can_tx_msg delayed_tx;
    pdc_beep_txq.peek(&delayed_tx);
    if (millis() >= delayed_tx.transmit_time) {
      kcan_write_msg(delayed_tx.tx_msg);
      pdc_beep_txq.drop();
    }
  }
}
#endif


void evaluate_engine_rpm()
{
  RPM = ((uint16_t)k_msg.buf[5] << 8) | (uint16_t)k_msg.buf[4];
  if (!engine_running && (RPM > 2000)) {
    engine_running = true;
    #if CONTROL_SHIFTLIGHTS
      shiftlight_startup_animation();                                                                                               // Show off shift light segments during engine startup (>500rpm).
    #endif
    #if NEEDLE_SWEEP
      needle_sweep_animation();
    #endif
    #if EXHAUST_FLAP_CONTROL
      exhaust_flap_action_timer = millis();                                                                                         // Start tracking the exhaust flap.
    #endif
    serial_log("Engine started.");
  } else if (engine_running && (RPM < 200)) {                                                                                       // Less than 50 RPM. Engine stalled or was stopped.
    engine_running = false;
    serial_log("Engine stopped.");
  }
}


#if DEBUG_MODE
void evaluate_battery_voltage() 
{
  battery_voltage = (((k_msg.buf[1] - 240 ) * 256.0) + k_msg.buf[0]) / 68.0;
}
#endif


#if SERVOTRONIC_SVT70
void send_svt_kcan_cc_notification()
{
  if (pt_msg.buf[1] == 0x49 && pt_msg.buf[2] == 0) {                                                                                // Change from CC-ID 73 (EPS Inoperative) to CC-ID 70 (Servotronic).
    pt_msg.buf[1] = 0x46;
  }
  kcan_write_msg(pt_msg);                                                                                                           // Forward the SVT error status to KCAN.
}


void dcan_to_ptcan()
{
  if (!deactivate_ptcan_temporariliy) {
    ptcan_write_msg(d_msg);
    #if DEBUG_MODE
      dcan_forwarded_count++;
    #endif
  }
}


void ptcan_to_dcan()
{
  dcan_write_msg(pt_msg);
  #if DEBUG_MODE
    ptcan_forwarded_count++;
  #endif
}
#endif


void check_console_buttons()
{
  if (!digitalRead(POWER_BUTTON_PIN)) {
    if ((millis() - power_button_debounce_timer) >= power_debounce_time_ms) {                                                       // POWER console button should only change throttle mapping.
      power_button_debounce_timer = millis();
      if (!console_power_mode) {
        if (!mdrive_power_active) {
          console_power_mode = true;
          serial_log("Console: POWER mode ON.");
        } else {
          mdrive_power_active = false;                                                                                              // If POWER button was pressed while MDrive POWER is active, disable POWER.
          serial_log("Deactivated MDrive POWER with console button press.");
        }
      } else {
        serial_log("Console: POWER mode OFF.");
        console_power_mode = false;
        if (mdrive_power_active) {
          mdrive_power_active = false;                                                                                              // If POWER button was pressed while MDrive POWER is active, disable POWER.
          serial_log("Deactivated MDrive POWER with console button press.");
        }
      }
    }
  } 
  
  if (!digitalRead(DSC_BUTTON_PIN)) {
    if (dsc_program_status != 2) {
      if (!holding_dsc_off_console) {
        holding_dsc_off_console = true;
        dsc_off_button_hold_timer = millis();
      } else {
        if ((millis() - dsc_off_button_hold_timer) >= dsc_hold_time_ms) {                                                           // DSC OFF sequence should only be sent after user holds button for a configured time
          if ((millis() - dsc_off_button_debounce_timer) >= dsc_debounce_time_ms) {
            serial_log("Console: DSC OFF button held. Sending DSC OFF.");
            send_dsc_mode(2);
            dsc_off_button_debounce_timer = millis();
          }
        }
      }      
    } else {
      if ((millis() - dsc_off_button_debounce_timer) >= dsc_debounce_time_ms) {                                                     // A quick tap re-enables everything
        serial_log("Console: DSC button tapped. Sending DSC ON.");
        dsc_off_button_debounce_timer = millis();
        send_dsc_mode(0);
      }
    }
  } else {
    holding_dsc_off_console = false;
  }
}


#if RTC
void update_car_time_from_rtc()
{
  if (rtc_valid) {                                                                                                                  // Make sure time in RTC is actually valid before forcing it.
    serial_log("Vehicle date/time not set. Setting from RTC.");
    time_t t = now();
    uint8_t rtc_hours = hour(t);
    uint8_t rtc_minutes = minute(t);
    uint8_t rtc_seconds = second(t);
    uint8_t rtc_day = day(t);
    uint8_t rtc_month = month(t);
    uint16_t rtc_year = year(t);
    uint8_t date_time_can[] = {rtc_hours, rtc_minutes, rtc_seconds, 
                              rtc_day, uint8_t((rtc_month << 4) | 0xF), uint8_t(rtc_year & 0xFF), uint8_t(rtc_year >> 8), 0xF2};
    kcan_write_msg(makeMsgBuf(0x39E, 8, date_time_can));
  } else {
    serial_log("Teensy RTC invalid. Cannot set car's clock.");
  }
}
#endif


#if DOOR_VOLUME
void evaluate_door_status()
{
  if (k_msg.id == 0xE2) {
    if (k_msg.buf[3] == 0xFD) {
      if (!left_door_open) {
        left_door_open = true;
        #if RHD
          serial_log("Passenger's door open.");
        #else
          serial_log("Driver's door open.");
        #endif
        send_volume_request();
      }
    } else if (k_msg.buf[3] == 0xFC) {
      if (left_door_open) {
        left_door_open = false;
        #if RHD
          serial_log("Passenger's door closed.");
        #else
          serial_log("Driver's door closed.");
        #endif
        send_volume_request();
      }
    }
  } else if (k_msg.id == 0xEA) {
    if (k_msg.buf[3] == 0xFD) {
      if (!right_door_open) {
        right_door_open = true;
        #if RHD
          serial_log("Driver's door opened.");
        #else
          serial_log("Passenger's door opened.");
        #endif
        send_volume_request();
      }
    } else if (k_msg.buf[3] == 0xFC) {
      if (right_door_open) {
        right_door_open = false;
        #if RHD
          serial_log("Driver's door closed.");
        #else
          serial_log("Passenger's door closed.");
        #endif
        send_volume_request();
      }
    }
  }
}


void send_volume_request()
{
  if (!volume_requested && diag_transmit && default_volume_sent) {
    volume_requested = true;
    kcan_write_msg(vol_request_buf);
  }
}


void evaluate_audio_volume()
{
  if (volume_requested) {                                                                                                           // Make sure that the module is the one that requested this result.
    if (k_msg.buf[3] == 0x24) {                                                                                                     // status_volumeaudio response.
      uint8_t audio_volume = k_msg.buf[4];
      if (audio_volume > 0) {
        #if DEBUG_MODE
          sprintf(serial_debug_string, "Received audio volume: 0x%X", audio_volume);
          serial_log(serial_debug_string);
        #endif
        uint8_t volume_change[] = {0x63, 4, 0x31, 0x23, 0, 0, 0, 0};
        if (!volume_reduced) {
          if (left_door_open || right_door_open) {
            volume_restore_offset = (audio_volume % 2) == 0 ? 0 : 1;                                                                // Volumes adjusted from faceplate go up by 1 while MFL goes up by 2.
            volume_change[4] = floor(audio_volume / 2);                                                                             // Reduce volume to 50%.
            if ((volume_change[4] + volume_restore_offset) > 0x20) {                                                                // Don't blow the speakers out if something went wrong...
              volume_change[4] = 0x20;
              volume_restore_offset = 0;
            }
            kcan_write_msg(makeMsgBuf(0x6F1, 8, volume_change));
            volume_reduced = true;
            volume_changed_to = volume_change[4];                                                                                   // Save this value to compare when door is closed back.
            #if DEBUG_MODE
              sprintf(serial_debug_string, "Reducing audio volume with door open to: 0x%X", volume_changed_to);
              serial_log(serial_debug_string);
            #endif
          }
        } else {
          if (!left_door_open && !right_door_open) {
            if (volume_reduced) {
              if (audio_volume == volume_changed_to) {
                volume_change[4] = audio_volume * 2 + volume_restore_offset;
                if (volume_change[4] > 0x33) {
                  volume_change[4] = 0x33;                                                                                          // Set a nanny in case the code goes wrong. 0x33 is pretty loud...
                }
                delayed_can_tx_msg m;
                unsigned long timeNow = millis();
                if (ignition && !engine_running) {
                  m = {makeMsgBuf(0x6F1, 8, volume_change), timeNow + 500};                                                         // Need this delay when Ignition is on and the warning gong is on.
                  idrive_txq.push(&m);
                  m = {makeMsgBuf(0x6F1, 8, volume_change), timeNow + 1000};                                                        // Send more in case we get lucky and the gong shuts up early...
                  idrive_txq.push(&m);
                  m = {makeMsgBuf(0x6F1, 8, volume_change), timeNow + 1700};
                  idrive_txq.push(&m);
                } else {
                  m = {makeMsgBuf(0x6F1, 8, volume_change), timeNow + 200};
                  idrive_txq.push(&m);
                  m = {makeMsgBuf(0x6F1, 8, volume_change), timeNow + 500};
                  idrive_txq.push(&m);
                }
                #if DEBUG_MODE
                  sprintf(serial_debug_string, "Restoring audio volume with door closed. to: 0x%X", volume_change[4]);
                  serial_log(serial_debug_string);
                #endif
              }
              volume_reduced = false;
            }
          }
        }
      }
    }
    volume_requested = false;
  }
}


void send_default_startup_volume() {
  delayed_can_tx_msg m = {default_vol_set_buf, millis() + IDRIVE_BOOT_TIME};
  idrive_txq.push(&m);                                                                                                              // Send this after the iDrive wakes up to ensure a volume is set.
}
#endif


#if DOOR_VOLUME
void check_idrive_queue()
{
  if (!idrive_txq.isEmpty() && diag_transmit) {
    delayed_can_tx_msg delayed_tx;
    idrive_txq.peek(&delayed_tx);
    if (millis() >= delayed_tx.transmit_time) {
      if (vehicle_awake) {
        kcan_write_msg(delayed_tx.tx_msg);
        #if DOOR_VOLUME
          if (delayed_tx.tx_msg.buf[3] == default_vol_set_buf.buf[3]) {
            default_volume_sent = true;
            serial_log("Sent default volume job to iDrive.");
          } else if (delayed_tx.tx_msg.buf[3] == 0x23) {
            serial_log("Sent volume change job to iDrive.");
          }
        #endif
      }
      idrive_txq.drop();
    }
  }
}
#endif


void disable_diag_transmit_jobs()
{
  if (diag_transmit) {
    diag_transmit = false;
    #if DOOR_VOLUME
      volume_requested = false;
      volume_changed_to = 0;
      volume_restore_offset = 0;
    #endif
  }
  diag_deactivate_timer = millis();
}


void check_diag_transmit_status()
{
  if (diag_transmit && vehicle_awake) {
    if ((millis() - diag_deactivate_timer) >= 60000) {                                                                              // Re-activate after period of no DCAN requests.
      diag_transmit = false;
    }
  }
}


#if FAKE_MSA
void evaluate_msa_button()
{
  if (k_msg.buf[0] == 0xF5) {                                                                                                       // Button pressed.
    if (!msa_button_pressed) {
      if (engine_running) {
        kcan_write_msg(msa_deactivated_cc_on_buf);
        serial_log("Sent MSA OFF CC.");
        delayed_can_tx_msg m = {msa_deactivated_cc_off_buf, millis() + 3000};
        kcan_cc_txq.push(&m);
      }
    }
    msa_button_pressed = true;
  } else {                                                                                                                          // Now receiving released (0xF4) messages from IHKA.
    msa_button_pressed = false;
  }
}
#endif


#if HDC || FAKE_MSA
void check_kcan_cc_queue()
{
  if (!kcan_cc_txq.isEmpty()) {
    delayed_can_tx_msg delayed_tx;
    kcan_cc_txq.peek(&delayed_tx);
    if (millis() >= delayed_tx.transmit_time) {
      kcan_write_msg(delayed_tx.tx_msg);
      kcan_cc_txq.drop();
    }
  }
}
#endif
