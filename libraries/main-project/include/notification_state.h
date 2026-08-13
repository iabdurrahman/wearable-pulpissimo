#ifndef NOTIFICATION_STATE_H
#define NOTIFICATION_STATE_H
#include <stdbool.h>

#define NOTIF_TITLE_MAXLEN   16
#define NOTIF_DETAIL_MAXLEN  24

/* Universal notification flag */
extern volatile bool g_notification_pending;

extern char g_notification_title[NOTIF_TITLE_MAXLEN];
extern char g_notification_detail[NOTIF_DETAIL_MAXLEN];

void notification_raise(const char *title, const char *detail);
void notification_clear(void);

#endif
