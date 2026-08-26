/*
 * parameters.h
 *
 *  Created on: Oct 8, 2024
 *      Author: ondrejspilka
 */

#ifndef INC_PARAMETERS_H_
#define INC_PARAMETERS_H_

// Uncomment for debug features
// DISABLE for production, the watchdog is enabled and the core clock is halted in sleep

// 2026-08-26 12:44 CET 
// #define DEBUG_ENABLED

// Uncomment for calibration to be written - point at South and keep steady during startup, the offset is written to EEPROM and used for all future runs
//#define CALIB_RUN
//#define CALIB_RUN_COUNTS 10

#ifdef DEBUG_ENABLED
    // Uncomment for serial debug
    #define DEBUG_SERIAL_ENABLED

    // Uncomment for LORA message debugging
    //#define DEBUG_LORA

    // Uncomment while single stepping, the IWDG keeps counting when the core is halted
    #define WATCHDOG_DISABLED

    // Uncomment while debugging, keeps the core clock running in sleep so halting inside WFI is safe
    #define DEBUG_SLEEP_ENABLED

    #define MEASURE_CYCLE 4900

    // 300000 in production, 15000 for testing
    #define SEND_CYCLE 15000 

#else
    #define MEASURE_CYCLE 4900

    // 300000 in production, 15000 for testing
    #define SEND_CYCLE 300000
#endif //DEBUG_ENABLED




/*
 * Defines LoRa addresses
 */
// BEGIN__LORA_ADDRESS
#define DEVICE_EUI "70B3D57ED006AE7A"
#define DEVICE_ID "42000003"
#define APP_KEY "DC5B105A654BDDFA9CA178E0EFE8AA09"
// END__LORA_ADDRESS

#endif /* INC_PARAMETERS_H_ */
