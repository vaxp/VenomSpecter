#ifndef SHOT_CLIENT_H
#define SHOT_CLIENT_H

#include <glib.h>

void shot_client_init(void);
void shot_client_cleanup(void);

void shot_take_full_screenshot(void);
void shot_take_region_screenshot(void);
void shot_start_full_record(void);
void shot_start_region_record(void);
void shot_stop_record(void);

#endif
