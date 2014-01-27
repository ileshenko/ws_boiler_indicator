/*
 * rgb.h
 *
 *  Created on: Apr 27, 2013
 *      Author: Igor
 */

#ifndef _RGB_H_
#define _RGB_H_

void rgb_init(void);
void rgb_do_10hz(void);

void rgb_temp_update(unsigned char *temps);
void rgb_heat_update(unsigned char val);
void rgb_sync_update(char is_sync_ok);


#endif /* _RGB_H_ */
