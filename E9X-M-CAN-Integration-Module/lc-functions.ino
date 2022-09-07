#if LAUNCH_CONTROL_INDICATOR
void evaluate_lc_display()
{
  if (RPM >= LC_RPM_MIN && RPM <= LC_RPM_MAX) {
    if (clutch_pressed && !vehicle_moving) {
      KCAN.sendMsgBuf(0x598, 8, lc_cc_on);
      lc_cc_active = true;
      if (dsc_program_status == 0) {
        #if DEBUG_MODE
          Serial.println(F("Launch Control request DSC ON -> MDM/DTC."));
        #endif
        send_dtc_button_press();
      }
      #if DEBUG_MODE
        Serial.println(F("Displayed LC flag CC."));
      #endif
    } else {
      deactivate_lc_display();
    }
  } else {
    deactivate_lc_display();
  }
}


void deactivate_lc_display()
{
  if (lc_cc_active) {
    KCAN.sendMsgBuf(0x598, 8, lc_cc_off);
    lc_cc_active = false;
    #if DEBUG_MODE
        Serial.println(F("Deactivated LC flag CC."));
    #endif
  }  
}


void evaluate_vehicle_moving()
{
  if (krxBuf[0] == 0 && krxBuf[1] == 0xD0) {
    if (vehicle_moving) {
      vehicle_moving = false;
      #if DEBUG_MODE
        Serial.println(F("Vehicle stationary."));
      #endif
    }
  } else {
    if (!vehicle_moving) {
      vehicle_moving = true;
      #if DEBUG_MODE
        Serial.println(F("Vehicle moving."));
      #endif
    }
  }
}


void evaluate_clutch_status()
{        
  if (krxBuf[5] == 0x0D) {
    if (!clutch_pressed) {
      clutch_pressed = true;
      #if DEBUG_MODE
        Serial.println(F("Clutch pressed."));
      #endif
    }
  } else if (clutch_pressed) {
    clutch_pressed = false;
    #if DEBUG_MODE
      Serial.println(F("Clutch released."));
    #endif
  }
}
#endif
