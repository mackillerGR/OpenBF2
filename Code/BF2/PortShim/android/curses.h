// [CHOSEN, not recovered] Android has no terminal for the status monitor, so the screen calls that
// Game/Main/StatusMonitorImpl/posix.cpp makes are inert: no window opens and no key is ever read.
#pragma once

typedef struct AndroidNoWindow WINDOW;

#define ERR (-1)
#define OK 0
#define TRUE 1
#define FALSE 0

static WINDOW* const stdscr = 0;

inline WINDOW* initscr() { return 0; }
inline WINDOW* newwin(int, int, int, int) { return 0; }
inline int delwin(WINDOW*) { return OK; }
inline int endwin() { return OK; }
inline int cbreak() { return OK; }
inline int noecho() { return OK; }
inline int nonl() { return OK; }
inline int keypad(WINDOW*, int) { return OK; }
inline int nodelay(WINDOW*, int) { return OK; }
inline int werase(WINDOW*) { return OK; }
inline int wrefresh(WINDOW*) { return OK; }
inline int wmove(WINDOW*, int, int) { return OK; }
inline int wgetch(WINDOW*) { return ERR; }
inline int waddstr(WINDOW*, const char*) { return OK; }
inline int waddnstr(WINDOW*, const char*, int) { return OK; }
inline int wclrtoeol(WINDOW*) { return OK; }
#define getmaxyx(win, y, x) ((y) = 25, (x) = 80)
#define getmaxy(win) 25
#define getmaxx(win) 80
static WINDOW* const curscr = 0;
inline int wtouchln(WINDOW*, int, int, int) { return OK; }
#define KEY_DOWN 0402
#define KEY_UP 0403
#define KEY_BACKSPACE 0407
#define KEY_NPAGE 0522
#define KEY_PPAGE 0523
#define KEY_ENTER 0527
#define KEY_RESIZE 0632
inline int intrflush(WINDOW*, int) { return OK; }
