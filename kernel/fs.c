// File system implementation with support for large files using double indirect
// blocks. Architecture consists of five layers:
//   + Blocks: raw disk block allocation and management
//   + Log: transaction logging for crash recovery
//   + Files: inode management, I/O operations, metadata handling
//   + Directories: special inodes containing directory entries
//   + Names: path resolution and hierarchical naming
//
// This module implements the core file system primitives.
// Higher-level system call handlers are located in sysfile.c.

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

static void itrunc(struct inode *);

struct superblock sb;

// Load superblock from disk into memory
void readsb(int dev, struct superblock *sb) {
	struct buf *bp;

	bp = bread(dev, 1);
	memmove(sb, bp->data, sizeof(*sb));
	brelse(bp);
}

// Initialize a disk block to all zeros
static void bzero(int dev, int bno) {
	struct buf *bp;

	bp = bread(dev, bno);
	memset(bp->data, 0, BSIZE);
	log_write(bp);
	brelse(bp);
}

// Allocate a new disk block and zero its contents
static uint balloc(uint dev) {
	int b, bi, m;
	struct buf *bp;

	bp = 0;
	for (b = 0; b < sb.size; b += BPB) {
		bp = bread(dev, BBLOCK(b, sb));
		for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
			m = 1 << (bi % 8);
			if ((bp->data[bi / 8] & m) == 0) {
				bp->data[bi / 8] |= m;
				log_write(bp);
				brelse(bp);
				bzero(dev, b + bi);
				return b + bi;
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

	bp = bread(dev, BBLOCK(b, sb));
	bi = b % BPB;
	m = 1 << (bi % 8);
	if ((bp->data[bi / 8] & m) == 0)
		panic("freeing free block");
	bp->data[bi / 8] &= ~m;
	log_write(bp);
	brelse(bp);
}

// Inode cache structure
// Provides synchronization and caching for active inodes
// Protected by icache.lock spinlock
struct {
	struct spinlock lock;
	struct inode inode[NINODE];
} icache;

// Initialize the inode subsystem
// Sets up the inode cache and loads the superblock
void iinit(int dev) {
	int i = 0;

	initlock(&icache.lock, "icache");
	for (i = 0; i < NINODE; i++) {
		initsleeplock(&icache.inode[i].lock, "inode");
	}

	readsb(dev, &sb);
	cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d\
 inodestart %d bmap start %d\n",
		sb.size, sb.nblocks, sb.ninodes, sb.nlog, sb.logstart,
		sb.inodestart, sb.bmapstart);
}

static struct inode *iget(uint dev, uint inum);

// Allocate a new inode on the specified device
// Returns an unlocked but referenced inode
struct inode *ialloc(uint dev, short type) {
	int inum;
	struct buf *bp;
	struct dinode *dip;

	for (inum = 1; inum < sb.ninodes; inum++) {
		bp = bread(dev, IBLOCK(inum, sb));
		dip = (struct dinode *)bp->data + inum % IPB;
		if (dip->type == 0) {
			memset(dip, 0, sizeof(*dip));
			dip->type = type;
			log_write(bp);
			brelse(bp);
			return iget(dev, inum);
		}
		brelse(bp);
	}
	panic("ialloc: no inodes");
}

// Synchronize in-memory inode state to disk
// Must be called within a transaction and with inode locked
void iupdate(struct inode *ip) {
	struct buf *bp;
	struct dinode *dip;

	bp = bread(ip->dev, IBLOCK(ip->inum, sb));
	dip = (struct dinode *)bp->data + ip->inum % IPB;
	dip->type = ip->type;
	dip->major = ip->major;
	dip->minor = ip->minor;
	dip->nlink = ip->nlink;
	dip->size = ip->size;
	memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
	log_write(bp);
	brelse(bp);
}

// Retrieve inode from cache or allocate new cache entry
// Returns inode with incremented reference count
// Does not lock the inode or read from disk
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

// Create an additional reference to an inode
struct inode *idup(struct inode *ip) {
	acquire(&icache.lock);
	ip->ref++;
	release(&icache.lock);
	return ip;
}

// Acquire exclusive access to inode
// Loads inode metadata from disk if not already cached
void ilock(struct inode *ip) {
	struct buf *bp;
	struct dinode *dip;

	if (ip == 0 || ip->ref < 1)
		panic("ilock");

	acquiresleep(&ip->lock);

	if (ip->valid == 0) {
		bp = bread(ip->dev, IBLOCK(ip->inum, sb));
		dip = (struct dinode *)bp->data + ip->inum % IPB;
		ip->type = dip->type;
		ip->major = dip->major;
		ip->minor = dip->minor;
		ip->nlink = dip->nlink;
		ip->size = dip->size;
		memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
		brelse(bp);
		ip->valid = 1;
		if (ip->type == 0)
			panic("ilock: no type");
	}
}

// Release exclusive access to inode
void iunlock(struct inode *ip) {
	if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
		panic("iunlock");

	releasesleep(&ip->lock);
}

// Decrement inode reference count
// Frees inode and its data blocks if this was the last reference
// Must be called within a transaction
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

// Release lock and reference in one operation
void iunlockput(struct inode *ip) {
	iunlock(ip);
	iput(ip);
}

// Map logical block number to physical disk block
// Supports direct, single indirect, and double indirect blocks
// Allocates new blocks as needed for writes
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

	if (bn < NINDIRECT) {
		if ((addr = ip->addrs[NDIRECT + 1]) == 0)
			ip->addrs[NDIRECT + 1] = addr = balloc(ip->dev);
		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		if ((addr = a[bn / NINDIRECT]) == 0) {
			a[bn / NINDIRECT] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
		bp = bread(ip->dev, addr);
		a = (uint *)bp->data;
		if ((addr = a[bn % NINDIRECT]) == 0) {
			a[bn % NINDIRECT] = addr = balloc(ip->dev);
			log_write(bp);
		}
		brelse(bp);
		return addr;
	}

	panic("bmap: out of range");
}

// Deallocate all data blocks associated with an inode
// Handles direct, single indirect, and double indirect blocks
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

// Populate stat structure with inode metadata
void stati(struct inode *ip, struct stat *st) {
	st->dev = ip->dev;
	st->ino = ip->inum;
	st->type = ip->type;
	st->nlink = ip->nlink;
	st->size = ip->size;
}

// Read data from inode into buffer
// Caller must hold inode lock
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

// Write data from buffer to inode
// Caller must hold inode lock
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

// Compare directory entry names
int namecmp(const char *s, const char *t) { return strncmp(s, t, DIRSIZ); }

// Search for a directory entry by name
// Returns inode if found, sets offset if poff is non-null
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

// Add a new directory entry to a directory
// Returns -1 if name already exists
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

// Extract next path component from path string
// Returns pointer to remainder of path, copies component to name
// Skips leading and trailing slashes
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", name = "a"
//   skipelem("///a//bb", name) = "bb", name = "a"
//   skipelem("a", name) = "", name = "a"
//   skipelem("", name) = 0
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

// Resolve pathname to inode
// If nameiparent is true, returns parent directory and copies final component
// to name Otherwise returns the target inode
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

// Look up pathname and return locked inode
struct inode *namei(char *path) {
	char name[DIRSIZ];
	return namex(path, 0, name);
}

// Return parent directory and copy final path element to name
struct inode *nameiparent(char *path, char *name) {
	return namex(path, 1, name);
}