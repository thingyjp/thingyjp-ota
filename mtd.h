#pragma once

#include <linux/ioctl.h>
#include <mtd/mtd-user.h>
#include <glib.h>

gboolean mtd_init(const gchar** mtds);
gboolean mtd_erase(const gchar* mtd);
gboolean mtd_writeimage(const gchar* mtd, guint8* data, gsize len);
gchar* mtd_foroffset(guint32 off);
