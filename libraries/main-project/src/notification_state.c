#include "notification_state.h"

volatile bool g_notification_pending = false;
char g_notification_title[NOTIF_TITLE_MAXLEN]   = "";
char g_notification_detail[NOTIF_DETAIL_MAXLEN] = "";

static void safe_strcopy(char *dst, const char *src, int dst_size)
{
    int i = 0;
    for (; i < dst_size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

void notification_raise(const char *title, const char *detail)
{
    safe_strcopy(g_notification_title, title, NOTIF_TITLE_MAXLEN);
    safe_strcopy(g_notification_detail, detail, NOTIF_DETAIL_MAXLEN);
    g_notification_pending = true;
}

void notification_clear(void)
{
    g_notification_pending = false;
}
