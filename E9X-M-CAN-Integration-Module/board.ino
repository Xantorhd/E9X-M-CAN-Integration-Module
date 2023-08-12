// Functions configuring Teensy, peripherals and its transceivers go here.


void configure_IO(void) {
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);                                                                                                 
  pinMode(DSC_BUTTON_PIN, INPUT_PULLUP);
  pinMode(POWER_LED_PIN, OUTPUT);
  #if FRONT_FOG_LED_INDICATOR
    pinMode(FOG_LED_PIN, OUTPUT);
  #endif
  #if EXHAUST_FLAP_CONTROL
    pinMode(EXHAUST_FLAP_SOLENOID_PIN, OUTPUT);
    actuate_exhaust_solenoid(LOW);                                                                                                  // Keep the solenoid de-energised (flap open)
  #endif
  #if AUTO_STEERING_HEATER
    pinMode(STEERING_HEATER_SWITCH_PIN, OUTPUT);
    digitalWrite(STEERING_HEATER_SWITCH_PIN, LOW);
  #endif
  #if RTC
    setSyncProvider(get_teensy_time);
  #endif

  uint8_t disable_pins[40] = {1};                                                                                                   // Disable unused pins to save a tiny bit of current.
  disable_pins[0] = 0;                                                                                                              // CAN2
  disable_pins[1] = 0;
  disable_pins[11] = 0;                                                                                                             // CAN3
  disable_pins[13] = 0;
  disable_pins[22] = 0;                                                                                                             // CAN1
  disable_pins[23] = 0;
  disable_pins[POWER_BUTTON_PIN] = 0;
  disable_pins[POWER_LED_PIN] = 0;
  disable_pins[FOG_LED_PIN] = 0;
  #if AUTO_STEERING_HEATER
    disable_pins[STEERING_HEATER_SWITCH_PIN] = 0;
  #endif
  disable_pins[DCAN_STBY_PIN] = 0;
  disable_pins[PTCAN_STBY_PIN] = 0;
  disable_pins[DSC_BUTTON_PIN] = 0;
  #if EXHAUST_FLAP_CONTROL
    disable_pins[EXHAUST_FLAP_SOLENOID_PIN] = 0;
  #endif

  for (uint i = 0; i < 40; i++) {
    if (disable_pins[i]) {
      pinMode(disable_pins[i], INPUT_DISABLE);
    }
  }
}


void scale_cpu_speed(void) {
  if (ignition) {
    set_arm_clock(STANDARD_CLOCK);
    #if DEBUG_MODE
      max_loop_timer = 0;
    #endif
  } else {
    set_arm_clock(MAX_UNDERCLOCK);                                                                                                  // Reduce core clock.
    #if DEBUG_MODE
      max_loop_timer = 0;
    #endif
  }
}


void configure_can_controller(void) {
  uint8_t filter_count = 0;

  // KCAN
  KCAN.begin();
  KCAN.setClock(CLK_60MHz);                                                                                                         // Increase from the default 24MHz clock. Run before setBaudRate.
  KCAN.setBaudRate(100000);                                                                                                         // 100k
  KCAN.enableFIFO();                                                                                                                // Activate FIFO mode.
  KCAN.setMaxMB(40);                                                                                                                // Increase max filters
  KCAN.setRFFN(RFFN_40);
  KCAN.setFIFOFilter(REJECT_ALL);                                                                                                   // Reject unfiltered messages

  KCAN.setFIFOFilter(filter_count, 0xAA, STD);                                                                                      // RPM, throttle pos.                                           Cycle time 100ms (KCAN)
  filter_count++;
  KCAN.setFIFOFilter(filter_count, 0x130, STD);                                                                                     // Key/ignition status                                          Cycle time 100ms
  filter_count++;
  #if HDC
    KCAN.setFIFOFilter(filter_count, 0x193, STD);                                                                                   // Kombi cruise control status
    filter_count++;
  #endif
  #if FAKE_MSA || MSA_RVC
    KCAN.setFIFOFilter(filter_count, 0x195, STD);                                                                                   // MSA button status sent by IHKA.
    filter_count++;
  #endif
  #if PWR_CKM
    KCAN.setFIFOFilter(filter_count, 0x1AA, STD);                                                                                   // iDrive ErgoCommander (rear entertainment?)                   Sent at boot time and when cycling Terminal R.
    filter_count++;
  #endif
  KCAN.setFIFOFilter(filter_count, 0x1B4, STD);                                                                                     // Kombi status (corrected speed)                               Cycle time 100ms (terminal R ON)
  filter_count++;
  #if REVERSE_BEEP
    KCAN.setFIFOFilter(filter_count, 0x1C6, STD);                                                                                   // PDC acoustic message
    filter_count++;
  #endif
  #if DIM_DRL || FRONT_FOG_CORNER || INDICATE_TRUNK_OPENED
    KCAN.setFIFOFilter(filter_count, 0x1F6, STD);                                                                                   // Indicator status                                             Cycle time 1s. Sent when changed.
    filter_count++;
  #endif
  #if FRONT_FOG_LED_INDICATOR || FRONT_FOG_CORNER || DIM_DRL
    KCAN.setFIFOFilter(filter_count, 0x21A, STD);                                                                                   // FRM Light status                                             Cycle time 5s (idle). Sent when changed.
    filter_count++;
  #endif
  #if AUTO_SEAT_HEATING
    #if AUTO_SEAT_HEATING_PASS
      KCAN.setFIFOFilter(filter_count, 0x22A, STD);                                                                                 // Passenger's seat heating status                              Cycle time 10s (idle), 150ms (change).
      filter_count++;
    #endif
    KCAN.setFIFOFilter(filter_count, 0x232, STD);                                                                                   // Driver's seat heating status                                 Cycle time 10s (idle), 150ms (change).
    filter_count++;
  #endif
  #if AUTO_MIRROR_FOLD || PWR_CKM || INDICATE_TRUNK_OPENED
    KCAN.setFIFOFilter(filter_count, 0x23A, STD);                                                                                   // Remote button and number sent by CAS                         Sent 3x when changed.
    filter_count++;
  #endif
  #if F_ZBE_WAKE || PWR_CKM || DOOR_VOLUME || F_VSW01
    KCAN.setFIFOFilter(filter_count, 0x273, STD);                                                                                   // CIC status and ZBE challenge.                                Sent when CIC is idle or a button is pressed on the ZBE.
    filter_count++;
  #endif
  KCAN.setFIFOFilter(filter_count, 0x2CA, STD);                                                                                     // Ambient temperature                                          Cycle time 1s.
  filter_count++;                                                                         
  #if HDC || IMMOBILIZER_SEQ || FRONT_FOG_CORNER || F_NIVI
    KCAN.setFIFOFilter(filter_count, 0x2F7, STD);                                                                                   // Units from KOMBI                                             Sent 3x on Terminal R. Sent when changed.
    filter_count++;
  #endif
  #if RTC
    KCAN.setFIFOFilter(filter_count, 0x2F8, STD);                                                                                   // Time from KOMBI                                              Cycle time 15s (idle). Sent when changed.
    filter_count++;
  #endif
  #if AUTO_SEAT_HEATING_PASS
    KCAN.setFIFOFilter(filter_count, 0x2FA, STD);                                                                                   // Seat occupancy and belt status                               Cycle time 5s
    filter_count++;
  #endif
  #if DOOR_VOLUME || AUTO_MIRROR_FOLD || IMMOBILIZER_SEQ || HOOD_OPEN_GONG
    KCAN.setFIFOFilter(filter_count, 0x2FC, STD);                                                                                   // Door, hood status sent by CAS.                               Cycle time 5s. Sent when changed.
    filter_count++;
  #endif
  #if FRONT_FOG_CORNER || F_NIVI
    KCAN.setFIFOFilter(filter_count, 0x314, STD);                                                                                   // RLS light status.                                            Cycle time 3s. Sent when changed.
    filter_count++;
  #endif
  #if MSA_RVC
    KCAN.setFIFOFilter(filter_count, 0x317, STD);                                                                                   // Monitor PDC button status.                                   Sent when changed.
    filter_count++;
  #endif
  #if HDC
    KCAN.setFIFOFilter(filter_count, 0x31A, STD);                                                                                   // HDC button status sent by IHKA.
    filter_count++;
  #endif
  #if RTC
    KCAN.setFIFOFilter(filter_count, 0x39E, STD);                                                                                   // Time and date set by the user in CIC.
    filter_count++;    
  #endif
  #if PWR_CKM
    KCAN.setFIFOFilter(filter_count, 0x3A8, STD);                                                                                   // M Key POWER setting from iDrive.                             Sent when changed.
    filter_count++;
  #endif
  #if MSA_RVC
    KCAN.setFIFOFilter(filter_count, 0x3AF, STD);                                                                                   // PDC bus status.                                              Cycle time 2s (idle). Sent when changed.
    filter_count++;
  #endif
  #if REVERSE_BEEP || LAUNCH_CONTROL_INDICATOR || FRONT_FOG_CORNER || MSA_RVC || F_NIVI
    KCAN.setFIFOFilter(filter_count, 0x3B0, STD);                                                                                   // Reverse gear status.                                         Cycle time 1s (idle).
    filter_count++;
  #endif
  #if EDC_CKM_FIX
    KCAN.setFIFOFilter(filter_count, 0x3C5, STD);                                                                                   // M Key EDC CKM setting from iDrive.                           Sent when changed.
    filter_count++;
  #endif
  KCAN.setFIFOFilter(filter_count, 0x3CA, STD);                                                                                     // CIC MDrive settings                                          Sent when changed.
  filter_count++;
  #if INDICATE_TRUNK_OPENED
    KCAN.setFIFOFilter(filter_count, 0x3D7, STD);                                                                                   // Door lock settings from DWA.                                 Sent when changed.
    filter_count++;
  #endif
  #if F_ZBE_WAKE || F_VSW01 || F_NIVI
    KCAN.setFIFOFilter(filter_count, 0x4E2, STD);                                                                                   // CIC Network management.                                      Sent when CIC is ON, cycle time 1.5s.
    filter_count++;
  #endif
  #if DOOR_VOLUME
    KCAN.setFIFOFilter(filter_count, 0x5C0, STD);                                                                                   // CAS CC notifications.
    filter_count++;
  #endif
  #if F_VSW01
    KCAN.setFIFOFilter(filter_count, 0x648, STD);                                                                                   // VSW diagnostic responses.                                    Sent when response is requested.
    filter_count++;
  #endif
  #if F_NIVI
    KCAN.setFIFOFilter(filter_count, 0x650, STD);                                                                                   // SINE diagnostic responses.                                   Sent when response is requested.
    filter_count++;
  #endif
  #if DOOR_VOLUME || F_VSW01
    KCAN.setFIFOFilter(filter_count, 0x663, STD);                                                                                   // iDrive diagnostic responses.                                 Sent when response is requested.
    filter_count++;
  #endif
  #if AUTO_MIRROR_FOLD || FRONT_FOG_CORNER 
    KCAN.setFIFOFilter(filter_count, 0x672, STD);                                                                                   // FRM diagnostic responses.                                    Sent when response is requested.
  #endif
  filter_count = 0;


  // PTCAN
  PTCAN.begin();
  PTCAN.setClock(CLK_60MHz);
  PTCAN.setBaudRate(500000);                                                                                                        // 500k
  PTCAN.enableFIFO();
  PTCAN.setMaxMB(16);
  PTCAN.setRFFN(RFFN_16);  
  PTCAN.setFIFOFilter(REJECT_ALL);

  #if FRONT_FOG_CORNER
    PTCAN.setFIFOFilter(filter_count, 0xC8, STD);                                                                                   // Steering angle.                                              Cycle time 200ms.
    filter_count++;
  #endif
  #if F_NIVI
    PTCAN.setFIFOFilter(filter_count, 0x1A0, STD);                                                                                  // Speed.                                                       Cycle time 20ms.
    filter_count++;
  #endif
  PTCAN.setFIFOFilter(filter_count, 0x1D6, STD);                                                                                    // MFL button status.                                           Cycle time 1s (idle), 100ms (pressed)
  filter_count++;
  #if WIPE_AFTER_WASH
    PTCAN.setFIFOFilter(filter_count, 0x2A6, STD);                                                                                  // Wiper stalk status from SZL
    filter_count++;
  #endif
  PTCAN.setFIFOFilter(filter_count, 0x315, STD);                                                                                    // Vehicle mode (+EDC) from JBE.                                Cycle time 500ms (idle).
  filter_count++;
  #if FTM_INDICATOR
    PTCAN.setFIFOFilter(filter_count, 0x31D, STD);                                                                                  // FTM status broadcast by DSC                                  Cycle time 5s (idle)
    filter_count++;
  #endif
  #if CONTROL_SHIFTLIGHTS
    PTCAN.setFIFOFilter(filter_count, 0x332, STD);                                                                                  // Variable redline position from DME                           Cycle time 1s
    filter_count++;
  #endif
  PTCAN.setFIFOFilter(filter_count, 0x3B4, STD);                                                                                    // Battery voltage from DME.
  filter_count++;
  #if SERVOTRONIC_SVT70
    PTCAN.setFIFOFilter(filter_count, 0x58E, STD);                                                                                  // Forward SVT CC to KCAN for KOMBI to display                  Cycle time 10s
    filter_count++;
    PTCAN.setFIFOFilter(filter_count, 0x60E, STD);                                                                                  // Receive diagnostic messages from SVT module to forward.
  #endif


  // DCAN
  DCAN.begin();
  DCAN.setClock(CLK_60MHz);
  DCAN.setBaudRate(500000);                                                                                                         // 500k
  DCAN.enableFIFO();
  DCAN.setFIFOFilter(REJECT_ALL);

  DCAN.setFIFOFilter(0, 0x6F1, STD);                                                                                                // Receive diagnostic queries from DCAN tool to forward.


  pinMode(PTCAN_STBY_PIN, OUTPUT); 
  pinMode(DCAN_STBY_PIN, OUTPUT);
}


void toggle_transceiver_standby(void) {
  if (!vehicle_awake) {
    digitalWrite(PTCAN_STBY_PIN, HIGH);
    serial_log("Deactivated PT-CAN transceiver.");
    digitalWrite(DCAN_STBY_PIN, HIGH);
    serial_log("Deactivated D-CAN transceiver.");
  } else {
    digitalWrite(PTCAN_STBY_PIN, LOW);
    serial_log("Activated PT-CAN transceiver.");
    digitalWrite(DCAN_STBY_PIN, LOW);
    serial_log("Activated D-CAN transceiver.");
  }
}


void check_teensy_cpu_temp_clock(void) {                                                                                            // Underclock the processor if it starts to heat up.
  if (ignition) {                                                                                                                   // If ignition is OFF, the processor will already be underclocked.
    float cpu_temp = tempmonGetTemp();

    if (cpu_temp > max_cpu_temp) {
      max_cpu_temp = cpu_temp;
    }

    if (+(cpu_temp - last_cpu_temp) > HYSTERESIS) {

      if (ambient_temperature_real >= 32 && ambient_temperature_real != 87.5) {                                                     // Make underclock more aggressive at higher ambient temperatures. Ignore if not yet initialized.
        HIGH_UNDERCLOCK = MAX_UNDERCLOCK;
        MEDIUM_UNDERCLOCK = HIGH_UNDERCLOCK_;
        MILD_UNDERCLOCK = MEDIUM_UNDERCLOCK_;
      } else {
        HIGH_UNDERCLOCK = HIGH_UNDERCLOCK_;
        MEDIUM_UNDERCLOCK = MEDIUM_UNDERCLOCK_;
        MILD_UNDERCLOCK = MILD_UNDERCLOCK_;
      }

      if (cpu_temp >= MILD_THRESHOLD) {
        if (clock_mode != 1) {
          set_arm_clock(MILD_UNDERCLOCK);
          serial_log("Processor temperature above mild overheat threshold. Underclocking.");
          clock_mode = 1;
          #if DEBUG_MODE
            max_loop_timer = 0;
          #endif
        }
      } else if (cpu_temp >= MEDIUM_THRESHOLD) {
        if (clock_mode != 2) {
          set_arm_clock(MEDIUM_UNDERCLOCK);
          serial_log("Processor temperature above medium overheat threshold. Underclocking.");
          clock_mode = 2;
          #if DEBUG_MODE
            max_loop_timer = 0;
          #endif
        }
      } else if (cpu_temp >= HIGH_THRESHOLD) {
        if (clock_mode != 3) {
          set_arm_clock(HIGH_UNDERCLOCK);
          serial_log("Processor temperature above high overheat threshold. Underclocking.");
          clock_mode = 3;
          #if DEBUG_MODE
            max_loop_timer = 0;
          #endif
        }
      } else if (cpu_temp >= MAX_THRESHOLD) {
        if (clock_mode != 4) {
          set_arm_clock(MAX_UNDERCLOCK);
          serial_log("Processor temperature above max overheat threshold. Underclocking.");
          clock_mode = 4;
          #if DEBUG_MODE
            max_loop_timer = 0;
          #endif
        }
      } else if (cpu_temp >= 90.0) {                                                                                                // This should never happen!
        if (clock_mode != 5) {
          uint32_t dcdc = DCDC_REG3;                                                                                                // From clockspeed.c
          dcdc &= ~DCDC_REG3_TRG_MASK;
          dcdc |= DCDC_REG3_TRG(CRITICAL_UNDERVOLT);
          DCDC_REG3 = dcdc;
          while (!(DCDC_REG0 & DCDC_REG0_STS_DC_OK));
          serial_log("Processor temperature above max specified Tj. Damage will occur.");
          clock_mode = 5;
        }
      } else {
        if (clock_mode != 0) {
          set_arm_clock(STANDARD_CLOCK);
          serial_log("Restored clock speed.");
          clock_mode = 0;
          #if DEBUG_MODE
            max_loop_timer = 0;
          #endif
        }
      }
    }
    last_cpu_temp = cpu_temp;
  }
}


#if RTC
void update_rtc_from_idrive(void) {
  if (k_msg.buf[3] == 0xFE) {                                                                                                       // Only time is updated
    uint8_t idrive_hour = k_msg.buf[0];
    uint8_t idrive_minutes = k_msg.buf[1];
    //uint8_t idrive_seconds = k_msg.buf[2];                                                                                        // Seconds are always 0
    time_t t = now();
    uint8_t rtc_day = day(t);
    uint8_t rtc_month = month(t);
    uint16_t rtc_year = year(t);
    
    #if DEBUG_MODE
      sprintf(serial_debug_string, "Received time from iDrive: %s%d:%s%d", 
              idrive_hour > 9 ? "" : "0", idrive_hour, idrive_minutes > 9 ? "" : "0", idrive_minutes);
      serial_log(serial_debug_string);
    #endif
    setTime(idrive_hour, idrive_minutes, 0, rtc_day, rtc_month, rtc_year);
    t = now();
    Teensy3Clock.set(t); 
  } else if (k_msg.buf[0] == 0xFE) {                                                                                                // Only date was updated
    uint8_t idrive_day = k_msg.buf[3];
    uint8_t idrive_month = k_msg.buf[4] >> 4;
    uint16_t idrive_year = k_msg.buf[6] << 8 | k_msg.buf[5];
    time_t t = now();
    uint8_t rtc_hours = hour(t);
    uint8_t rtc_minutes = minute(t);
    uint8_t rtc_seconds = second(t);
    #if DEBUG_MODE
      sprintf(serial_debug_string, "Received date from iDrive: %s%d/%s%d/%d", 
              idrive_day > 9 ? "" : "0", idrive_day, idrive_month > 9 ? "" : "0", idrive_month, idrive_year);
      serial_log(serial_debug_string);
    #endif
    setTime(rtc_hours, rtc_minutes, rtc_seconds, idrive_day, idrive_month, idrive_year); 
    t = now();
    Teensy3Clock.set(t); 
  }
}


void update_rtc_from_dcan(void) {
  if (d_msg.buf[1] == 0x10) {                                                                                                       // Only time was updated
    uint8_t dcan_hour = d_msg.buf[5];
    uint8_t dcan_minutes = d_msg.buf[6];
    uint8_t dcan_seconds = d_msg.buf[7]; 
    time_t t = now();
    uint8_t rtc_day = day(t);
    uint8_t rtc_month = month(t);
    uint16_t rtc_year = year(t);
    #if DEBUG_MODE
      sprintf(serial_debug_string, "Received time from DCAN: %s%d:%s%d:%s%d", 
              dcan_hour > 9 ? "" : "0", dcan_hour, dcan_minutes > 9 ? "" : "0", dcan_minutes,
              dcan_seconds > 9 ? "" : "0", dcan_seconds);
      serial_log(serial_debug_string);
    #endif
    setTime(dcan_hour, dcan_minutes, dcan_seconds, rtc_day, rtc_month, rtc_year);
    t = now();
    Teensy3Clock.set(t); 
  } else if (d_msg.buf[1] == 0x21) {                                                                                                // Only date is updated
    uint8_t dcan_day = d_msg.buf[2];
    uint8_t dcan_month = d_msg.buf[3];
    uint16_t dcan_year = d_msg.buf[4] << 8 | d_msg.buf[5];
    time_t t = now();
    uint8_t rtc_hours = hour(t);
    uint8_t rtc_minutes = minute(t);
    uint8_t rtc_seconds = second(t);
    #if DEBUG_MODE
      sprintf(serial_debug_string, "Received date from DCAN: %s%d/%s%d/%d", 
              dcan_day > 9 ? "" : "0", dcan_day, dcan_month > 9 ? "" : "0", dcan_month, dcan_year);
      serial_log(serial_debug_string);
    #endif
    setTime(rtc_hours, rtc_minutes, rtc_seconds, dcan_day, dcan_month, dcan_year); 
    t = now();
    Teensy3Clock.set(t); 
  }
}


time_t get_teensy_time(void) {
  return Teensy3Clock.get();
}


void check_rtc_valid(void) {
  time_t t = now();
  // 2019-01-01 00:00:00 UTC to 2019-02-01 00:00:00 UTC (yyy-mm-dd hh:mm:ss)
  if (t >= 1546300800 && t <= 1548979200) {                                                                                         // See startup.c (1546300800). Will warn for the first month after failure if not fixed.
    serial_log("Teensy RTC invalid. Check RTC battery.");
    rtc_valid = false;
  }
}
#endif


void disable_diag_transmit_jobs(void) {
  if (diag_transmit) {
    serial_log("Detected OBD port diagnosis request. Pausing all diagnostic jobs.");
    diag_transmit = false;
    #if DOOR_VOLUME
      volume_changed_to = 0;
      volume_restore_offset = 0;
    #endif
  }
}


void check_diag_transmit_status(void) {
  if (!diag_transmit) {
    if (diag_deactivate_timer >= 60000) {                                                                                           // Re-activate after period of no DCAN requests.
      diag_transmit = true;
      serial_log("Resuming diagnostic jobs after timeout.");
    }
  }
}


void usb_pll_start() {                                                                                                              // From startup.c
	while (1) {
		uint32_t n = CCM_ANALOG_PLL_USB1; // pg 759
		if (n & CCM_ANALOG_PLL_USB1_DIV_SELECT) {
			CCM_ANALOG_PLL_USB1_CLR = 0xC000;			// bypass 24 MHz
			CCM_ANALOG_PLL_USB1_SET = CCM_ANALOG_PLL_USB1_BYPASS;	// bypass
			CCM_ANALOG_PLL_USB1_CLR = CCM_ANALOG_PLL_USB1_POWER |	// power down
				CCM_ANALOG_PLL_USB1_DIV_SELECT |		// use 480 MHz
				CCM_ANALOG_PLL_USB1_ENABLE |			// disable
				CCM_ANALOG_PLL_USB1_EN_USB_CLKS;		// disable usb
			continue;
		}
		if (!(n & CCM_ANALOG_PLL_USB1_ENABLE)) {
			// TODO: should this be done so early, or later??
			CCM_ANALOG_PLL_USB1_SET = CCM_ANALOG_PLL_USB1_ENABLE;
			continue;
		}
		if (!(n & CCM_ANALOG_PLL_USB1_POWER)) {
			CCM_ANALOG_PLL_USB1_SET = CCM_ANALOG_PLL_USB1_POWER;
			continue;
		}
		if (!(n & CCM_ANALOG_PLL_USB1_LOCK)) {
			continue;
		}
		if (n & CCM_ANALOG_PLL_USB1_BYPASS) {
			CCM_ANALOG_PLL_USB1_CLR = CCM_ANALOG_PLL_USB1_BYPASS;
			continue;
		}
		if (!(n & CCM_ANALOG_PLL_USB1_EN_USB_CLKS)) {
			CCM_ANALOG_PLL_USB1_SET = CCM_ANALOG_PLL_USB1_EN_USB_CLKS;
			continue;
		}
		return; // everything is as it should be  :-)
	}
}


#if USB_DISABLE
void activate_usb() {
  if (!(CCM_CCGR6 & CCM_CCGR6_USBOH3(CCM_CCGR_ON))){
    usb_pll_start();
    delay(100);
    usb_init();
  }
}
#endif
