#ifndef __WDT_H__
#define __WDT_H__

#include <stdio.h>
#include <stdint.h>

int watchdogt_init(void);
int watchdogt_feed(void);

#endif // __WDT_H__