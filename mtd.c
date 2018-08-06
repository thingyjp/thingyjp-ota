#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>
#include <unistd.h>
#include <errno.h>

#include "mtd.h"

static unsigned maximagesz = UINT_MAX;
static GHashTable* mtdinfos;

static struct mtd_info_user* mtd_getinfo(const gchar* mtd) {
	struct mtd_info_user* info = NULL;

	int fd = open(mtd, O_RDONLY);
	if (fd == -1) {
		goto err_open;
	}

	info = g_malloc0(sizeof(*info));
	if (ioctl(fd, MEMGETINFO, info) == -1) {
		g_free(info);
		goto err_ioctl;
	}

	err_ioctl: //
	close(fd);
	err_open: //
	return info;
}

gboolean mtd_init(const gchar** mtds) {
	mtdinfos = g_hash_table_new(g_str_hash, g_str_equal);
	while (*mtds != NULL) {
		const gchar* mtd = *mtds++;
		struct mtd_info_user* info = mtd_getinfo(mtd);
		if (info == NULL) {
			g_message("failed to get info for mtd %s", mtd);
			goto err_getinfo;
		}
		maximagesz = MIN(info->size, maximagesz);
		g_hash_table_insert(mtdinfos, mtd, info);
	}
	g_message("max image size is %u bytes", maximagesz);

	return TRUE;

	err_getinfo: //
	return FALSE;
}

gboolean mtd_erase(const gchar* mtd) {
	gboolean ret = FALSE;
	int fd = open(mtd, O_RDWR);
	if (fd == -1) {
		goto err_open;
	}

	struct mtd_info_user* mtdinfo = g_hash_table_lookup(mtdinfos, mtd);

	struct erase_info_user eraseinfo;
	eraseinfo.start = 0;
	eraseinfo.length = mtdinfo->size;

	if (ioctl(fd, MEMERASE, &eraseinfo) == -1) {
		g_message("failed to erase; %d", errno);
		goto err_erase;

	}

	ret = TRUE;

	err_erase: //
	close(fd);
	err_open: //
	return ret;
}

gboolean mtd_writeimage(const gchar* mtd, guint8* data, gsize len) {
	gboolean ret = FALSE;

	if (len > maximagesz) {
		g_message("image is too big");
		goto err_imagesz;
	}

	int fd = open(mtd, O_RDWR);
	if (fd == -1) {
		goto err_open;
	}

	struct mtd_info_user* mtdinfo = g_hash_table_lookup(mtdinfos, mtd);

	int tail = len % mtdinfo->writesize;
	int head = len - tail;

	int writeret;
	if ((writeret = write(fd, data, head)) < 0) {
		g_message("head write failed");
		goto err_writehead;
	}

	guint8* paddedtail = NULL;
	if (tail > 0) {
		paddedtail = g_malloc0(mtdinfo->writesize);
		memcpy(paddedtail, data + head, tail);
		if ((writeret = write(fd, paddedtail, mtdinfo->writesize)) < 0) {
			g_message("tail write failed");
			goto err_writetail;
		}
	}

	ret = TRUE;

	err_writetail: //
	if (paddedtail != NULL)
		g_free(paddedtail);
	err_writehead: //
	close(fd);
	err_open: //
	err_imagesz: //
	return ret;
}

gchar* mtd_foroffset(guint32 off) {
	const gchar* mtdclasspath = "/sys/class/mtd";
	GDir* mtdclassdir = g_dir_open(mtdclasspath, 0, NULL);
	for (const gchar* subdir = g_dir_read_name(mtdclassdir); subdir != NULL;
			subdir = g_dir_read_name(mtdclassdir)) {
		const gchar* offsetpath = g_build_path("/", mtdclasspath, subdir,
				"offset", NULL);
		const gchar* sizepath = g_build_path("/", mtdclasspath, subdir, "size",
		NULL);

		guint64 offset = 0;
		guint64 size = 0;
		gsize offsetdatasz;
		gchar* offsetdata;
		if (!g_file_get_contents(offsetpath, &offsetdata, &offsetdatasz, NULL))
			goto err_readoff;

		gsize sizedatasz;
		gchar* sizedata;
		if (!g_file_get_contents(sizepath, &sizedata, &sizedatasz, NULL))
			goto err_readsz;

		offset = g_ascii_strtoull(offsetdata, NULL, 10);
		size = g_ascii_strtoull(sizedata, NULL, 10);

		g_free(sizedata);
		err_readsz: //
		g_free(offsetdata);
		err_readoff: //

		if (size == 0)
			continue;

		if (off >= offset && off < offset + size) {
			gchar* mtd = g_build_path("/", "/dev", subdir, NULL);
			return mtd;
		}
	}
	return NULL;
}
