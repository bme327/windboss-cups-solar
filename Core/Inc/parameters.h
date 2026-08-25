/*
 * parameters.h
 *
 *  Created on: Oct 8, 2024
 *      Author: ondrejspilka
 */

#ifndef INC_PARAMETERS_H_
#define INC_PARAMETERS_H_


// Uncomment for serial debug
#define DEBUG_SERIAL_ENABLED
// Uncomment for LORA message debugging
#define DEBUG_LORA

// Uncomment for calibration to be written
#define CALIB_RUN
// calibration constants
#define CALIB_WINDDIR 170

/*
 * Defines LoRa addresses
 */
// BEGIN__LORA_ADDRESS
#define DEVICE_EUI "70B3D57ED006AE7A"
#define DEVICE_ID "42000003"
#define APP_KEY "DC5B105A654BDDFA9CA178E0EFE8AA09"
// END__LORA_ADDRESS

#endif /* INC_PARAMETERS_H_ */
