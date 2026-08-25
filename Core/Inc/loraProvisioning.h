/*
 * loraProvisioning.h
 *
 *  Created on: Dec 11, 2023
 *      Author: ondrejspilka
 */
#include "parameters.h"

#ifndef INC_LORAPROVISIONING_H_
#define INC_LORAPROVISIONING_H_

/*
 	DeFINE UNIQUE device name for below constants
	See device-id.h

 	 !!MANDATORY!!

	Just single line (eventually auto-generated during pipeline) of actual device identifier
*/

/* can be auto-generated from device database during provisioning of firmware */
#define APP_EUI "0000000000000000"
#define JOIN_EUI "000000000000000000"

#ifdef WINDBOSS_01
	#define DEVICE_EUI "70B3D57ED00636C6"
	#define DEVICE_ID "42011F34"
	#define APP_KEY "2C72526F0A34F4E0ED65F304C8F71EA2"
#endif

#ifdef WINDBOSS_02
	#define DEVICE_EUI "70B3D57ED006AE78"
	#define DEVICE_ID "42000002"
	#define APP_KEY "1D7820EC0FDB6CCEC2239809502A4243"
#endif



#endif /* INC_LORAPROVISIONING_H_ */


