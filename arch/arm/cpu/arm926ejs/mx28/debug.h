#ifdef DEBUG
extern void printhex(int data);
extern void printdec(int data);
extern void dprintf(const char *fmt, ...);
#else
#define printhex(d) do { } while (0)
#define printdec(d) do { } while (0)
#define dprintf(fmt...) do { } while (0)
#endif
