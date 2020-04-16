/**
	// High-level aggregate filesystem operations.
*/

#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef SEPARATOR
	#define SEPARATOR '/'
#endif

#ifndef TERMINATOR
	#define TERMINATOR '\0'
#endif

typedef enum {
	fs_mkdir_start_forwards = 1 << 0,
	fs_mkdir_dirty_failure = 1 << 1
} fs_mkdir_ctl;

/**
	// `mkdir -p` equivalent.
*/
static int
_fs_mkdir(fs_mkdir_ctl ctlopt, const char *dirpath, const mode_t dmode)
{
	int ncreated = 0;
	char buf[PATH_MAX];
	size_t len;
	char *p, *e;

	errno = 0;

	len = strlen(dirpath);
	if (len > sizeof(buf)-2)
	{
		errno = ENAMETOOLONG;
		return(-1);
	}

	bzero(buf, PATH_MAX);
	memcpy(buf, dirpath, len);
	buf[len] = SEPARATOR;
	buf[len+1] = TERMINATOR;

	if (ctlopt & fs_mkdir_start_forwards)
	{
		e = buf;
		if (*e != SEPARATOR)
			--e;
	}
	else
	{
		e = p = buf + len;
		++e; /* on the forced trailing slash */

		/* Backwards; skipped if dirpath equals "/" */
		while (p != buf)
		{
			if (*p == SEPARATOR)
			{
				*e = TERMINATOR;

				if (mkdir(buf, dmode) == 0)
				{
					*e = SEPARATOR;
					break;
				}
				else
				{
					if (errno == ENOTDIR)
					{
						/* Should never succeed. */
						return(-1);
					}

					errno = 0;
				}

				*e = SEPARATOR;
				e = p;
			}

			--p;
		}
	}

	/* Forwards */
	for (p = e + 1; *p != TERMINATOR; ++p)
	{
		if (*p == SEPARATOR)
		{
			*p = TERMINATOR;

			if (mkdir(buf, dmode) == 0)
				ncreated += 1;
			else
			{
				if (errno == EEXIST)
				{
					/* Directory already present. */
					;
				}
				else
				{
					/* Unacceptable failure. */
					goto failure;
				}
			}

			*p = SEPARATOR;
		}
	}

	if (mkdir(buf, dmode) != 0)
	{
		if (errno == EEXIST)
			errno = 0;
		else
			goto failure;
	}

	return(0);

	failure:
	{
		/*
			// Directory in path could not be created.
		*/

		if (ctlopt & fs_mkdir_dirty_failure)
			return(-(ncreated + 1));

		if (ncreated > 0)
		{
			int err = errno; /* preserve original failure */
			errno = 0;

			e = p = buf + len;
			++e; /* on the forced trailing slash */

			/* Backwards */
			while (p != buf)
			{
				if (*p == SEPARATOR)
				{
					*e = TERMINATOR;

					if (unlink(buf) != 0)
					{
						errno = err;
						return(-(ncreated + 1));
					}
					else
					{
						if (--ncreated == 0)
							break;
					}

					e = p;
				}

				--p;
			}

			errno = err;
		}

		return(-1);
	}
}

static int
fs_mkdir(const char *dirpath)
{
	return(_fs_mkdir(0, dirpath, S_IRWXU|S_IRWXG|S_IRWXO));
}

static int
fs_init(fs_mkdir_ctl ctlopt, const char *path, const mode_t dmode, const mode_t fmode, const char *data)
{
	int fd;
	size_t len, i;

	if (_fs_mkdir(ctlopt, path, dmode) != 0)
		return(-1);

	unlink(path);
	fd = open(path, O_WRONLY);
	if (fd < 0)
		return(-2);

	len = strlen(data);
	i = 0;
	while (len > 0)
	{
		i = write(fd, data, len);
		if (i > 0)
		{
			len -= i;
			data += i;
		}
		else
			break;
	}

	close(fd);
	return(0);
}

#undef TERMINATOR
#undef SEPARATOR
