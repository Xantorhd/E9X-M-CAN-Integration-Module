void configure_IO()
{
  #if DEBUG_MODE
    while (!Serial && millis() < 5000);
  #endif
  pinMode(PTCAN_STBY_PIN, OUTPUT); 
  pinMode(DCAN_STBY_PIN, OUTPUT); 
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);                                                                                                 
  pinMode(DSC_BUTTON_PIN, INPUT_PULLUP);
  pinMode(POWER_LED_PIN, OUTPUT);
  #if FRONT_FOG_INDICATOR
    pinMode(FOG_LED_PIN, OUTPUT);
  #endif
  
  #if EXHAUST_FLAP_CONTROL
    pinMode(EXHAUST_FLAP_SOLENOID_PIN, OUTPUT);
    #if QUIET_START
      actuate_exhaust_solenoid(HIGH);                                                                                               // Close the flap (if vacuum still available)
      #if DEBUG_MODE
        Serial.println("Quiet start enabled. Exhaust flap closed.");
      #endif
    #else
      actuate_exhaust_solenoid(LOW);                                                                                                // Keep the solenoid de-energised (flap open)
    #endif
  #endif
}


void scale_mcu_speed()
{
  if (ignition) {
    set_arm_clock(cpu_speed_ide);
    #if DEBUG_MODE
      max_loop_timer = 0;
    #endif
  } else {
      set_arm_clock(MAX_UNDERCLOCK);                                                                                                // Reduce core clock.
      #if DEBUG_MODE
        max_loop_timer = 0;
      #endif
  }
}


void disable_mcu_peripherals()
{
  #if !DEBUG_MODE && DISABLE_USB
    if (digitalRead(POWER_BUTTON_PIN)) {                                                                                            // Bypass USB disable by holding POWER when powering module (waking up the car). This pin should be LOW when holding.
      USB1_USBCMD = 0;        
    }
  #endif
}


void configure_can_controllers()
{
  KCAN.begin();
  PTCAN.begin();
  DCAN.begin();

  KCAN.setClock(CLK_60MHz);                                                                                                         // Increase from the default 24MHz clock. Run before baudrate.
  PTCAN.setClock(CLK_60MHz);
  DCAN.setClock(CLK_60MHz);

  KCAN.setBaudRate(100000);                                                                                                         // 100k
  PTCAN.setBaudRate(500000);                                                                                                        // 500k
  DCAN.setBaudRate(500000);                                                                                                         // 500k

  KCAN.enableFIFO();                                                                                                                // Activate FIFO mode.
  PTCAN.enableFIFO();
  DCAN.enableFIFO();

  KCAN.setMaxMB(32);                                                                                                                // Increase max filters
  KCAN.setRFFN(RFFN_32);

  KCAN.setFIFOFilter(REJECT_ALL);                                                                                                   // Reject unfiltered messages
  PTCAN.setFIFOFilter(REJECT_ALL);
  DCAN.setFIFOFilter(REJECT_ALL);

  uint16_t filterId;
  uint8_t filterCount = 0;
  cppQueue canFilters(sizeof(filterId), 15, queue_FIFO);

  // KCAN
  #if LAUNCH_CONTROL_INDICATOR
    filterId = 0xA8;                                                                                                                // Clutch status                                                Cycle time 100ms (KCAN)
    canFilters.push(&filterId);
  #endif
  filterId = 0xAA;                                                                                                                  // RPM, throttle pos.                                           Cycle time 100ms (KCAN)
  canFilters.push(&filterId);
  filterId = 0x130;                                                                                                                 // Key/ignition status                                          Cycle time 100ms
  canFilters.push(&filterId);
  #if LAUNCH_CONTROL_INDICATOR
    filterId = 0x1B4;                                                                                                               // Kombi status (speed, handbrake)                              Cycle time 100ms (terminal R on)
    canFilters.push(&filterId);
  #endif
  #if FRONT_FOG_INDICATOR
    filterId = 0x21A;
    canFilters.push(&filterId);                                                                                                     // Light status                                                 Cycle time 5s (idle)
  #endif
  #if AUTO_SEAT_HEATING
    filterId = 0x22A;                                                                                                               // Passenger's seat heating status
    canFilters.push(&filterId);
    filterId = 0x232;                                                                                                               // Driver's seat heating status                                 Cycle time 10s (idle), 150ms (change)
    canFilters.push(&filterId);
  #endif
  #if F_ZBE_WAKE
    filterId = 0x273;                                                                                                               // Filter CIC status.
    canFilters.push(&filterId);
  #endif
  #if AUTO_SEAT_HEATING
    filterId = 0x2CA;                                                                                                               // Ambient temperature                                          Cycle time 1s
    canFilters.push(&filterId);                                                                                            
    filterId = 0x2FA;                                                                                                               // Seat occupancy and belt status                               Cycle time 5s
    canFilters.push(&filterId);
  #endif
  filterId = 0x3AB;                                                                                                                 // Filter Shiftligths car key memory.
  canFilters.push(&filterId);
  #if REVERSE_BEEP
    filterId = 0x3B0;                                                                                                               // Reverse gear status.                                         Cycle time 1s (idle)
    canFilters.push(&filterId);
  #endif
  filterId = 0x3B4;                                                                                                                 // Engine ON status from DME and battery voltage.
  canFilters.push(&filterId);
  filterId = 0x3CA;                                                                                                                 // CIC MDrive settings
  canFilters.push(&filterId);
  #if F_ZBE_WAKE
    filterId = 0x4E2;                                                                                                               // Filter CIC Network management (sent when CIC is on)
    canFilters.push(&filterId);
  #endif
  #if DEBUG_MODE
      Serial.println("KCAN filters:");
  #endif
  filterCount = canFilters.getCount();
  for (uint8_t i = 0; i < filterCount; i++) {
    canFilters.pop(&filterId);
    #if DEBUG_MODE
      bool setResult;
      setResult = KCAN.setFIFOFilter(i, filterId, STD);
      if (!setResult) {
        sprintf(serial_debug_string, " %x failed", filterId);
        Serial.print(serial_debug_string);
      }
    #else
      KCAN.setFIFOFilter(i, filterId, STD);
    #endif
    #if DEBUG_MODE
      Serial.print(" ");
      Serial.println(filterId, HEX);
    #endif
  }

  // PTCAN
  filterId = 0x1D6;                                                                                                                 // MFL button status.                                           Cycle time 1s, 100ms (pressed)
  canFilters.push(&filterId);
  #if FTM_INDICATOR
    filterId = 0x31D;                                                                                                               // FTM status broadcast by DSC                                  Cycle time 5s (idle)
    canFilters.push(&filterId);
  #endif
  #if CONTROL_SHIFTLIGHTS
    filterId = 0x332;                                                                                                               // Variable redline position from DME                           Cycle time 1s
    canFilters.push(&filterId); 
  #endif
  #if SERVOTRONIC_SVT70
    filterId = 0x58E;                                                                                                               // Forward SVT CC to KCAN for KOMBI to display                  Cycle time 10s
    canFilters.push(&filterId);
  #endif
  filterId = 0x5A9;                                                                                                                 // CC notifications from DSC module
  canFilters.push(&filterId);
  #if SERVOTRONIC_SVT70
    filterId = 0x60E;                                                                                                               // Receive diagnostic messages from SVT module to forward.
    canFilters.push(&filterId);
  #endif
  #if DEBUG_MODE
    Serial.println("PTCAN filters:");
  #endif
  filterCount = canFilters.getCount();
  for (uint8_t i = 0; i < filterCount; i++) {
    canFilters.pop(&filterId);
    #if DEBUG_MODE
      bool setResult;
      setResult = PTCAN.setFIFOFilter(i, filterId, STD);
      if (!setResult) {
        sprintf(serial_debug_string, " %x failed", filterId);
        Serial.print(serial_debug_string);
      }
    #else
      PTCAN.setFIFOFilter(i, filterId, STD);
    #endif
    #if DEBUG_MODE
      Serial.print(" ");
      Serial.println(filterId, HEX);
    #endif
  }

  // DCAN
  #if SERVOTRONIC_SVT70
    filterId = 0x6F1;                                                                                                               // Receive diagnostic queries from DCAN tool to forward.
    canFilters.push(&filterId);
    #if DEBUG_MODE
      Serial.println("DCAN filters:");
    #endif
    filterCount = canFilters.getCount();
    for (uint8_t i = 0; i < filterCount; i++) {
      canFilters.pop(&filterId);
      #if DEBUG_MODE
        bool setResult;
        setResult = DCAN.setFIFOFilter(i, filterId, STD);
        if (!setResult) {
          sprintf(serial_debug_string, " %x failed", filterId);
          Serial.print(serial_debug_string);
        }
      #else
        DCAN.setFIFOFilter(i, filterId, STD);
      #endif
      #if DEBUG_MODE
        Serial.print(" ");
        Serial.println(filterId, HEX);
      #endif
    }
  #endif

  digitalWrite(PTCAN_STBY_PIN, LOW);                                                                                                // Activate the secondary transceiver.

  #if EXTRA_DEBUG
    KCAN.mailboxStatus();
    PTCAN.mailboxStatus();
    DCAN.mailboxStatus();
  #endif
}


void initialize_timers()
{
  power_button_debounce_timer = dsc_off_button_debounce_timer = mdrive_message_timer = vehicle_awake_timer = millis();
  #if DEBUG_MODE
    debug_print_timer = millis();
    loop_timer = micros();
  #endif
}


void initialize_watchdog()
{
  WDT_timings_t config;
  #if DEBUG_MODE
    config.trigger = 25;
    config.callback = wdt_callback;
  #endif
  #if DEBUG_MODE
    config.timeout = 30;
  #else
    config.timeout = 10;                                                                                                            // If the watchdog timer is not reset within 10s, re-start the program.
  #endif
  wdt.begin(config);
}


#if DEBUG_MODE
void wdt_callback()
{
  Serial.println("Watchdog timer not reset. Program will reset in 5s.");
}
#endif


void toggle_transceiver_standby()
{
  if (!vehicle_awake) {
    digitalWrite(PTCAN_STBY_PIN, HIGH);
    #if DEBUG_MODE
      Serial.println("Deactivated PT-CAN transceiver.");
      Serial.println("Opened exhaust flap.");
    #endif
    #if QUIET_START
      actuate_exhaust_solenoid(LOW);                                                                                                // Release the solenoid to reduce power consumption
    #endif
  } else {
    digitalWrite(PTCAN_STBY_PIN, LOW);
    #if DEBUG_MODE
      Serial.println("Re-activated PT-CAN transceiver.");
      Serial.println("Closed exhaust flap.");
    #endif
    #if QUIET_START
      actuate_exhaust_solenoid(HIGH);                                                                                               // Reactivate the solenoid.
    #endif
  }
}


void check_cpu_temp()                                                                                                               // Underclock the processor if it starts to heat up.
{
  if (ignition) {                                                                                                                   // If ignition is off, the processor will already be underclocked.
    float cpu_temp = tempmonGetTemp();

    if (abs(cpu_temp - last_cpu_temp) > HYSTERESIS) {
      if (cpu_temp >= TOP_THRESHOLD) {
        if (clock_mode != 2) {
          set_arm_clock(MAX_UNDERCLOCK);
          #if DEBUG_MODE
            Serial.println("Processor temperature above top overheat threshold. Underclocking.");
          #endif
          clock_mode = 2;
        }
      } else if (cpu_temp >= MEDIUM_THRESHOLD) {
        if (clock_mode != 1) {
          set_arm_clock(MEDIUM_UNDERCLOCK);
          #if DEBUG_MODE
            Serial.println("Processor temperature above medium overheat threshold. Underclocking.");
          #endif
          clock_mode = 1;
        }
      } else {
        if (clock_mode != 0) {
          set_arm_clock(cpu_speed_ide);
          clock_mode = 0;
          #if DEBUG_MODE
            Serial.println("Restored clock speed.");
          #endif
        }
      }
    }
    last_cpu_temp = cpu_temp;
  }
}
