#include "buf.h"
#include "defs.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "types.h"
#include "x86.h"
#include "traps.h"

#define SECTOR_SIZE 512
#define IDE_BSY  0x80
#define IDE_DRDY 0x40
#define IDE_DF   0x20
#define IDE_ERR  0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4 // Read Multiple (Efisien untuk BSIZE > 512)
#define IDE_CMD_WRMUL 0xc5 // Write Multiple

static struct spinlock idelock;
static struct buf *idequeue;
static int havedisk1;

// Helper: Menunggu status disk dengan timeout agar kernel tidak freeze
static int idewait(int checkerr) {
    int r;
    for (int i = 0; i < 1000000; i++) {
        r = inb(0x1f7);
        if (!(r & IDE_BSY) && (r & IDE_DRDY)) {
            if (checkerr && (r & (IDE_DF | IDE_ERR)))
                return -1;
            return 0;
        }
    }
    return -1; // Timeout
}

void ideinit(void) {
    initlock(&idelock, "ide");
    ioapicenable(IRQ_IDE, ncpu - 1);
    idewait(0);

    // Cek keberadaan Disk 1 (Secondary Master)
    outb(0x1f6, 0xe0 | (1 << 4));
    for (int i = 0; i < 1000; i++) {
        if (inb(0x1f7) != 0) {
            havedisk1 = 1;
            break;
        }
    }
    outb(0x1f6, 0xe0 | (0 << 4)); // Kembali ke Disk 0
}

// Mulai operasi I/O pada buffer b
static void idestart(struct buf *b) {
    if (b == 0) panic("idestart: null buf");
    if (b->blockno >= FSSIZE) panic("idestart: block out of range");

    int sectors = BSIZE / SECTOR_SIZE; // BSIZE 2048 = 4 sektor
    uint sector = b->blockno * sectors;

    if (idewait(0) < 0) {
        cprintf("ide: disk not ready for block %d\n", b->blockno);
        return;
    }

    // Konfigurasi Control Register
    outb(0x3f6, 0);                // Aktifkan interupsi
    outb(0x1f2, sectors);          // Jumlah sektor per blok
    outb(0x1f3, sector & 0xff);
    outb(0x1f4, (sector >> 8) & 0xff);
    outb(0x1f5, (sector >> 16) & 0xff);
    outb(0x1f6, 0xe0 | ((b->dev & 1) << 4) | ((sector >> 24) & 0x0f));

    if (b->flags & B_DIRTY) {
        outb(0x1f7, (sectors > 1) ? IDE_CMD_WRMUL : IDE_CMD_WRITE);
        outsl(0x1f0, b->data, BSIZE / 4); // Kirim data (PIO)
    } else {
        outb(0x1f7, (sectors > 1) ? IDE_CMD_RDMUL : IDE_CMD_READ);
    }
}

void ideintr(void) {
    struct buf *b;

    acquire(&idelock);
    if ((b = idequeue) == 0) {
        release(&idelock);
        return;
    }
    idequeue = b->qnext;

    // Baca data jika ini adalah operasi Read
    if (!(b->flags & B_DIRTY)) {
        if (idewait(1) >= 0) {
            insl(0x1f0, b->data, BSIZE / 4); // Ambil data dari disk
            b->flags |= B_VALID;
        } else {
            cprintf("ide: read error on block %d\n", b->blockno);
            b->flags &= ~B_VALID; // Tandai gagal
        }
    } else {
        b->flags |= B_VALID; // Write selesai
    }

    b->flags &= ~B_DIRTY;
    wakeup(b); // Bangunkan proses yang menunggu blok ini

    if (idequeue != 0)
        idestart(idequeue);

    release(&idelock);
}

void iderw(struct buf *b) {
    struct buf **pp;

    if (!holdingsleep(&b->lock)) panic("iderw: buf not locked");
    if ((b->flags & (B_VALID | B_DIRTY)) == B_VALID) return;
    if (b->dev != 0 && !havedisk1) panic("iderw: disk 1 missing");

    acquire(&idelock);

    // PENINGKATAN: SSTF (Shortest Seek Time First) Sederhana
    // Menyisipkan buffer ke antrean berdasarkan nomor blok terdekat
    b->qnext = 0;
    for (pp = &idequeue; *pp; pp = &(*pp)->qnext) {
        // Urutkan antrean agar head disk bergerak searah (meminimalkan seek)
        if (b->blockno < (*pp)->blockno)
            break;
    }
    b->qnext = *pp;
    *pp = b;

    if (idequeue == b)
        idestart(b);

    // Tunggu sampai interupsi menandakan I/O selesai
    while ((b->flags & (B_VALID | B_DIRTY)) != B_VALID) {
        sleep(b, &idelock);
    }

    release(&idelock);
}