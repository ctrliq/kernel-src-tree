// SPDX-License-Identifier: GPL-2.0
/*
 * FUSE io-uring selftest
 *
 * Validates FUSE request dispatch over io_uring by running a minimal
 * in-memory filesystem daemon in a thread and exercising it from the
 * test thread.  The daemon negotiates FUSE_OVER_IO_URING during
 * FUSE_INIT, registers ring entries via FUSE_IO_URING_CMD_REGISTER,
 * and handles requests through FUSE_IO_URING_CMD_COMMIT_AND_FETCH.
 *
 * An atomic counter tracks requests served through io_uring so tests
 * can verify the kernel is actually using the io_uring path.
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
/* linux/fuse.h and liburing.h after glibc headers to avoid redefinition */
#include <linux/fuse.h>
#include <liburing.h>

#include "../../kselftest_harness.h"

/*
 * FUSE io-uring UAPI definitions.  Remove this block once linux/fuse.h
 * includes the io-uring structures (added upstream in v6.14).
 */

#ifndef FUSE_OVER_IO_URING
#define FUSE_OVER_IO_URING		(1ULL << 41)
#endif

#ifndef FUSE_URING_IN_OUT_HEADER_SZ
#define FUSE_URING_IN_OUT_HEADER_SZ	128
#define FUSE_URING_OP_IN_OUT_SZ		128

struct fuse_uring_ent_in_out {
	uint64_t flags;
	uint64_t commit_id;
	uint32_t payload_sz;
	uint32_t padding;
	uint64_t reserved;
};

struct fuse_uring_req_header {
	char in_out[FUSE_URING_IN_OUT_HEADER_SZ];
	char op_in[FUSE_URING_OP_IN_OUT_SZ];
	struct fuse_uring_ent_in_out ring_ent_in_out;
};

enum fuse_uring_cmd {
	FUSE_IO_URING_CMD_REGISTER = 1,
	FUSE_IO_URING_CMD_COMMIT_AND_FETCH = 2,
};

struct fuse_uring_cmd_req {
	uint64_t flags;
	uint64_t commit_id;
	uint16_t qid;
	uint8_t  padding[6];
};
#endif /* FUSE_URING_IN_OUT_HEADER_SZ */

/* ---- constants --------------------------------------------------------- */

#define HELLO_CONTENT	"Hello from FUSE io-uring!\n"
#define HELLO_LEN	(sizeof(HELLO_CONTENT) - 1)
#define TESTFILE_MAX	(256 * 1024)
#define INODE_ROOT	1
#define INODE_HELLO	2
#define INODE_TESTFILE	3
#define DIRENT_NAME_OFF	24	/* offsetof(struct fuse_dirent, name) */
#define DIRENT_ALIGN(x)	(((x) + 7) & ~(size_t)7)
#define MP_LEN		128	/* max mountpoint path length */
#define PATH_LEN	(MP_LEN + 32)

/* ---- daemon state (shared between daemon thread and test thread) ------- */

struct daemon_state {
	int		fuse_fd;
	struct io_uring	ring;
	int		ring_ready;
	int		nr_queues;
	size_t		max_payload_sz;

	/* per-queue entry */
	struct {
		struct fuse_uring_req_header *hdr;
		void		*payload;
		struct iovec	iov[2];
	} *entries;

	/* synchronisation */
	pthread_mutex_t	lock;
	pthread_cond_t	cond;
	atomic_int	ready;
	atomic_int	exiting;

	/* io-uring path verification */
	atomic_int	uring_reqs;

	/* in-memory filesystem */
	char		*testfile;
	size_t		testfile_sz;
};

/* ---- FUSE request handling --------------------------------------------- */

static size_t add_dirent(void *buf, size_t bufsz, size_t off,
			 uint64_t ino, unsigned type,
			 const char *name, uint64_t next_off)
{
	size_t nlen = strlen(name);
	size_t elen = DIRENT_ALIGN(DIRENT_NAME_OFF + nlen);
	struct fuse_dirent *d;

	if (off + elen > bufsz)
		return 0;
	d = (struct fuse_dirent *)((char *)buf + off);
	d->ino = ino;
	d->off = next_off;
	d->namelen = nlen;
	d->type = type;
	memcpy(d->name, name, nlen);
	if (elen > DIRENT_NAME_OFF + nlen)
		memset(d->name + nlen, 0, elen - DIRENT_NAME_OFF - nlen);
	return elen;
}

static void fill_attr(struct fuse_attr *a, uint64_t ino, size_t tf_sz)
{
	memset(a, 0, sizeof(*a));
	a->ino = ino;
	a->blksize = 4096;
	switch (ino) {
	case INODE_ROOT:
		a->mode = S_IFDIR | 0755;
		a->nlink = 2;
		break;
	case INODE_HELLO:
		a->mode = S_IFREG | 0444;
		a->nlink = 1;
		a->size = HELLO_LEN;
		break;
	case INODE_TESTFILE:
		a->mode = S_IFREG | 0644;
		a->nlink = 1;
		a->size = tf_sz;
		break;
	}
}

/*
 * Handle one FUSE request.  Returns payload bytes (>=0) or -errno.
 */
static int handle_request(struct daemon_state *ds,
			  struct fuse_uring_req_header *hdr, void *payload,
			  uint32_t payload_sz)
{
	struct fuse_in_header *ih = (void *)hdr->in_out;

	switch (ih->opcode) {
	case FUSE_GETATTR: {
		struct fuse_attr_out *ao = (void *)payload;

		if (ih->nodeid < INODE_ROOT || ih->nodeid > INODE_TESTFILE)
			return -ENOENT;
		memset(ao, 0, sizeof(*ao));
		ao->attr_valid = 1;
		fill_attr(&ao->attr, ih->nodeid, ds->testfile_sz);
		return sizeof(*ao);
	}
	case FUSE_LOOKUP: {
		const char *name = (const char *)payload;
		struct fuse_entry_out *eo;
		uint64_t ino = 0;

		if (ih->nodeid != INODE_ROOT)
			return -ENOENT;
		if (payload_sz >= 5 && !memcmp(name, "hello", 5) &&
		    (payload_sz == 5 || name[5] == '\0'))
			ino = INODE_HELLO;
		else if (payload_sz >= 8 && !memcmp(name, "testfile", 8) &&
			 (payload_sz == 8 || name[8] == '\0'))
			ino = INODE_TESTFILE;
		else
			return -ENOENT;
		eo = (void *)payload;
		memset(eo, 0, sizeof(*eo));
		eo->nodeid = ino;
		eo->generation = 1;
		eo->entry_valid = 1;
		eo->attr_valid = 1;
		fill_attr(&eo->attr, ino, ds->testfile_sz);
		return sizeof(*eo);
	}
	case FUSE_OPEN:
	case FUSE_OPENDIR: {
		struct fuse_open_out *oo = (void *)payload;

		memset(oo, 0, sizeof(*oo));
		return sizeof(*oo);
	}
	case FUSE_READ: {
		struct fuse_read_in *ri = (void *)hdr->op_in;
		const char *src;
		size_t src_sz, sz;

		if (ih->nodeid == INODE_HELLO) {
			src = HELLO_CONTENT;
			src_sz = HELLO_LEN;
		} else if (ih->nodeid == INODE_TESTFILE) {
			src = ds->testfile;
			src_sz = ds->testfile_sz;
		} else {
			return -ENOENT;
		}
		if (ri->offset >= src_sz)
			return 0;
		sz = src_sz - ri->offset;
		if (sz > ri->size)
			sz = ri->size;
		memcpy(payload, src + ri->offset, sz);
		return (int)sz;
	}
	case FUSE_WRITE: {
		struct fuse_write_in *wi = (void *)hdr->op_in;
		struct fuse_write_out *wo;
		size_t sz = wi->size, end;

		if (ih->nodeid != INODE_TESTFILE)
			return -EACCES;
		if (sz > payload_sz)
			sz = payload_sz;
		end = wi->offset + sz;
		if (end > TESTFILE_MAX)
			return -ENOSPC;
		memcpy(ds->testfile + wi->offset, payload, sz);
		if (end > ds->testfile_sz)
			ds->testfile_sz = end;
		wo = (void *)payload;
		memset(wo, 0, sizeof(*wo));
		wo->size = sz;
		return sizeof(*wo);
	}
	case FUSE_READDIR: {
		struct fuse_read_in *ri = (void *)hdr->op_in;
		size_t bufsz = ri->size, w = 0, n;

		if (ih->nodeid != INODE_ROOT)
			return -ENOENT;
		if (bufsz > ds->max_payload_sz)
			bufsz = ds->max_payload_sz;
		if (ri->offset < 1) {
			n = add_dirent(payload, bufsz, w, INODE_ROOT, DT_DIR, ".", 1);
			if (n) w += n;
		}
		if (ri->offset < 2) {
			n = add_dirent(payload, bufsz, w, INODE_ROOT, DT_DIR, "..", 2);
			if (n) w += n;
		}
		if (ri->offset < 3) {
			n = add_dirent(payload, bufsz, w, INODE_HELLO, DT_REG, "hello", 3);
			if (n) w += n;
		}
		if (ri->offset < 4) {
			n = add_dirent(payload, bufsz, w, INODE_TESTFILE, DT_REG, "testfile", 4);
			if (n) w += n;
		}
		return (int)w;
	}
	case FUSE_STATFS: {
		struct fuse_statfs_out *so = (void *)payload;

		memset(so, 0, sizeof(*so));
		so->st.bsize = 4096;
		so->st.frsize = 4096;
		so->st.namelen = 255;
		so->st.blocks = 1024;
		so->st.bfree = 512;
		so->st.bavail = 512;
		return sizeof(*so);
	}
	case FUSE_RELEASE:
	case FUSE_RELEASEDIR:
	case FUSE_FLUSH:
	case FUSE_FSYNC:
	case FUSE_FSYNCDIR:
		return 0;
	case FUSE_DESTROY:
		ds->exiting = 1;
		return 0;
	default:
		return -ENOSYS;
	}
}

/* ---- io_uring SQE helpers ---------------------------------------------- */

static int sqe_register(struct io_uring *ring, int fd,
			struct iovec *iov, int qid, void *user_data)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	struct fuse_uring_cmd_req *cr;

	if (!sqe)
		return -ENOSPC;
	io_uring_prep_rw(IORING_OP_URING_CMD, sqe, fd, NULL, 0, 0);
	sqe->cmd_op = FUSE_IO_URING_CMD_REGISTER;
	sqe->addr = (unsigned long)iov;
	sqe->len = 2;
	sqe->user_data = (uint64_t)(uintptr_t)user_data;
	cr = (void *)sqe->cmd;
	memset(cr, 0, sizeof(*cr));
	cr->qid = qid;
	return 0;
}

static int sqe_commit_fetch(struct io_uring *ring, int fd,
			     struct fuse_uring_req_header *hdr,
			     uint64_t commit_id, int qid,
			     int32_t error, uint32_t payload_sz,
			     void *user_data)
{
	struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
	struct fuse_uring_cmd_req *cr;
	struct fuse_out_header *oh = (void *)hdr->in_out;

	if (!sqe)
		return -ENOSPC;

	oh->len = sizeof(*oh);
	oh->error = error;
	oh->unique = commit_id;
	hdr->ring_ent_in_out.commit_id = commit_id;
	hdr->ring_ent_in_out.payload_sz = payload_sz;

	io_uring_prep_rw(IORING_OP_URING_CMD, sqe, fd, NULL, 0, 0);
	sqe->cmd_op = FUSE_IO_URING_CMD_COMMIT_AND_FETCH;
	sqe->user_data = (uint64_t)(uintptr_t)user_data;
	cr = (void *)sqe->cmd;
	memset(cr, 0, sizeof(*cr));
	cr->commit_id = commit_id;
	cr->qid = qid;
	return 0;
}

/* ---- daemon thread ----------------------------------------------------- */

static int daemon_init(struct daemon_state *ds)
{
	char *buf;
	char reply[sizeof(struct fuse_out_header) + sizeof(struct fuse_init_out)];
	struct fuse_in_header *ih;
	struct fuse_init_in *ii;
	struct fuse_out_header *oh;
	struct fuse_init_out *io;
	ssize_t n;

	buf = malloc(FUSE_MIN_READ_BUFFER);
	if (!buf)
		return -1;

	n = read(ds->fuse_fd, buf, FUSE_MIN_READ_BUFFER);
	if (n < (ssize_t)sizeof(struct fuse_in_header)) {
		free(buf);
		return -1;
	}
	ih = (void *)buf;
	if (ih->opcode != FUSE_INIT) {
		free(buf);
		return -1;
	}
	ii = (void *)(buf + sizeof(*ih));

	if (!(((uint64_t)ii->flags | ((uint64_t)ii->flags2 << 32)) & FUSE_OVER_IO_URING)) {
		free(buf);
		return -1;
	}

	memset(reply, 0, sizeof(reply));
	oh = (void *)reply;
	io = (void *)(reply + sizeof(*oh));
	oh->len = sizeof(reply);
	oh->unique = ih->unique;
	io->major = FUSE_KERNEL_VERSION;
	io->minor = FUSE_KERNEL_MINOR_VERSION;
	io->max_readahead = ii->max_readahead;
	free(buf);

	io->max_write = TESTFILE_MAX;
	io->max_pages = TESTFILE_MAX / 4096;
	io->max_background = 16;
	io->congestion_threshold = 12;
	io->time_gran = 1;
	io->flags = FUSE_BIG_WRITES | FUSE_INIT_EXT;
	io->flags2 = (uint32_t)(FUSE_OVER_IO_URING >> 32);

	n = write(ds->fuse_fd, reply, sizeof(reply));
	if (n != sizeof(reply))
		return -1;

	ds->max_payload_sz = io->max_write;
	if (ds->max_payload_sz < FUSE_MIN_READ_BUFFER)
		ds->max_payload_sz = FUSE_MIN_READ_BUFFER;
	return 0;
}

static int daemon_setup_ring(struct daemon_state *ds)
{
	struct io_uring_params params = { .flags = IORING_SETUP_SQE128 };
	int i, ret;

	ret = io_uring_queue_init_params(ds->nr_queues * 2 + 4,
					 &ds->ring, &params);
	if (ret < 0)
		return ret;
	ds->ring_ready = 1;

	ds->testfile = calloc(1, TESTFILE_MAX);
	if (!ds->testfile)
		return -ENOMEM;

	ds->entries = calloc(ds->nr_queues, sizeof(*ds->entries));
	if (!ds->entries)
		return -ENOMEM;

	for (i = 0; i < ds->nr_queues; i++) {
		if (posix_memalign((void **)&ds->entries[i].hdr, 4096,
				   sizeof(*ds->entries[i].hdr)))
			return -ENOMEM;
		if (posix_memalign(&ds->entries[i].payload, 4096,
				   ds->max_payload_sz))
			return -ENOMEM;
		memset(ds->entries[i].hdr, 0, sizeof(*ds->entries[i].hdr));
		memset(ds->entries[i].payload, 0, ds->max_payload_sz);
		ds->entries[i].iov[0].iov_base = ds->entries[i].hdr;
		ds->entries[i].iov[0].iov_len = sizeof(*ds->entries[i].hdr);
		ds->entries[i].iov[1].iov_base = ds->entries[i].payload;
		ds->entries[i].iov[1].iov_len = ds->max_payload_sz;

		ret = sqe_register(&ds->ring, ds->fuse_fd,
				   ds->entries[i].iov, i,
				   (void *)(uintptr_t)i);
		if (ret)
			return ret;
	}
	return io_uring_submit(&ds->ring);
}

static void *daemon_loop(void *arg)
{
	struct daemon_state *ds = arg;
	struct io_uring_cqe *cqe;

	if (daemon_init(ds))
		goto out;
	if (daemon_setup_ring(ds) < 0)
		goto out;

	/* signal test thread that we're ready */
	pthread_mutex_lock(&ds->lock);
	ds->ready = 1;
	pthread_cond_signal(&ds->cond);
	pthread_mutex_unlock(&ds->lock);

	while (!ds->exiting) {
		if (io_uring_wait_cqe(&ds->ring, &cqe))
			break;
		do {
			int qid = (int)(uintptr_t)cqe->user_data;
			int ret;

			if (cqe->res < 0) {
				io_uring_cqe_seen(&ds->ring, cqe);
				if (cqe->res == -ENOTCONN ||
				    cqe->res == -ECONNABORTED)
					goto out;
				continue;
			}

			atomic_fetch_add(&ds->uring_reqs, 1);

			ret = handle_request(ds, ds->entries[qid].hdr,
					     ds->entries[qid].payload,
					     ds->entries[qid].hdr->ring_ent_in_out.payload_sz);

			if (ret < 0) {
				if (sqe_commit_fetch(&ds->ring, ds->fuse_fd,
						     ds->entries[qid].hdr,
						     ds->entries[qid].hdr->ring_ent_in_out.commit_id,
						     qid, ret, 0,
						     (void *)(uintptr_t)qid))
					goto out;
			} else {
				if (sqe_commit_fetch(&ds->ring, ds->fuse_fd,
						     ds->entries[qid].hdr,
						     ds->entries[qid].hdr->ring_ent_in_out.commit_id,
						     qid, 0, ret,
						     (void *)(uintptr_t)qid))
					goto out;
			}

			io_uring_cqe_seen(&ds->ring, cqe);
		} while (io_uring_peek_cqe(&ds->ring, &cqe) == 0);

		io_uring_submit(&ds->ring);
	}
out:
	return NULL;
}

/* ---- daemon lifecycle (used by fixtures) ------------------------------- */

static int daemon_start(struct daemon_state *ds, const char *mountpoint)
{
	char opts[256];

	memset(ds, 0, sizeof(*ds));
	pthread_mutex_init(&ds->lock, NULL);
	pthread_cond_init(&ds->cond, NULL);
	ds->nr_queues = sysconf(_SC_NPROCESSORS_CONF);
	if (ds->nr_queues > 64)
		ds->nr_queues = 64;

	ds->fuse_fd = open("/dev/fuse", O_RDWR | O_CLOEXEC);
	if (ds->fuse_fd < 0)
		return -errno;

	snprintf(opts, sizeof(opts),
		 "fd=%d,rootmode=40000,user_id=%u,group_id=%u",
		 ds->fuse_fd, getuid(), getgid());
	if (mount("fuse_uring_test", mountpoint, "fuse",
		  MS_NOSUID | MS_NODEV, opts)) {
		close(ds->fuse_fd);
		ds->fuse_fd = -1;
		return -errno;
	}

	return 0;
}

static int daemon_wait_ready(struct daemon_state *ds, const char *mountpoint,
			     pthread_t *thread)
{
	struct timespec ts;
	int err;

	if (pthread_create(thread, NULL, daemon_loop, ds))
		return -errno;

	pthread_mutex_lock(&ds->lock);
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 10;
	err = 0;
	while (!ds->ready && !err)
		err = pthread_cond_timedwait(&ds->cond, &ds->lock, &ts);
	pthread_mutex_unlock(&ds->lock);

	if (!ds->ready) {
		/* Thread was created but daemon failed; clean up */
		ds->exiting = 1;
		umount2(mountpoint, MNT_DETACH);
		pthread_join(*thread, NULL);
		close(ds->fuse_fd);
		ds->fuse_fd = -1;
		return -ETIMEDOUT;
	}
	return 0;
}

static void daemon_stop(struct daemon_state *ds, const char *mountpoint,
			pthread_t thread)
{
	int i;

	umount2(mountpoint, MNT_DETACH);
	pthread_join(thread, NULL);
	if (ds->ring_ready)
		io_uring_queue_exit(&ds->ring);
	if (ds->entries) {
		for (i = 0; i < ds->nr_queues; i++) {
			free(ds->entries[i].hdr);
			free(ds->entries[i].payload);
		}
		free(ds->entries);
	}
	free(ds->testfile);
	if (ds->fuse_fd >= 0)
		close(ds->fuse_fd);
	pthread_mutex_destroy(&ds->lock);
	pthread_cond_destroy(&ds->cond);
}

/* ---- prerequisites and kernel parameter management --------------------- */

static int orig_uring_disabled = -1;	/* -1 = not saved */
static int orig_enable_uring = -1;	/* -1 = not saved */

static int read_sysctl_int(const char *path)
{
	FILE *f;
	int val = -1;

	f = fopen(path, "r");
	if (f) {
		if (fscanf(f, "%d", &val) != 1)
			val = -1;
		fclose(f);
	}
	return val;
}

static void write_sysctl(const char *path, const char *val)
{
	FILE *f;

	f = fopen(path, "w");
	if (f) {
		fputs(val, f);
		fclose(f);
	}
}

static int ensure_io_uring_enabled(void)
{
	int val;

	/* enable io_uring if kernel restricts it */
	val = read_sysctl_int("/proc/sys/kernel/io_uring_disabled");
	if (val > 0) {
		if (orig_uring_disabled < 0)
			orig_uring_disabled = val;
		write_sysctl("/proc/sys/kernel/io_uring_disabled", "0");
		if (read_sysctl_int("/proc/sys/kernel/io_uring_disabled") != 0)
			return -1;
	}

	return 0;
}

static int ensure_fuse_uring_enabled(void)
{
	FILE *f;
	char val;

	if (access("/dev/fuse", R_OK | W_OK))
		return -1;

	f = fopen("/sys/module/fuse/parameters/enable_uring", "r");
	if (!f)
		return -1;
	val = fgetc(f);
	fclose(f);

	if (val != 'Y') {
		if (orig_enable_uring < 0)
			orig_enable_uring = 0;
		write_sysctl("/sys/module/fuse/parameters/enable_uring", "1");
		/* re-check */
		f = fopen("/sys/module/fuse/parameters/enable_uring", "r");
		if (!f)
			return -1;
		val = fgetc(f);
		fclose(f);
		if (val != 'Y')
			return -1;
	}

	return 0;
}

static void restore_kernel_params(void)
{
	if (orig_enable_uring >= 0) {
		write_sysctl("/sys/module/fuse/parameters/enable_uring",
			     orig_enable_uring ? "1" : "0");
		orig_enable_uring = -1;
	}
	if (orig_uring_disabled >= 0) {
		char buf[16];

		snprintf(buf, sizeof(buf), "%d", orig_uring_disabled);
		write_sysctl("/proc/sys/kernel/io_uring_disabled", buf);
		orig_uring_disabled = -1;
	}
}

/* ---- post-test health helpers ------------------------------------------ */

/*
 * Count entries under /sys/fs/fuse/connections — each corresponds to
 * an active fuse_conn.  Used to detect connection leaks after teardown.
 */
static int count_fuse_connections(void)
{
	DIR *d = opendir("/sys/fs/fuse/connections");
	struct dirent *de;
	int n = 0;

	if (!d)
		return -1;
	while ((de = readdir(d)) != NULL) {
		if (de->d_name[0] == '.')
			continue;
		n++;
	}
	closedir(d);
	return n;
}

/*
 * Time-bounded umount.  Retries every 50 ms up to deadline_ms total.
 * Returns 0 on success (or already unmounted), -1 on timeout.
 */
static int umount_with_deadline(const char *mp, unsigned int deadline_ms)
{
	unsigned int slept_ms = 0;

	while (slept_ms <= deadline_ms) {
		int rc = umount2(mp, MNT_DETACH);

		if (rc == 0)
			return 0;
		if (errno == EINVAL || errno == ENOENT)
			return 0;  /* not mounted — treat as success */
		usleep(50000);
		slept_ms += 50;
	}
	return -1;
}

/* ---- kselftest fixtures ------------------------------------------------ */

FIXTURE(fuse_uring) {
	struct daemon_state ds;
	char		    mountpoint[MP_LEN];
	pthread_t	    thread;
	int		    thread_started;
	int		    conn_baseline;
};

FIXTURE_SETUP(fuse_uring)
{
	static int atexit_registered;

	if (getuid() != 0)
		SKIP(return, "must run as root");
	if (!atexit_registered) {
		atexit(restore_kernel_params);
		atexit_registered = 1;
	}
	if (ensure_io_uring_enabled())
		SKIP(return, "cannot enable io_uring");
	if (ensure_fuse_uring_enabled())
		SKIP(return, "FUSE io-uring not available");

	strcpy(self->mountpoint, "/tmp/fuse_uring_XXXXXX");
	if (!mkdtemp(self->mountpoint))
		SKIP(return, "mkdtemp: %s", strerror(errno));

	if (daemon_start(&self->ds, self->mountpoint)) {
		rmdir(self->mountpoint);
		SKIP(return, "daemon_start: %s", strerror(errno));
	}
	if (daemon_wait_ready(&self->ds, self->mountpoint, &self->thread)) {
		rmdir(self->mountpoint);
		SKIP(return, "daemon did not become ready");
	}
	self->thread_started = 1;
	self->conn_baseline = count_fuse_connections();
	if (self->conn_baseline < 0)
		self->conn_baseline = 0;
}

FIXTURE_TEARDOWN(fuse_uring)
{
	if (!self->thread_started)
		return;
	daemon_stop(&self->ds, self->mountpoint, self->thread);
	rmdir(self->mountpoint);
}

/* ---- tests ------------------------------------------------------------- */

TEST_F(fuse_uring, read_file)
{
	char buf[256];
	char path[PATH_LEN];
	int fd, n;

	snprintf(path, sizeof(path), "%s/hello", self->mountpoint);
	fd = open(path, O_RDONLY);
	ASSERT_GE(fd, 0);
	n = read(fd, buf, sizeof(buf));
	close(fd);
	ASSERT_EQ(n, (int)HELLO_LEN);
	buf[n] = '\0';
	ASSERT_STREQ(buf, HELLO_CONTENT);
}

TEST_F(fuse_uring, write_and_readback)
{
	char path[PATH_LEN];
	const char *msg = "io-uring write test";
	char buf[256];
	int fd, n;

	snprintf(path, sizeof(path), "%s/testfile", self->mountpoint);

	fd = open(path, O_WRONLY);
	ASSERT_GE(fd, 0);
	n = write(fd, msg, strlen(msg));
	close(fd);
	ASSERT_EQ(n, (int)strlen(msg));

	fd = open(path, O_RDONLY);
	ASSERT_GE(fd, 0);
	n = read(fd, buf, sizeof(buf));
	close(fd);
	ASSERT_EQ(n, (int)strlen(msg));
	buf[n] = '\0';
	ASSERT_STREQ(buf, msg);
}

TEST_F(fuse_uring, readdir)
{
	DIR *d;
	struct dirent *de;
	int found_hello = 0, found_testfile = 0;

	d = opendir(self->mountpoint);
	ASSERT_NE(d, NULL);
	while ((de = readdir(d)) != NULL) {
		if (!strcmp(de->d_name, "hello"))
			found_hello = 1;
		if (!strcmp(de->d_name, "testfile"))
			found_testfile = 1;
	}
	closedir(d);
	ASSERT_EQ(found_hello, 1);
	ASSERT_EQ(found_testfile, 1);
}

TEST_F(fuse_uring, stat_files)
{
	struct stat st;
	char path[PATH_LEN];

	ASSERT_EQ(stat(self->mountpoint, &st), 0);
	ASSERT_TRUE(S_ISDIR(st.st_mode));

	snprintf(path, sizeof(path), "%s/hello", self->mountpoint);
	ASSERT_EQ(stat(path, &st), 0);
	ASSERT_TRUE(S_ISREG(st.st_mode));
	ASSERT_EQ(st.st_size, (off_t)HELLO_LEN);
}

struct conc_ctx {
	const char *mountpoint;
	int	    id;
};

static void *concurrent_worker(void *arg)
{
	struct conc_ctx *ctx = arg;
	char path[PATH_LEN];
	int i;

	for (i = 0; i < 10; i++) {
		char buf[128];
		int fd;

		/* readers hit /hello */
		snprintf(path, sizeof(path), "%s/hello", ctx->mountpoint);
		fd = open(path, O_RDONLY);
		if (fd >= 0) {
			read(fd, buf, sizeof(buf));
			close(fd);
		}

		/* writers hit /testfile */
		snprintf(path, sizeof(path), "%s/testfile", ctx->mountpoint);
		fd = open(path, O_WRONLY);
		if (fd >= 0) {
			snprintf(buf, sizeof(buf), "thread%d iter%d", ctx->id, i);
			write(fd, buf, strlen(buf));
			close(fd);
		}
	}
	return NULL;
}

#define CONC_THREADS 4

TEST_F(fuse_uring, concurrent_io)
{
	pthread_t threads[CONC_THREADS];
	struct conc_ctx ctxs[CONC_THREADS];
	int i;

	for (i = 0; i < CONC_THREADS; i++) {
		ctxs[i].mountpoint = self->mountpoint;
		ctxs[i].id = i;
		pthread_create(&threads[i], NULL, concurrent_worker, &ctxs[i]);
	}
	for (i = 0; i < CONC_THREADS; i++)
		pthread_join(threads[i], NULL);

	/* verify filesystem still works after concurrent access */
	{
		char path[PATH_LEN];
		char buf[128];
		int fd, n;

		snprintf(path, sizeof(path), "%s/hello", self->mountpoint);
		fd = open(path, O_RDONLY);
		ASSERT_GE(fd, 0);
		n = read(fd, buf, sizeof(buf));
		close(fd);
		ASSERT_EQ(n, (int)HELLO_LEN);
	}
}

/*
 * Per-operation io_uring path verification.  Reset the daemon's
 * request counter before each distinct operation type and verify it
 * incremented afterwards.  If the kernel silently falls back to
 * /dev/fuse for any opcode, the counter stays at zero for that op.
 */
TEST_F(fuse_uring, requests_via_uring)
{
	char path[PATH_LEN];
	char buf[256];
	int fd, n, before, after;
	DIR *d;
	struct dirent *de;
	struct stat st;

	/* LOOKUP + OPEN + READ + RELEASE (via open+read+close) */
	atomic_store(&self->ds.uring_reqs, 0);
	snprintf(path, sizeof(path), "%s/hello", self->mountpoint);
	fd = open(path, O_RDONLY);
	ASSERT_GE(fd, 0);
	n = read(fd, buf, sizeof(buf));
	close(fd);
	ASSERT_EQ(n, (int)HELLO_LEN);
	after = atomic_load(&self->ds.uring_reqs);
	ASSERT_GT(after, 0);
	TH_LOG("read path: %d io_uring requests (LOOKUP+OPEN+READ+RELEASE)",
	       after);

	/* WRITE (via open+write+close on testfile) */
	before = atomic_load(&self->ds.uring_reqs);
	snprintf(path, sizeof(path), "%s/testfile", self->mountpoint);
	fd = open(path, O_WRONLY);
	ASSERT_GE(fd, 0);
	n = write(fd, "verify", 6);
	close(fd);
	ASSERT_EQ(n, 6);
	after = atomic_load(&self->ds.uring_reqs);
	ASSERT_GT(after, before);
	TH_LOG("write path: %d new io_uring requests", after - before);

	/* READDIR (via opendir+readdir+closedir) */
	before = atomic_load(&self->ds.uring_reqs);
	d = opendir(self->mountpoint);
	ASSERT_NE(d, NULL);
	while ((de = readdir(d)) != NULL)
		;
	closedir(d);
	after = atomic_load(&self->ds.uring_reqs);
	ASSERT_GT(after, before);
	TH_LOG("readdir path: %d new io_uring requests", after - before);

	/* GETATTR (via stat) */
	before = atomic_load(&self->ds.uring_reqs);
	snprintf(path, sizeof(path), "%s/hello", self->mountpoint);
	ASSERT_EQ(stat(path, &st), 0);
	after = atomic_load(&self->ds.uring_reqs);
	ASSERT_GT(after, before);
	TH_LOG("stat path: %d new io_uring requests", after - before);

	TH_LOG("total io_uring requests: %d", after);
}

/*
 * Sustained I/O: write 256KB in 4KB chunks with a known pattern, then
 * read the entire file back and verify every byte.  Exercises payload
 * buffer reuse across many commit/fetch cycles and catches corruption
 * in the io_uring request pipeline.
 */
TEST_F(fuse_uring, sustained_io)
{
	char path[PATH_LEN];
	char wbuf[4096], rbuf[4096];
	int fd, total, off, n, i;
	int write_sz = TESTFILE_MAX;

	snprintf(path, sizeof(path), "%s/testfile", self->mountpoint);

	/* write phase: fill file with pattern */
	fd = open(path, O_WRONLY);
	ASSERT_GE(fd, 0);
	for (off = 0; off < write_sz; off += sizeof(wbuf)) {
		/* fill with offset-dependent pattern */
		for (i = 0; i < (int)sizeof(wbuf); i++)
			wbuf[i] = (char)((off + i) & 0xff);
		n = write(fd, wbuf, sizeof(wbuf));
		ASSERT_EQ(n, (int)sizeof(wbuf));
	}
	close(fd);

	/* read phase: verify every byte */
	fd = open(path, O_RDONLY);
	ASSERT_GE(fd, 0);
	total = 0;
	while (total < write_sz) {
		n = read(fd, rbuf, sizeof(rbuf));
		ASSERT_GT(n, 0);
		for (i = 0; i < n; i++) {
			if (rbuf[i] != (char)((total + i) & 0xff)) {
				TH_LOG("data mismatch at offset %d: "
				       "expected 0x%02x got 0x%02x",
				       total + i,
				       (total + i) & 0xff,
				       (unsigned char)rbuf[i]);
				ASSERT_EQ(rbuf[i],
					  (char)((total + i) & 0xff));
			}
		}
		total += n;
	}
	close(fd);
	ASSERT_EQ(total, write_sz);
	TH_LOG("sustained I/O: wrote and verified %d bytes in 4KB chunks",
	       write_sz);
}

/*
 * Background requests: fill a file, then read it sequentially in small
 * chunks to trigger kernel readahead.  Readahead generates background
 * FUSE requests that flow through fuse_uring_queue_bq_req() and
 * fuse_uring_flush_bg() — the code path fixed by 31da059891bd (bg dispatch
 * ordering).  Verify that all data arrives correctly and that the
 * io_uring request count exceeds the minimum expected from foreground
 * requests alone, indicating background requests were dispatched.
 */
TEST_F(fuse_uring, background_requests)
{
	char path[PATH_LEN];
	char wbuf[4096], rbuf[512];
	int fd, n, i, off, total;
	int before, after, read_calls;
	int write_sz = TESTFILE_MAX;

	snprintf(path, sizeof(path), "%s/testfile", self->mountpoint);

	/* fill the file with a known pattern */
	fd = open(path, O_WRONLY);
	ASSERT_GE(fd, 0);
	for (off = 0; off < write_sz; off += sizeof(wbuf)) {
		for (i = 0; i < (int)sizeof(wbuf); i++)
			wbuf[i] = (char)((off + i) % 251);
		n = write(fd, wbuf, sizeof(wbuf));
		ASSERT_EQ(n, (int)sizeof(wbuf));
	}
	close(fd);

	/*
	 * Sequential read in small chunks (512B) to maximise the
	 * kernel's readahead window.  The kernel will issue background
	 * FUSE_READ requests ahead of what we ask for.
	 */
	before = atomic_load(&self->ds.uring_reqs);
	fd = open(path, O_RDONLY);
	ASSERT_GE(fd, 0);

	total = 0;
	read_calls = 0;
	while (total < write_sz) {
		n = read(fd, rbuf, sizeof(rbuf));
		ASSERT_GT(n, 0);
		/* verify pattern */
		for (i = 0; i < n; i++) {
			if (rbuf[i] != (char)((total + i) % 251)) {
				TH_LOG("bg read mismatch at offset %d",
				       total + i);
				ASSERT_EQ(rbuf[i],
					  (char)((total + i) % 251));
			}
		}
		total += n;
		read_calls++;
	}
	close(fd);
	after = atomic_load(&self->ds.uring_reqs);

	ASSERT_EQ(total, write_sz);

	/*
	 * The io_uring request count should reflect both the foreground
	 * reads and any background readahead the kernel issued.  At
	 * minimum we expect LOOKUP + OPEN + reads + RELEASE.
	 */
	TH_LOG("background requests: %d read() calls for %d bytes, "
	       "%d io_uring requests (delta from before)",
	       read_calls, write_sz, after - before);
	ASSERT_GT(after - before, 0);
}

/*
 * Crash recovery: fork a daemon process, drive active I/O against it
 * from multiple threads, then SIGKILL the daemon while requests are
 * in flight.  This exercises the teardown paths that race with
 * io_uring task work (the bug class fixed by bea4fe98204b, 952b5d36f6a2,
 * 198f45eeb9f7).
 */

static atomic_int crash_io_running;

static void *crash_io_worker(void *arg)
{
	const char *mp = arg;
	char path[PATH_LEN];
	char buf[128];

	snprintf(path, sizeof(path), "%s/hello", mp);
	while (crash_io_running) {
		int fd = open(path, O_RDONLY);

		if (fd >= 0) {
			read(fd, buf, sizeof(buf));
			close(fd);
		}
		snprintf(path, sizeof(path), "%s/testfile", mp);
		fd = open(path, O_WRONLY);
		if (fd >= 0) {
			write(fd, "crash", 5);
			close(fd);
		}
		snprintf(path, sizeof(path), "%s/hello", mp);
	}
	return NULL;
}

#define CRASH_IO_THREADS 4

TEST_F(fuse_uring, crash_recovery)
{
	char crash_mp[MP_LEN];
	pid_t pid;
	int status;

	strcpy(crash_mp, "/tmp/fuse_crash_XXXXXX");
	if (!mkdtemp(crash_mp))
		SKIP(return, "mkdtemp: %s", strerror(errno));

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		/* child: run a daemon, handle requests until killed */
		struct daemon_state cds;
		pthread_t ct;

		if (daemon_start(&cds, crash_mp))
			_exit(1);
		if (daemon_wait_ready(&cds, crash_mp, &ct))
			_exit(1);
		pthread_join(ct, NULL);
		_exit(0);
	}

	/* parent: wait for child's filesystem to appear */
	{
		char path[PATH_LEN];
		int fd, i;

		snprintf(path, sizeof(path), "%s/hello", crash_mp);
		for (i = 0; i < 50; i++) {
			fd = open(path, O_RDONLY);
			if (fd >= 0) {
				close(fd);
				break;
			}
			usleep(100000);
		}
	}

	/* start I/O threads hammering the filesystem */
	{
		pthread_t io_threads[CRASH_IO_THREADS];
		int i;

		crash_io_running = 1;
		for (i = 0; i < CRASH_IO_THREADS; i++)
			pthread_create(&io_threads[i], NULL,
				       crash_io_worker, crash_mp);

		/* let I/O build up, then kill daemon mid-flight */
		usleep(200000);
		kill(pid, SIGKILL);
		waitpid(pid, &status, 0);

		/* stop I/O threads (they'll get errors now) */
		crash_io_running = 0;
		for (i = 0; i < CRASH_IO_THREADS; i++)
			pthread_join(io_threads[i], NULL);
	}

	/*
	 * Bounded umount: if async teardown is stuck, this will
	 * time out rather than silently leaking the connection.
	 */
	{
		int rc, final_conns;

		rc = umount_with_deadline(crash_mp, 6000);
		ASSERT_EQ(rc, 0) {
			TH_LOG("crash_mp umount timed out after 6s "
			       "(async teardown may be stuck)");
		}
		rmdir(crash_mp);

		/* grace period for delayed_work to drain */
		usleep(200000);

		final_conns = count_fuse_connections();
		TH_LOG("crash_recovery: conns %d -> %d (baseline %d)",
		       self->conn_baseline, final_conns,
		       self->conn_baseline);
		ASSERT_LE(final_conns, self->conn_baseline + 1);
	}

	/*
	 * The kernel should be healthy.  Verify by doing an operation
	 * on the main fixture's filesystem.
	 */
	{
		struct stat st;

		ASSERT_EQ(stat(self->mountpoint, &st), 0);
		TH_LOG("kernel healthy after daemon crash with active I/O");
	}
}

/*
 * Abort before ring ready: negotiate FUSE_OVER_IO_URING in FUSE_INIT
 * but never register any io_uring entries, then abort the connection.
 * Exercises the deadlock fix (d55011469b41) where blocked allocators would
 * hang forever waiting for a ring that never becomes ready, and the
 * abort-during-creation fix (c146284c4355) where rings created after abort
 * would leak.
 */
TEST_F(fuse_uring, abort_before_ring_ready)
{
	char abort_mp[MP_LEN];
	pid_t pid;
	int status;
	struct stat st;

	strcpy(abort_mp, "/tmp/fuse_abort_XXXXXX");
	if (!mkdtemp(abort_mp))
		SKIP(return, "mkdtemp: %s", strerror(errno));

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		/*
		 * Child: open /dev/fuse, mount, do FUSE_INIT with
		 * FUSE_OVER_IO_URING, but never register io_uring
		 * entries.  The ring never becomes ready.  Sleep
		 * until killed — the kernel must handle teardown of
		 * an io_uring-negotiated connection where the ring
		 * was never initialised without deadlocking.
		 */
		struct daemon_state ds;

		memset(&ds, 0, sizeof(ds));
		if (daemon_start(&ds, abort_mp))
			_exit(1);
		if (daemon_init(&ds))
			_exit(1);
		pause();
		_exit(0);
	}

	/*
	 * Give the child time to complete FUSE_INIT, then kill it.
	 * This tears down the FUSE connection while fc->io_uring is
	 * set but the ring was never readied.  On a buggy kernel this
	 * deadlocks in fuse_block_alloc() or leaks resources.
	 */
	usleep(500000);
	kill(pid, SIGKILL);
	waitpid(pid, &status, 0);

	{
		int rc;

		rc = umount_with_deadline(abort_mp, 6000);
		ASSERT_EQ(rc, 0) {
			TH_LOG("abort_mp umount timed out after 6s");
		}
		rmdir(abort_mp);
	}

	/* verify kernel is still healthy */
	ASSERT_EQ(stat(self->mountpoint, &st), 0);
	TH_LOG("kernel healthy after teardown of uninitialized io_uring ring");
}

#define REG_RACE_CHILDREN 16

/*
 * Regression test for the race classes fixed by:
 *   952b5d36f6a2 fuse-uring: fix race between registration and
 *                connection abortion
 *   b70a3aca1693 fuse-uring: Avoid queue->stopped races and set/read
 *                that value under lock
 *   d351da750669 fuse-uring: Avoid use-after-free in
 *                fuse_uring_async_stop_queues (CVE-2026-64261)
 *
 * Forks N children performing FUSE_INIT with FUSE_OVER_IO_URING
 * negotiated but delaying entry registration.  Parent kills them
 * at staggered offsets — some during FUSE_INIT, others after
 * negotiation while in pause().  The crucial window is between
 * fuse_uring_create() running under fc->lock and fuse_abort_conn()
 * clearing fc->connected.
 *
 * Delays are deterministic (child_index * 2.5ms) for reproducibility.
 * Seed logged via TH_LOG for reruns.
 */
TEST_F(fuse_uring, teardown_under_concurrent_registration)
{
	char mp_bufs[REG_RACE_CHILDREN][MP_LEN];
	pid_t pids[REG_RACE_CHILDREN];
	int i, status, initial_conns, final_conns;
	unsigned int seed = (unsigned int)getpid();

	srandom(seed);
	TH_LOG("registration-race seed=%u", seed);

	initial_conns = count_fuse_connections();
	if (initial_conns < 0)
		initial_conns = 0;

	for (i = 0; i < REG_RACE_CHILDREN; i++) {
		snprintf(mp_bufs[i], sizeof(mp_bufs[i]),
			 "/tmp/fuse_regrace_%d_XXXXXX", i);
		if (!mkdtemp(mp_bufs[i])) {
			int j;

			for (j = 0; j < i; j++) {
				kill(pids[j], SIGKILL);
				waitpid(pids[j], &status, 0);
				umount_with_deadline(mp_bufs[j], 1000);
				rmdir(mp_bufs[j]);
			}
			SKIP(return, "mkdtemp: %s", strerror(errno));
		}

		pids[i] = fork();
		ASSERT_GE(pids[i], 0);

		if (pids[i] == 0) {
			struct daemon_state ds;

			memset(&ds, 0, sizeof(ds));
			if (daemon_start(&ds, mp_bufs[i]))
				_exit(1);
			if (daemon_init(&ds))
				_exit(1);
			/*
			 * Deliberately skip ring registration.
			 * Enter pause() with fc->io_uring=1 but ring
			 * never initialized.
			 */
			usleep(i * 2500);  /* deterministic stagger */
			pause();
			_exit(0);
		}
	}

	/* Stage 1: kill even-indexed children early */
	usleep(10000);
	for (i = 0; i < REG_RACE_CHILDREN; i += 2)
		kill(pids[i], SIGKILL);

	/* Stage 2: kill odd-indexed children slightly later */
	usleep(20000);
	for (i = 1; i < REG_RACE_CHILDREN; i += 2)
		kill(pids[i], SIGKILL);

	for (i = 0; i < REG_RACE_CHILDREN; i++)
		waitpid(pids[i], &status, 0);

	/* Bounded umount for all children */
	{
		int leaked = 0;

		for (i = 0; i < REG_RACE_CHILDREN; i++) {
			if (umount_with_deadline(mp_bufs[i], 6000) != 0)
				leaked++;
			rmdir(mp_bufs[i]);
		}
		ASSERT_EQ(leaked, 0) {
			TH_LOG("%d child mounts would not release", leaked);
		}
	}

	/* Wait for async teardown delayed_work to drain */
	usleep(6000000);

	final_conns = count_fuse_connections();
	TH_LOG("registration race: %d children, conns %d -> %d",
	       REG_RACE_CHILDREN, initial_conns, final_conns);
	ASSERT_EQ(final_conns, initial_conns);

	/* Main fixture mount must still be healthy */
	{
		struct stat st;

		ASSERT_EQ(stat(self->mountpoint, &st), 0);
	}
}

TEST_HARNESS_MAIN
