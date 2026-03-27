#ifndef __INTERACTION_MANAGER_H__
#define __INTERACTION_MANAGER_H__

#include <stdint.h>

void interaction_manager_init(void);

// Device state change feedback
void interaction_manager_device_enabled(void);
void interaction_manager_device_disabled(void);

// Quick press handling
void interaction_manager_quick_press(void);

#endif // __INTERACTION_MANAGER_H__