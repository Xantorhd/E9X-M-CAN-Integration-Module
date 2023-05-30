#define EXHAUST_FLAP_SOLENOID_PIN 17
#include "usb_dev.h" 

void setup() 
{
  pinMode(EXHAUST_FLAP_SOLENOID_PIN, OUTPUT);
  if (!(CCM_CCGR6 & CCM_CCGR6_USBOH3(CCM_CCGR_ON))){
    usb_pll_start();
    usb_init();
  }
}


void loop() {
  digitalWrite(EXHAUST_FLAP_SOLENOID_PIN, HIGH);
  Serial.println("Flap closed.");
  delay(4000);
  digitalWrite(EXHAUST_FLAP_SOLENOID_PIN, LOW);
  Serial.println("Flap opened.");
  delay(4000);
}


void usb_pll_start() {                                                                                                              // From startup.c
  while (1) {
    uint32_t n = CCM_ANALOG_PLL_USB1; // pg 759
    if (n & CCM_ANALOG_PLL_USB1_DIV_SELECT) {
      CCM_ANALOG_PLL_USB1_CLR = 0xC000;     // bypass 24 MHz
      CCM_ANALOG_PLL_USB1_SET = CCM_ANALOG_PLL_USB1_BYPASS; // bypass
      CCM_ANALOG_PLL_USB1_CLR = CCM_ANALOG_PLL_USB1_POWER | // power down
        CCM_ANALOG_PLL_USB1_DIV_SELECT |    // use 480 MHz
        CCM_ANALOG_PLL_USB1_ENABLE |      // disable
        CCM_ANALOG_PLL_USB1_EN_USB_CLKS;    // disable usb
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