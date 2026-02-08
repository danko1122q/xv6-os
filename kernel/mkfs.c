#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

// Simpan definisi asli stat sebelum di-redefine
#include <sys/stat.h>
typedef struct stat host_stat_t;

// Baru define stat untuk xv6
#define stat xv6_stat
#include "../include/fs.h"
#include "../include/param.h"
#include "../include/stat.h"
#include "../include/types.h"

#ifndef static_assert
#define static_assert(a, b)                                                    \
	do {                                                                   \
		switch (0)                                                     \
		case 0:                                                        \
		case (a):;                                                     \
	} while (0)
#endif

#define NINODES 200

int nbitmap = FSSIZE / (BSIZE * 8) + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGSIZE;
int nmeta;
int nblocks;

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void *);
void winode(uint, struct dinode *);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

ushort xshort(ushort x) {
	ushort y;
	uchar *a = (uchar *)&y;
	a[0] = x;
	a[1] = x >> 8;
	return y;
}

uint xint(uint x) {
	uint y;
	uchar *a = (uchar *)&y;
	a[0] = x;
	a[1] = x >> 8;
	a[2] = x >> 16;
	a[3] = x >> 24;
	return y;
}

int main(int argc, char *argv[]) {
	int i, cc, fd;
	uint rootino, inum, off;
	struct dirent de;
	char buf[BSIZE];
	struct dinode din;
	host_stat_t hst; // Gunakan typedef dari host

	static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

	if (argc < 2) {
		fprintf(stderr, "Usage: mkfs fs.img files...\n");
		exit(1);
	}

	assert((BSIZE % sizeof(struct dinode)) == 0);
	assert((BSIZE % sizeof(struct dirent)) == 0);

	fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fsfd < 0) {
		perror(argv[1]);
		exit(1);
	}

	nmeta = 2 + nlog + ninodeblocks + nbitmap;
	nblocks = FSSIZE - nmeta;

	sb.size = xint(FSSIZE);
	sb.nblocks = xint(nblocks);
	sb.ninodes = xint(NINODES);
	sb.nlog = xint(nlog);
	sb.logstart = xint(2);
	sb.inodestart = xint(2 + nlog);
	sb.bmapstart = xint(2 + nlog + ninodeblocks);
	sb.checksum = 0;

	printf("nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap "
	       "blocks %u) blocks %d total %d\n",
	       nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);
	printf("Max file size: %lu bytes (%lu MB)\n",
	       (unsigned long)MAXFILE * BSIZE,
	       (unsigned long)(MAXFILE * BSIZE) / (1024 * 1024));

	freeblock = nmeta;

	for (i = 0; i < FSSIZE; i++)
		wsect(i, zeroes);

	memset(buf, 0, sizeof(buf));
	memmove(buf, &sb, sizeof(sb));
	wsect(1, buf);

	rootino = ialloc(T_DIR);
	assert(rootino == ROOTINO);

	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, ".");
	iappend(rootino, &de, sizeof(de));

	bzero(&de, sizeof(de));
	de.inum = xshort(rootino);
	strcpy(de.name, "..");
	iappend(rootino, &de, sizeof(de));

	for (i = 2; i < argc; i++) {
		char *filepath = argv[i];
		char *filename = basename(strdup(filepath));

		// Skip underscore prefix for executables only
		if (filename[0] == '_')
			++filename;

		if ((fd = open(filepath, 0)) < 0) {
			perror(filepath);
			exit(1);
		}

		// Check file size menggunakan host_stat
		if (fstat(fd, &hst) < 0) {
			perror("fstat");
			close(fd);
			exit(1);
		}

		printf("Adding: %s (%ld bytes, inode ", filename,
		       (long)hst.st_size);

		if (hst.st_size > MAXFILE * BSIZE) {
			fprintf(stderr, "\nError: %s too large (%ld > %lu)\n",
				filename, (long)hst.st_size,
				(unsigned long)MAXFILE * BSIZE);
			close(fd);
			continue;
		}

		inum = ialloc(T_FILE);
		printf("%d)\n", inum);

		bzero(&de, sizeof(de));
		de.inum = xshort(inum);
		strncpy(de.name, filename, DIRSIZ);
		iappend(rootino, &de, sizeof(de));

		int total = 0;
		while ((cc = read(fd, buf, sizeof(buf))) > 0) {
			iappend(inum, buf, cc);
			total += cc;
			if (total % (1024 * 1024) == 0) {
				printf("  ... %d KB written\n", total / 1024);
			}
		}

		printf("  Total written: %d bytes\n", total);

		close(fd);
	}

	// Fix up root inode size
	rinode(rootino, &din);
	off = xint(din.data.size);
	off = ((off / BSIZE) + 1) * BSIZE;
	din.data.size = xint(off);
	winode(rootino, &din);

	balloc(freeblock);

	printf("Filesystem created successfully!\n");
	printf("Free blocks remaining: %d\n",
	       nblocks - (int)(freeblock - nmeta));

	exit(0);
}

void wsect(uint sec, void *buf) {
	if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
		perror("lseek");
		exit(1);
	}
	if (write(fsfd, buf, BSIZE) != BSIZE) {
		perror("write");
		exit(1);
	}
}

void winode(uint inum, struct dinode *ip) {
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb);
	rsect(bn, buf);
	dip = ((struct dinode *)buf) + (inum % IPB);
	*dip = *ip;
	wsect(bn, buf);
}

void rinode(uint inum, struct dinode *ip) {
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;

	bn = IBLOCK(inum, sb);
	rsect(bn, buf);
	dip = ((struct dinode *)buf) + (inum % IPB);
	*ip = *dip;
}

void rsect(uint sec, void *buf) {
	if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE) {
		perror("lseek");
		exit(1);
	}
	if (read(fsfd, buf, BSIZE) != BSIZE) {
		perror("read");
		exit(1);
	}
}

uint ialloc(ushort type) {
	uint inum = freeinode++;
	struct dinode din;

	bzero(&din, sizeof(din));
	din.data.type = xshort(type);
	din.data.nlink = xshort(1);
	din.data.size = xint(0);
	winode(inum, &din);
	return inum;
}

void balloc(int used) {
	uchar buf[BSIZE];
	int i;

	printf("balloc: first %d blocks have been allocated\n", used);
	assert(used < BSIZE * 8);
	bzero(buf, BSIZE);
	for (i = 0; i < used; i++) {
		buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
	}
	wsect(xint(sb.bmapstart), buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void iappend(uint inum, void *xp, int n) {
	char *p = (char *)xp;
	uint fbn, off, n1;
	struct dinode din;
	char buf[BSIZE];
	uint indirect[NINDIRECT];
	uint dindirect[NINDIRECT];
	uint x;

	rinode(inum, &din);
	off = xint(din.data.size);

	while (n > 0) {
		fbn = off / BSIZE;

		if (fbn >= MAXFILE) {
			fprintf(stderr,
				"\niappend: file too large (block %u >= %lu)\n",
				fbn, (unsigned long)MAXFILE);
			exit(1);
		}

		if (fbn < NDIRECT) {
			if (xint(din.data.addrs[fbn]) == 0) {
				if (freeblock >= (uint)(nmeta + nblocks)) {
					fprintf(stderr,
						"\nOut of data blocks!\n");
					exit(1);
				}
				din.data.addrs[fbn] = xint(freeblock++);
			}
			x = xint(din.data.addrs[fbn]);

		} else if (fbn < NDIRECT + NINDIRECT) {
			uint idx = fbn - NDIRECT;

			if (xint(din.data.addrs[NDIRECT]) == 0) {
				if (freeblock >= (uint)(nmeta + nblocks)) {
					fprintf(stderr, "\nOut of data blocks "
							"(indirect)!\n");
					exit(1);
				}
				din.data.addrs[NDIRECT] = xint(freeblock++);
			}

			rsect(xint(din.data.addrs[NDIRECT]), (char *)indirect);

			if (indirect[idx] == 0) {
				if (freeblock >= (uint)(nmeta + nblocks)) {
					fprintf(stderr,
						"\nOut of data blocks!\n");
					exit(1);
				}
				indirect[idx] = xint(freeblock++);
				wsect(xint(din.data.addrs[NDIRECT]),
				      (char *)indirect);
			}
			x = xint(indirect[idx]);

		} else {
			uint idx1 = (fbn - NDIRECT - NINDIRECT) / NINDIRECT;
			uint idx2 = (fbn - NDIRECT - NINDIRECT) % NINDIRECT;

			if (idx1 >= NINDIRECT) {
				fprintf(stderr, "\nDouble indirect index out "
						"of range!\n");
				exit(1);
			}

			if (xint(din.data.addrs[NDIRECT + 1]) == 0) {
				if (freeblock >= (uint)(nmeta + nblocks)) {
					fprintf(stderr, "\nOut of data blocks "
							"(dindirect)!\n");
					exit(1);
				}
				din.data.addrs[NDIRECT + 1] = xint(freeblock++);
			}

			rsect(xint(din.data.addrs[NDIRECT + 1]),
			      (char *)indirect);

			if (indirect[idx1] == 0) {
				if (freeblock >= (uint)(nmeta + nblocks)) {
					fprintf(stderr, "\nOut of data blocks "
							"(indirect2)!\n");
					exit(1);
				}
				bzero(dindirect, sizeof(dindirect));
				indirect[idx1] = xint(freeblock++);
				wsect(xint(din.data.addrs[NDIRECT + 1]),
				      (char *)indirect);
			}

			rsect(xint(indirect[idx1]), (char *)dindirect);

			if (dindirect[idx2] == 0) {
				if (freeblock >= (uint)(nmeta + nblocks)) {
					fprintf(stderr,
						"\nOut of data blocks!\n");
					exit(1);
				}
				dindirect[idx2] = xint(freeblock++);
				wsect(xint(indirect[idx1]), (char *)dindirect);
			}
			x = xint(dindirect[idx2]);
		}

		n1 = min(n, (fbn + 1) * BSIZE - off);
		rsect(x, buf);
		bcopy(p, buf + off - (fbn * BSIZE), n1);
		wsect(x, buf);
		n -= n1;
		off += n1;
		p += n1;
	}

	din.data.size = xint(off);
	winode(inum, &din);
}