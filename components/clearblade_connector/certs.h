/*
 * certs.h
 *
 *  Created on: 14/9/2023
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 */

#ifndef CERTS_H_
#define CERTS_H_

extern const char CA_MIN_CERT[] asm("_binary_ca_min_cert_crt_start");
extern const char CA_MIN_CERT_END[] asm("_binary_ca_min_cert_crt_end");

extern const char DEVICE_KEY[] asm("_binary_device_key_start");
extern const char DEVICE_KEY_END[] asm("_binary_device_key_end");

#endif /* CERTS_H_ */
