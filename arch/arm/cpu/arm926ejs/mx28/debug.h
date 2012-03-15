#ifdef DEBUG
extern void printhex(int data);
extern void printdec(int data);
extern void dprintf(const char *fmt, ...);
static int debug = 1;
#define dbg_lvl(lvl)		(debug > (lvl))
#define dbg(lvl, fmt...)	do { if (dbg_lvl(lvl)) dprintf(fmt); } while (0)
#else
#define dbg_lvl(lvl)		0
#define printhex(d)		do { } while (0)
#define printdec(d)		do { } while (0)
#define dprintf(lvl, fmt...)	do { } while (0)
#endif
