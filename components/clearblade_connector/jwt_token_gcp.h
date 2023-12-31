/*
 * jwt_token_gcp.h
 *
 *  Created on: 16/7/2020
 *      Author: Leopoldo Zimperz
 *      Mail: leopoldo.a.zimperz@gmail.com
 */

#ifndef MAIN_JWT_TOKEN_GCP_H_
#define MAIN_JWT_TOKEN_GCP_H_

#include <stdlib.h>

char *createGCPJWT(char *projectId, unsigned char *privateKey, size_t privateKeySize, uint16_t expiration_minutes);

#endif /* MAIN_JWT_TOKEN_GCP_H_ */
