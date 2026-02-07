// File system implementation with support for large files using double indirect
// blocks, mathematical invariants, and bitwise allocation optimizations.

#include "fs.h"
#include "buf.h"
#include "defs.h"
#include "file.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "sleeplock.h"
#include "spinlock.h"
#include "stat.h"
#include "types.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

// Invarian Matematika: Memastikan ukuran inode tepat 128 byte saat compile.
// Jika tidak pas, sistem tidak akan bisa di-build (Zero Bug Policy).
#define STATIC_ASSERT(COND, MSG)                                               \
	typedef char static_assertion_##MSG[(COND) ? 1 : -1]
STATIC_ASSERT(sizeof(struct dinode) == 128, INODE_SIZE_CORRUPT);

static void itrunc(struct inode *);
struct superblock sb;

// Hint untuk alokasi blok berikutnya guna meningkatkan spatial locality
// (kontiguitas)
static uint last_alloc_hint = 0;

// Fletcher-32 Checksum untuk memvalidasi integritas Superblock
uint fletcher32(ushort *data, int len) {
	uint sum1 = 0xffff, sum2 = 0xffff;
	while (len) {
		int tlen = len > 360 ? 360 : len;
		len -= tlen;
		do {
			sum1 += *data++;
			sum2 += sum1;
		} while (--tlen);
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	}
	return (sum2 << 16) | sum1;
}

// Load superblock from disk into memory
void readsb(int dev, struct superblock *sb_ptr) {
	struct buf *bp;
	bp = bread(dev, 1);
	memmove(sb_ptr, bp->data, sizeof(*sb_ptr));
	brelse(bp);

	// Copy ke global sb untuk penggunaan internal fs.c
	memmove(&sb, sb_ptr, sizeof(sb));
}

// Initialize a disk block to all zeros
static void bzero(int dev, int bno) {
	struct buf *bp;
	bp = bread(dev, bno);
	memset(bp->data, 0, BSIZE);
	log_write(bp);
	brelse(bp);
}

// Alokasi Blok Cerdas dengan Akselerasi Bitwise
// Melewati blok bitmap yang sudah penuh (0xFFFFFFFF) untuk kecepatan O(N/32)
static uint balloc(uint dev) {
	int b, bi, m;
	struct buf *bp;

	for (b = 0; b < sb.size; b += BPB) {
		// Spatial Locality: Mulai dari hint terakhir agar blok file
		// cenderung berdekatan
		uint curr_block = sb.bmapstart + ((last_alloc_hint + b) / BPB) %
							 (sb.size / BPB + 1);
		bp = bread(dev, curr_block);

		// Optimasi: Cek 32-bit sekaligus. Jika 0xFFFFFFFF, berarti 32
		// blok tersebut penuh.
		for (int i = 0; i < BSIZE; i += 4) {
			if (*(uint *)&bp->data[i] != 0xFFFFFFFF) {
				for (bi = (i * 8); bi < (i * 8) + 32; bi++) {
					m = 1 << (bi % 8);
					if ((bp->data[bi / 8] & m) == 0) {
						bp->data[bi / 8] |=
							m; // Tandai digunakan
						log_write(bp);
						brelse(bp);

						uint bno = ((curr_block -
							     sb.bmapstart) *
							    BPB) +
							   bi;
						bzero(dev, bno);
						last_alloc_hint = bno;
						return bno;
					}
				}
			}
		}
		brelse(bp);
	}
	panic("balloc: out of blocks");
}

// Release a disk block back to the free pool
static void bfree(int dev, uint b) {
	struct buf *bp;
	int bi, m;

	bp = bread(dev, BMAPBLOCK(b, sb));
	bi = b % BPB;
	m = 1 << (bi % 8);
	if ((bp->data[bi / 8] & m) == 0)
		panic("freeing free block");
	bp->data[bi / 8] &= ~m;
	log_write(bp);
	brelse(bp);
}

// Inode cache
struct {
	struct spinlock lock;
	struct inode inode[NINODE];
} icache;

void iinit(int dev) {
	int i = 0;
	initlock(&icache.lock, "icache");
	for (i = 0; i < NINODE; i++) {
		initsleeplock(&icache.inode[i].lock, "inode");
	}

	readsb(dev, &sb);
	cprintf("NorthOS FS: size %d, nblocks %d, inodestart %d, bmapstart "
		"%d\n",
		sb.size, sb.nblocks, sb.inodestart, sb.bmapstart);
}

static struct inode *iget(uint dev, uint inum);

// Allocate a new inode
struct inode *ialloc(uint dev, short type) {
	int inum;
	struct buf *bp;
	struct dinode *dip;

	for (inum = 1; inum < sb.ninodes; inum++) {
		bp = bread(dev, IBLOCK(inum, sb));
		dip = (struct dinode *)bp->data + inum % IPB;
		if (dip->data.type ==
		    0) { // Menggunakan dip->data karena struktur union di fs.h
			memset(dip, 0, sizeof(*dip));
			dip->data.type = type;
			log_write(bp);
			brelse(bp);
			return iget(dev, inum);
		}
		brelse(bp);
	}
	panic("ialloc: no inodes");
}

// Synchronize inode to disk
void iupdate(struct inode *ip) {
	struct buf *bp;
	struct dinode *dip;

	bp = bread(ip->dev, IBLOCK(ip->inum, sb));
	dip = (struct dinode *)bp->data + ip->inum % IPB;
	dip->data.type = ip->type;
	dip->data.major = ip->major;
	dip->data.minor = ip->minor;
	dip->data.nlink = ip->nlink;
	dip->data.size = ip->size;
	memmove(dip->data.addrs, ip->addrs, sizeof(ip->addrs));
	log_write(bp);
	brelse(bp);
}

static struct inode *iget(uint dev, uint inum) {
	struct inode *ip, *empty;
	acquire(&icache.lock);
	empty = 0;
	for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
		if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
			ip->ref++;
			release(&icache.lock);
			return ip;
		}
		if (empty == 0 && ip->ref == 0)
			empty = ip;
	}
	if (empty == 0)
		panic("iget: no inodes");
	ip = empty;
	ip->dev = dev;
	ip->inum = inum;
	ip->ref = 1;
	ip->valid = 0;
	release(&icache.lock);
	return ip;
}

struct inode *idup(struct inode *ip) {
	acquire(&icache.lock);
	ip->ref++;
	release(&icache.lock);
	return ip;
}

void ilock(struct inode *ip) {
	struct buf *bp;
	struct dinode *dip;
	if (ip == 0 || ip->ref < 1)
		panic("ilock");
	acquiresleep(&ip->lock);
	if (ip->valid == 0) {
		bp = bread(ip->dev, IBLOCK(ip->inum, sb));
		dip = (struct dinode *)bp->data + ip->inum % IPB;
		ip->type = dip->data.type;
		ip->major = dip->data.major;
		ip->minor = dip->data.minor;
		ip->nlink = dip->data.nlink;
		ip->size = dip->data.size;
		memmove(ip->addrs, dip->data.addrs, sizeof(ip->addrs));
		brelse(bp);
		ip->valid = 1;
		if (ip->type == 0)
			panic("ilock: no type");
	}
}

void iunlock(struct inode *ip) {
	if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
		panic("iunlock");
	releasesleep(&ip->lock);
}

void iput(struct inode *ip) {
	acquiresleep(&ip->lock);
	if (ip->valid && ip->nlink == 0) {
		acquire(&icache.lock);
		int r = ip->ref;
		release(&icache.lock);
		if (r == 1) {
			itrunc(ip);
			ip->type = 0;
			iupdate(ip);
			ip->valid = 0;
		}
	}
	releasesleep(&ip->lock);
	acquire(&icache.lock);
	ip->ref--;
	release(&icache.lock);
}

void iunlockput(struct inode *ip) {
	iunlock(ip);
	iput(ip);
}

// bmap: Mendukung Double Indirect untuk file besar
static uint bmap(struct inode *ip, uint bn) {
	uint addr, *a;
	struct buf *bp;

	if (bn < NDIRECT) {
		if ((addr = ip->addrs[bn]) == 0)
			ip->addrs[bn] = addr = balloc(ip->dev);
		return addr;
	}
	bn -= NDIRECT;

	if (bn < NINDIRECT) {
		if ((addr = ip->addrs[NDIRECT]) == 0)
			ip->addrs[NDIRECT] = addr = balloc(ip->dev);
		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		if ((addr = a[bn]) == 0) {
			a[bn] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
		return addr;
	}
	bn -= NINDIRECT;

	// Double Indirect logic
	if (bn < NDINDIRECT) {
		if ((addr = ip->addrs[NDIRECT + 1]) == 0)
			ip->addrs[NDIRECT + 1] = addr = balloc(ip->dev);

		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		uint index1 = bn / NINDIRECT;
		uint index2 = bn % NINDIRECT;

		if ((addr = a[index1]) == 0) {
			a[index1] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);

		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		if ((addr = a[index2]) == 0) {
			a[index2] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
		return addr;
	}
	panic("bmap: out of range");
}

static void itrunc(struct inode *ip) {
	int i, j, k;
	struct buf *bp, *bp2;
	uint *a, *a2;

	for (i = 0; i < NDIRECT; i++) {
		if (ip->addrs[i]) {
			bfree(ip->dev, ip->addrs[i]);
			ip->addrs[i] = 0;
		}
	}

	if (ip->addrs[NDIRECT]) {
		bp = bread(ip->dev, ip->addrs[NDIRECT]);
		a = (uint *)bp->data;
		for (j = 0; j < NINDIRECT; j++) {
			if (a[j])
				bfree(ip->dev, a[j]);
		}
		brelse(bp);
		bfree(ip->dev, ip->addrs[NDIRECT]);
		ip->addrs[NDIRECT] = 0;
	}

	if (ip->addrs[NDIRECT + 1]) {
		bp = bread(ip->dev, ip->addrs[NDIRECT + 1]);
		a = (uint *)bp->data;
		for (j = 0; j < NINDIRECT; j++) {
			if (a[j]) {
				bp2 = bread(ip->dev, a[j]);
				a2 = (uint *)bp2->data;
				for (k = 0; k < NINDIRECT; k++) {
					if (a2[k])
						bfree(ip->dev, a2[k]);
				}
				brelse(bp2);
				bfree(ip->dev, a[j]);
			}
		}
		brelse(bp);
		bfree(ip->dev, ip->addrs[NDIRECT + 1]);
		ip->addrs[NDIRECT + 1] = 0;
	}

	ip->size = 0;
	iupdate(ip);
}

void stati(struct inode *ip, struct stat *st) {
	st->dev = ip->dev;
	st->ino = ip->inum;
	st->type = ip->type;
	st->nlink = ip->nlink;
	st->size = ip->size;
}

int readi(struct inode *ip, char *dst, uint off, uint n) {
	uint tot, m;
	struct buf *bp;

	if (ip->type == T_DEV) {
		if (ip->major < 0 || ip->major >= NDEV ||
		    !devsw[ip->major].read)
			return -1;
		return devsw[ip->major].read(ip, dst, n);
	}

	if (off > ip->size || off + n < off)
		return -1;
	if (off + n > ip->size)
		n = ip->size - off;

	for (tot = 0; tot < n; tot += m, off += m, dst += m) {
		bp = bread(ip->dev, bmap(ip, off / BSIZE));
		m = min(n - tot, BSIZE - off % BSIZE);
		memmove(dst, bp->data + off % BSIZE, m);
		brelse(bp);
	}
	return n;
}

int writei(struct inode *ip, char *src, uint off, uint n) {
	uint tot, m;
	struct buf *bp;

	if (ip->type == T_DEV) {
		if (ip->major < 0 || ip->major >= NDEV ||
		    !devsw[ip->major].write)
			return -1;
		return devsw[ip->major].write(ip, src, n);
	}

	if (off > ip->size || off + n < off)
		return -1;
	if (off + n > MAXFILE * BSIZE)
		return -1;

	for (tot = 0; tot < n; tot += m, off += m, src += m) {
		bp = bread(ip->dev, bmap(ip, off / BSIZE));
		m = min(n - tot, BSIZE - off % BSIZE);
		memmove(bp->data + off % BSIZE, src, m);
		log_write(bp);
		brelse(bp);
	}

	if (n > 0 && off > ip->size) {
		ip->size = off;
		iupdate(ip);
	}
	return n;
}

int namecmp(const char *s, const char *t) { return strncmp(s, t, DIRSIZ); }

struct inode *dirlookup(struct inode *dp, char *name, uint *poff) {
	uint off, inum;
	struct dirent de;
	if (dp->type != T_DIR)
		panic("dirlookup not DIR");
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de))
			panic("dirlookup read");
		if (de.inum == 0)
			continue;
		if (namecmp(name, de.name) == 0) {
			if (poff)
				*poff = off;
			inum = de.inum;
			return iget(dp->dev, inum);
		}
	}
	return 0;
}

int dirlink(struct inode *dp, char *name, uint inum) {
	int off;
	struct dirent de;
	struct inode *ip;
	if ((ip = dirlookup(dp, name, 0)) != 0) {
		iput(ip);
		return -1;
	}
	for (off = 0; off < dp->size; off += sizeof(de)) {
		if (readi(dp, (char *)&de, off, sizeof(de)) != sizeof(de))
			panic("dirlink read");
		if (de.inum == 0)
			break;
	}
	strncpy(de.name, name, DIRSIZ);
	de.inum = inum;
	if (writei(dp, (char *)&de, off, sizeof(de)) != sizeof(de))
		panic("dirlink");
	return 0;
}

static char *skipelem(char *path, char *name) {
	char *s;
	int len;
	while (*path == '/')
		path++;
	if (*path == 0)
		return 0;
	s = path;
	while (*path != '/' && *path != 0)
		path++;
	len = path - s;
	if (len >= DIRSIZ)
		memmove(name, s, DIRSIZ);
	else {
		memmove(name, s, len);
		name[len] = 0;
	}
	while (*path == '/')
		path++;
	return path;
}

static struct inode *namex(char *path, int nameiparent, char *name) {
	struct inode *ip, *next;
	if (*path == '/')
		ip = iget(ROOTDEV, ROOTINO);
	else
		ip = idup(myproc()->cwd);
	while ((path = skipelem(path, name)) != 0) {
		ilock(ip);
		if (ip->type != T_DIR) {
			iunlockput(ip);
			return 0;
		}
		if (nameiparent && *path == '\0') {
			iunlock(ip);
			return ip;
		}
		if ((next = dirlookup(ip, name, 0)) == 0) {
			iunlockput(ip);
			return 0;
		}
		iunlockput(ip);
		ip = next;
	}
	if (nameiparent) {
		iput(ip);
		return 0;
	}
	return ip;
}

struct inode *namei(char *path) {
	char name[DIRSIZ];
	return namex(path, 0, name);
}

struct inode *nameiparent(char *path, char *name) {
	return namex(path, 1, name);
}