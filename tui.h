/*
 * tui.h - terminal user interface library
 *
 * Written by Hampus Fridholm
 *
 * Last updated: 2025-03-19
 *
 * This library depends on debug.h
 */

#ifndef TUI_H
#define TUI_H

#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Definitions of keys
 */

#define KEY_CTRLC  3
#define KEY_CTRLZ 26
#define KEY_ESC   27
#define KEY_CTRLS 19
#define KEY_CTRLH  8
#define KEY_CTRLD  4
#define KEY_ENTR  10
#define KEY_TAB    9

/*
 * Declarations of tui structs
 */

typedef struct tui_t tui_t;

typedef struct tui_menu_t tui_menu_t;

typedef struct tui_window_t tui_window_t;

/*
 * Definitions of event function signatures
 */

typedef void (*tui_window_event_t)(tui_window_t* window, int key);

typedef void (*tui_menu_event_t)(tui_menu_t* menu, int key);

typedef void (*tui_event_t)(tui_t* tui, int key);

/*
 * Type of window
 *
 * 1. Parent window contains child windows
 * 2. Text window displays text
 * 3. Input window inputs text
 */
typedef enum tui_window_type_t
{
  TUI_WINDOW_PARENT,
  TUI_WINDOW_TEXT,
  TUI_WINDOW_INPUT
} tui_window_type_t;

/*
 * Normal rect
 */
typedef struct tui_rect_t
{
  int w;
  int h;
  int x;
  int y;
} tui_rect_t;

/*
 * Colors
 */
typedef enum tui_color_t
{
  TUI_COLOR_NONE,
  TUI_COLOR_BLACK,
  TUI_COLOR_RED,
  TUI_COLOR_GREEN,
  TUI_COLOR_YELLOW,
  TUI_COLOR_BLUE,
  TUI_COLOR_MAGENTA,
  TUI_COLOR_CYAN,
  TUI_COLOR_WHITE
} tui_color_t;

/*
 * Border
 */
typedef struct tui_border_t
{
  tui_color_t fg_color;
  tui_color_t bg_color;
} tui_border_t;

/*
 * Type of parent
 */
typedef enum tui_parent_type_t
{
  TUI_PARENT_TUI,
  TUI_PARENT_MENU,
  TUI_PARENT_WINDOW
} tui_parent_type_t;

/*
 * Window struct
 */
typedef struct tui_window_t
{
  tui_window_type_t  type;
  char*              name;
  bool               is_visable;
  bool               is_interact;
  bool               is_locked;
  tui_rect_t         rect;
  WINDOW*            window;
  tui_color_t        fg_color;
  tui_color_t        bg_color;
  tui_border_t*      border;
  tui_window_event_t event;
  tui_parent_type_t  parent_type;
  union
  {
    tui_t*           tui;
    tui_menu_t*      menu;
    tui_window_t*    window;
  }                  parent;
  tui_t*             tui;
} tui_window_t;

/*
 * Position
 */
typedef enum tui_pos_t
{
  TUI_POS_START,
  TUI_POS_CENTER,
  TUI_POS_END
} tui_pos_t;

/*
 * Alignment
 */
typedef enum tui_align_t
{
  TUI_ALIGN_START,
  TUI_ALIGN_CENTER,
  TUI_ALIGN_END,
  TUI_ALIGN_BETWEEN,
  TUI_ALIGN_AROUND,
  TUI_ALIGN_EVENLY
} tui_align_t;

/*
 * Input window struct
 */
typedef struct tui_window_input_t
{
  tui_window_t head;
  char*        buffer;
  size_t       buffer_size;
  size_t       buffer_len;
  size_t       cursor;
  size_t       scroll;
  bool         is_secret;
  bool         is_hidden;
  tui_pos_t    pos;
} tui_window_input_t;

/*
 * Text window struct
 */
typedef struct tui_window_text_t
{
  tui_window_t head;
  char*        string;
  char*        text;
  tui_pos_t    pos;
  tui_align_t  align;
} tui_window_text_t;

/*
 * Parent window struct
 */
typedef struct tui_window_parent_t
{
  tui_window_t   head;
  tui_window_t** children;
  size_t         child_count;
  bool           is_vertical;
  tui_pos_t      pos;
  tui_align_t    align;
} tui_window_parent_t;

/*
 * Menu struct
 */
typedef struct tui_menu_t
{
  char*            name;
  tui_window_t**   windows;
  size_t           window_count;
  tui_menu_event_t event;
  tui_t*           tui;
} tui_menu_t;

/*
 * Tui struct
 */
typedef struct tui_t
{
  int            w;
  int            h;
  tui_menu_t**   menus;
  size_t         menu_count;
  tui_window_t** windows;
  size_t         window_count;
  tui_window_t** tab_windows;
  size_t         tab_window_count;
  tui_menu_t*    menu;
  tui_window_t*  window;
  short          color;
  tui_event_t    event;
  bool           is_running;
} tui_t;

#endif // TUI_H

#ifdef TUI_IMPLEMENT

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"

short TUI_COLORS[] =
{
  -1,
  COLOR_BLACK,
  COLOR_RED,
  COLOR_GREEN,
  COLOR_YELLOW,
  COLOR_BLUE,
  COLOR_MAGENTA,
  COLOR_CYAN,
  COLOR_WHITE
};

/*
 * Turn on forground and background color
 *
 * In case of transparent colors (TUI_COLOR_NONE),
 * the last color is used
 *
 * ncurses colors and tui colors differ by 1
 */
void tui_color_on(tui_t* tui, tui_color_t fg, tui_color_t bg)
{
  short last_fg, last_bg;

  if (pair_content(tui->color, &last_fg, &last_bg) == OK)
  {
    if (fg == TUI_COLOR_NONE)
    {
      fg = last_fg + 1;
    }

    if (bg == TUI_COLOR_NONE)
    {
      bg = last_bg + 1;
    }
  }

  short color = fg * 9 + bg;

  tui->color = color;

  attron(COLOR_PAIR(color));
}

/*
 * Turn off forground and background color
 */
void tui_color_off(tui_t* tui, tui_color_t fg, tui_color_t bg)
{
  short last_fg, last_bg;

  if (pair_content(tui->color, &last_fg, &last_bg) == OK)
  {
    if (fg == TUI_COLOR_NONE)
    {
      fg = last_fg + 1;
    }

    if (bg == TUI_COLOR_NONE)
    {
      bg = last_bg + 1;
    }
  }

  attroff(COLOR_PAIR(fg * 9 + bg));
}

/*
 * Initialize tui colors
 */
void tui_colors_init(void)
{
  for (size_t fg_index = 0; fg_index < 9; fg_index++)
  {
    for (size_t bg_index = 0; bg_index < 9; bg_index++)
    {
      size_t index = fg_index * 9 + bg_index;

      short fg_color = TUI_COLORS[fg_index];
      short bg_color = TUI_COLORS[bg_index];

      init_pair(index, fg_color, bg_color);
    }
  }
}

/*
 * Initialize tui (ncurses)
 */
int tui_init(void)
{
  initscr();
  noecho();
  raw();
  keypad(stdscr, TRUE);

  if (start_color() == ERR || !has_colors())
  {
    endwin();

    return 1;
  }

  use_default_colors();

  tui_colors_init();

  clear();
  refresh();

  return 0;
}

/*
 * Quit tui (ncurses)
 */
void tui_quit(void)
{
  clear();

  refresh();

  endwin();
}

/*
 * Create ncurses WINDOW* for tui_window_t
 */
static inline WINDOW* tui_ncurses_window_create(int w, int h, int x, int y)
{
  WINDOW* window = newwin(h, w, y, x);

  if (!window)
  {
    return NULL;
  }

  keypad(window, TRUE);

  return window;
}

/*
 * Resize ncurses WINDOW*
 */
static inline void tui_ncurses_window_resize(WINDOW* window, int w, int h, int x, int y)
{
  wresize(window, h, w);

  mvwin(window, y, x);
}

/*
 * Delete ncurses WINDOW*
 */
static inline void tui_ncurses_window_free(WINDOW** window)
{
  if (!window || !(*window)) return;

  wclear(*window);

  wrefresh(*window);

  delwin(*window);

  *window = NULL;
}

/*
 * Create tui struct
 */
tui_t* tui_create(tui_event_t event)
{
  tui_t* tui = malloc(sizeof(tui_t));

  if (!tui)
  {
    return NULL;
  }

  memset(tui, 0, sizeof(tui_t));

  *tui = (tui_t)
  {
    .w     = getmaxx(stdscr),
    .h     = getmaxy(stdscr),
    .event = event
  };

  return tui;
}

static inline void tui_windows_free(tui_window_t*** windows, size_t* count);

/*
 * Free parent window struct
 */
static inline void tui_window_parent_free(tui_window_parent_t** window)
{
  tui_windows_free(&(*window)->children, &(*window)->child_count);

  tui_ncurses_window_free(&(*window)->head.window);

  free(*window);

  *window = NULL;
}

/*
 * Free text window struct
 */
static inline void tui_window_text_free(tui_window_text_t** window)
{
  tui_ncurses_window_free(&(*window)->head.window);

  free((*window)->text);

  free(*window);

  *window = NULL;
}

/*
 * Free input window struct
 */
static inline void tui_window_input_free(tui_window_input_t** window)
{
  tui_ncurses_window_free(&(*window)->head.window);

  free(*window);

  *window = NULL;
}

/*
 * Free window struct
 */
static inline void tui_window_free(tui_window_t** window)
{
  if (!window || !(*window)) return;

  switch ((*window)->type)
  {
    case TUI_WINDOW_PARENT:
      tui_window_parent_free((tui_window_parent_t**) window);
      break;

    case TUI_WINDOW_TEXT:
      tui_window_text_free((tui_window_text_t**) window);
      break;

    case TUI_WINDOW_INPUT:
      tui_window_input_free((tui_window_input_t**) window);
      break;

    default:
      break;
  }
}

/*
 * Free windows
 */
static inline void tui_windows_free(tui_window_t*** windows, size_t* count)
{
  for (size_t index = 0; index < *count; index++)
  {
    tui_window_free(&(*windows)[index]);
  }

  free(*windows);

  *windows = NULL;
  *count = 0;
}

/*
 * Free menu struct
 */
static inline void tui_menu_free(tui_menu_t** menu)
{
  if (!menu || !(*menu)) return;

  tui_windows_free(&(*menu)->windows, &(*menu)->window_count);

  free(*menu);

  *menu = NULL;
}

/*
 * Delete tui struct
 */
void tui_delete(tui_t** tui)
{
  if (!tui || !(*tui)) return;

  for (size_t index = 0; index < (*tui)->menu_count; index++)
  {
    tui_menu_free(&(*tui)->menus[index]);
  }

  free((*tui)->menus);

  tui_windows_free(&(*tui)->windows, &(*tui)->window_count);

  free((*tui)->tab_windows);

  free(*tui);

  *tui = NULL;
}

/*
 * Trigger tui events
 */
void tui_event(tui_t* tui, int key)
{

}

void tui_render(tui_t* tui)
{
  curs_set(0);

  refresh();
}

#endif // TUI_IMPLEMENT
