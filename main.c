// main.c
#define RGFW_IMPLEMENTATION
#define RGFW_WAYLAND
#include "RGFW.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------
   Editor configuration & types
   ---------------------------- */
#define SCALE 3
#define GLYPH_W 8
#define GLYPH_H 8
#define MARGIN 8

typedef unsigned char u8;

typedef struct {
  char **lines;
  size_t line_count;
} TextBuffer;

/* ----------------------------
   TextBuffer helpers
   ---------------------------- */
static void tb_init(TextBuffer *tb) {
  tb->lines = NULL;
  tb->line_count = 0;
}
static void tb_free(TextBuffer *tb) {
  if (!tb)
    return;
  for (size_t i = 0; i < tb->line_count; ++i)
    free(tb->lines[i]);
  free(tb->lines);
  tb->lines = NULL;
  tb->line_count = 0;
}
static void tb_append_line(TextBuffer *tb, const char *s) {
  tb->lines =
      (char **)realloc(tb->lines, (tb->line_count + 1) * sizeof(char *));
  tb->lines[tb->line_count] = strdup(s ? s : "");
  tb->line_count++;
}
static void tb_insert_char(TextBuffer *tb, size_t row, size_t col, char c) {
  if (row >= tb->line_count) {
    tb_append_line(tb, "");
  }
  char *line = tb->lines[row];
  size_t len = strlen(line);
  if (col > len)
    col = len;
  char *n = (char *)malloc(len + 2);
  memcpy(n, line, col);
  n[col] = c;
  memcpy(n + col + 1, line + col, len - col);
  n[len + 1] = '\0';
  free(tb->lines[row]);
  tb->lines[row] = n;
}
static void tb_backspace(TextBuffer *tb, size_t *row, size_t *col) {
  if (*row >= tb->line_count)
    return;
  if (*col > 0) {
    char *line = tb->lines[*row];
    size_t len = strlen(line);
    if (len == 0)
      return;
    memmove(line + (*col - 1), line + (*col), len - *col + 1);
    *col -= 1;
  } else if (*row > 0) {
    size_t prev_len = strlen(tb->lines[*row - 1]);
    size_t cur_len = strlen(tb->lines[*row]);
    tb->lines[*row - 1] =
        (char *)realloc(tb->lines[*row - 1], prev_len + cur_len + 1);
    memcpy(tb->lines[*row - 1] + prev_len, tb->lines[*row], cur_len + 1);
    free(tb->lines[*row]);
    for (size_t i = *row + 1; i < tb->line_count; ++i)
      tb->lines[i - 1] = tb->lines[i];
    tb->line_count--;
    if (tb->line_count == 0) {
      free(tb->lines);
      tb->lines = NULL;
    } else {
      tb->lines = (char **)realloc(tb->lines, tb->line_count * sizeof(char *));
    }
    *row -= 1;
    *col = prev_len;
  }
}
static void tb_insert_newline(TextBuffer *tb, size_t *row, size_t *col) {
  if (*row >= tb->line_count) {
    tb_append_line(tb, "");
    *row = tb->line_count - 1;
    *col = 0;
    return;
  }
  char *line = tb->lines[*row];
  size_t len = strlen(line);
  size_t after = len - *col;
  char *new_cur = (char *)malloc((*col) + 1);
  memcpy(new_cur, line, *col);
  new_cur[*col] = '\0';
  char *new_next = (char *)malloc(after + 1);
  memcpy(new_next, line + *col, after);
  new_next[after] = '\0';
  free(tb->lines[*row]);
  tb->lines[*row] = new_cur;
  tb->lines =
      (char **)realloc(tb->lines, (tb->line_count + 1) * sizeof(char *));
  for (size_t i = tb->line_count; i > *row + 1; --i)
    tb->lines[i] = tb->lines[i - 1];
  tb->lines[*row + 1] = new_next;
  tb->line_count++;
  *row += 1;
  *col = 0;
}

/* ----------------------------
   Embedded 8x8 bitmap font
   ---------------------------- */
/* ASCII 0x20..0x7F (96 entries) */
static const u8 font8x8_basic[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 20 ' '
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // 21 '!'
    {0x6C, 0x6C, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00}, // 22 '"'
    {0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00}, // 23 '#'
    {0x10, 0x7C, 0xD0, 0x7C, 0x16, 0xFC, 0x10, 0x00}, // 24 '$'
    {0x00, 0xC6, 0xCC, 0x18, 0x30, 0x66, 0xC6, 0x00}, // 25 '%'
    {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00}, // 26 '&'
    {0x30, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00}, // 27 '''
    {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00}, // 28 '('
    {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00}, // 29 ')'
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // 2A '*'
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, // 2B '+'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30}, // 2C ','
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, // 2D '-'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // 2E '.'
    {0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00}, // 2F '/'
    {0x7C, 0xC6, 0xCE, 0xD6, 0xE6, 0xC6, 0x7C, 0x00}, // 30 '0'
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // 31 '1'
    {0x7C, 0xC6, 0x06, 0x1C, 0x30, 0x66, 0xFE, 0x00}, // 32 '2'
    {0x7C, 0xC6, 0x06, 0x3C, 0x06, 0xC6, 0x7C, 0x00}, // 33 '3'
    {0x0C, 0x1C, 0x3C, 0x6C, 0xFE, 0x0C, 0x0C, 0x00}, // 34 '4'
    {0xFE, 0xC0, 0xFC, 0x06, 0x06, 0xC6, 0x7C, 0x00}, // 35 '5'
    {0x3C, 0x60, 0xC0, 0xFC, 0xC6, 0xC6, 0x7C, 0x00}, // 36 '6'
    {0xFE, 0xC6, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00}, // 37 '7'
    {0x7C, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0x7C, 0x00}, // 38 '8'
    {0x7C, 0xC6, 0xC6, 0x7E, 0x06, 0x0C, 0x78, 0x00}, // 39 '9'
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00}, // 3A ':'
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30}, // 3B ';'
    {0x0E, 0x1C, 0x38, 0x70, 0x38, 0x1C, 0x0E, 0x00}, // 3C '<'
    {0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00}, // 3D '='
    {0x70, 0x38, 0x1C, 0x0E, 0x1C, 0x38, 0x70, 0x00}, // 3E '>'
    {0x7C, 0xC6, 0x0C, 0x18, 0x18, 0x00, 0x18, 0x00}, // 3F '?'
    {0x7C, 0xC6, 0xDE, 0xDE, 0xDE, 0xC0, 0x78, 0x00}, // 40 '@'
    {0x38, 0x6C, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00}, // 41 'A'
    {0xFC, 0x66, 0x66, 0x7C, 0x66, 0x66, 0xFC, 0x00}, // 42 'B'
    {0x3C, 0x66, 0xC0, 0xC0, 0xC0, 0x66, 0x3C, 0x00}, // 43 'C'
    {0xF8, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00}, // 44 'D'
    {0xFE, 0x62, 0x68, 0x78, 0x68, 0x62, 0xFE, 0x00}, // 45 'E'
    {0xFE, 0x62, 0x68, 0x78, 0x68, 0x60, 0xF0, 0x00}, // 46 'F'
    {0x3C, 0x66, 0xC0, 0xC0, 0xCE, 0x66, 0x3E, 0x00}, // 47 'G'
    {0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00}, // 48 'H'
    {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 49 'I'
    {0x1E, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78, 0x00}, // 4A 'J'
    {0xE6, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0xE6, 0x00}, // 4B 'K'
    {0xF0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xFE, 0x00}, // 4C 'L'
    {0xC6, 0xEE, 0xFE, 0xFE, 0xD6, 0xC6, 0xC6, 0x00}, // 4D 'M'
    {0xC6, 0xE6, 0xF6, 0xDE, 0xCE, 0xC6, 0xC6, 0x00}, // 4E 'N'
    {0x38, 0x6C, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x00}, // 4F 'O'
    {0xFC, 0x66, 0x66, 0x7C, 0x60, 0x60, 0xF0, 0x00}, // 50 'P'
    {0x38, 0x6C, 0xC6, 0xC6, 0xDA, 0xCC, 0x76, 0x00}, // 51 'Q'
    {0xFC, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0xE6, 0x00}, // 52 'R'
    {0x7C, 0xC6, 0xE0, 0x7C, 0x1E, 0xC6, 0x7C, 0x00}, // 53 'S'
    {0xFF, 0xDB, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 54 'T'
    {0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00}, // 55 'U'
    {0xC6, 0xC6, 0xC6, 0x6C, 0x6C, 0x38, 0x10, 0x00}, // 56 'V'
    {0xC6, 0xC6, 0xD6, 0xFE, 0xFE, 0xEE, 0xC6, 0x00}, // 57 'W'
    {0xC6, 0x6C, 0x38, 0x10, 0x38, 0x6C, 0xC6, 0x00}, // 58 'X'
    {0xCC, 0xCC, 0x78, 0x30, 0x30, 0x30, 0x78, 0x00}, // 59 'Y'
    {0xFE, 0xCE, 0x9C, 0x38, 0x70, 0xE6, 0xFE, 0x00}, // 5A 'Z'
    {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00}, // 5B '['
    {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00}, // 5C '\'
    {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00}, // 5D ']'
    {0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00}, // 5E '^'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // 5F '_'
    {0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00}, // 60 '`'
    {0x00, 0x00, 0x78, 0x0C, 0x7C, 0xCC, 0x76, 0x00}, // 61 'a'
    {0xE0, 0x60, 0x7C, 0x66, 0x66, 0x66, 0xDC, 0x00}, // 62 'b'
    {0x00, 0x00, 0x7C, 0xC6, 0xC0, 0xC6, 0x7C, 0x00}, // 63 'c'
    {0x1C, 0x0C, 0x7C, 0xCC, 0xCC, 0xCC, 0x76, 0x00}, // 64 'd'
    {0x00, 0x00, 0x7C, 0xC6, 0xFE, 0xC0, 0x7C, 0x00}, // 65 'e'
    {0x38, 0x6C, 0x60, 0xF8, 0x60, 0x60, 0xF0, 0x00}, // 66 'f'
    {0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0xF8}, // 67 'g'
    {0xE0, 0x60, 0x6C, 0x76, 0x66, 0x66, 0xE6, 0x00}, // 68 'h'
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 69 'i'
    {0x0C, 0x00, 0x1C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78}, // 6A 'j'
    {0xE0, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0xE6, 0x00}, // 6B 'k'
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 6C 'l'
    {0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xD6, 0xC6, 0x00}, // 6D 'm'
    {0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x00}, // 6E 'n'
    {0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x00}, // 6F 'o'
    {0x00, 0x00, 0xDC, 0x66, 0x66, 0x7C, 0x60, 0xF0}, // 70 'p'
    {0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0x1E}, // 71 'q'
    {0x00, 0x00, 0xDC, 0x76, 0x66, 0x60, 0xF0, 0x00}, // 72 'r'
    {0x00, 0x00, 0x7E, 0xC0, 0x7C, 0x06, 0xFC, 0x00}, // 73 's'
    {0x30, 0x30, 0xFC, 0x30, 0x30, 0x34, 0x18, 0x00}, // 74 't'
    {0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00}, // 75 'u'
    {0x00, 0x00, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00}, // 76 'v'
    {0x00, 0x00, 0xC6, 0xD6, 0xFE, 0xEE, 0x6C, 0x00}, // 77 'w'
    {0x00, 0x00, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0x00}, // 78 'x'
    {0x00, 0x00, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0xFC}, // 79 'y'
    {0x00, 0x00, 0xFE, 0x8C, 0x18, 0x32, 0xFE, 0x00}, // 7A 'z'
    {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00}, // 7B '{'
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // 7C '|'
    {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00}, // 7D '}'
    {0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 7E '~'
    {0x00, 0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00}  // 7F DEL (as small glyph)
};

/* ----------------------------
   Rendering helpers
   ---------------------------- */
static void clear_buffer(u8 *buffer, int w, int h, u8 r, u8 g, u8 b) {
  for (int i = 0; i < w * h; ++i) {
    buffer[i * 3 + 0] = r;
    buffer[i * 3 + 1] = g;
    buffer[i * 3 + 2] = b;
  }
}

static void render_glyph(u8 *buffer, int buf_w, int buf_h, int x, int y,
                         unsigned char ch, int scale, u8 r, u8 g, u8 b) {
  if (ch < 32 || ch > 127)
    ch = '?';
  const u8 *glyph = font8x8_basic[ch - 32];
  for (int gy = 0; gy < 8; ++gy) {
    u8 row = glyph[gy];
    for (int gx = 0; gx < 8; ++gx) {
      int bit = (row >> (7 - gx)) & 1;
      if (!bit)
        continue;
      for (int sy = 0; sy < scale; ++sy) {
        for (int sx = 0; sx < scale; ++sx) {
          int px = x + gx * scale + sx;
          int py = y + gy * scale + sy;
          if (px < 0 || py < 0 || px >= buf_w || py >= buf_h)
            continue;
          int idx = (py * buf_w + px) * 3;
          buffer[idx + 0] = r;
          buffer[idx + 1] = g;
          buffer[idx + 2] = b;
        }
      }
    }
  }
}

/* ----------------------------
   Editor globals for callback
   ---------------------------- */
static TextBuffer *G_tb = NULL;
static size_t *G_cx = NULL;
static size_t *G_cy = NULL;

/* Save helper (used by the callback) */
static void save_buffer_to_file(void) {
  if (!G_tb)
    return;
  FILE *f = fopen("out.txt", "w");
  if (!f)
    return;
  for (size_t i = 0; i < G_tb->line_count; ++i) {
    fputs(G_tb->lines[i], f);
    fputc('\n', f);
  }
  fclose(f);
  tb_append_line(G_tb, "Saved to out.txt");
}

/* ----------------------------
   Key callback
   ---------------------------- */
/* Signature: (window, key enum, keyChar ASCII-like, keyMod flags, repeat,
 * pressed) */
void editor_key_callback(RGFW_window *win, RGFW_key key, u8 keyChar,
                         RGFW_keymod keyMod, RGFW_bool repeat,
                         RGFW_bool pressed) {
  RGFW_UNUSED(repeat);
  if (!pressed)
    return; // we only handle press events (repeats have pressed==true)

  /* reliable modifier check: callback gives keyMod, but poll physical state too
   */
  int ctrl_down = ((keyMod & RGFW_modControl) != 0);

  /* Ctrl+S => save */
  if (ctrl_down && (keyChar == 's' || keyChar == 'S')) {
    save_buffer_to_file();
    return;
  }

  /* Navigation / edit keys using key enum */
  switch (key) {
  case RGFW_backSpace:
    if (G_tb && G_cx && G_cy)
      tb_backspace(G_tb, G_cy, G_cx);
    return;
  case RGFW_enter:
    if (G_tb && G_cx && G_cy)
      tb_insert_newline(G_tb, G_cy, G_cx);
    return;
  case RGFW_left:
    if (G_cx && G_cy && G_tb) {
      if (*G_cx > 0)
        (*G_cx)--;
      else if (*G_cy > 0) {
        (*G_cy)--;
        *G_cx = strlen(G_tb->lines[*G_cy]);
      }
    }
    return;
  case RGFW_right:
    if (G_cx && G_cy && G_tb) {
      if (*G_cx < strlen(G_tb->lines[*G_cy]))
        (*G_cx)++;
      else if (*G_cy + 1 < G_tb->line_count) {
        (*G_cy)++;
        *G_cx = 0;
      }
    }
    return;
  case RGFW_up:
    if (G_cx && G_cy && G_tb) {
      if (*G_cy > 0) {
        (*G_cy)--;
        if (*G_cx > strlen(G_tb->lines[*G_cy]))
          *G_cx = strlen(G_tb->lines[*G_cy]);
      }
    }
    return;
  case RGFW_down:
    if (G_cx && G_cy && G_tb) {
      if (*G_cy + 1 < G_tb->line_count) {
        (*G_cy)++;
        if (*G_cx > strlen(G_tb->lines[*G_cy]))
          *G_cx = strlen(G_tb->lines[*G_cy]);
      }
    }
    return;
  /*case RGFW_escape:
    RGFW_window_setShouldClose(win, RGFW_TRUE);
    return;*/
  default:
    break;
  }

  /* Printable characters (use keyChar for ASCII-like) - do not insert when Ctrl
   * down */
  if (!ctrl_down && keyChar >= 32 && keyChar <= 126) {
    if (G_tb && G_cx && G_cy) {
      tb_insert_char(G_tb, *G_cy, *G_cx, (char)keyChar);
      (*G_cx)++;
    }
  }
}

/* ----------------------------
   main()
   ---------------------------- */
int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  RGFW_window *win = RGFW_createWindow("RGFW Text Editor (8x8 font)", 100, 100,
                                       1280, 720, (u64)0);
  RGFW_window_setExitKey(win, RGFW_escape);

  int win_w = 800, win_h = 600;
  RGFW_window_getSize(win, &win_w, &win_h);

  size_t buf_size = (size_t)win_w * (size_t)win_h * 3;
  u8 *pixels = (u8 *)RGFW_ALLOC(buf_size);
  RGFW_surface *surface =
      RGFW_createSurface(pixels, win_w, win_h, RGFW_formatRGB8);

  TextBuffer tb;
  tb_init(&tb);
  size_t cx = 0, cy = 0; // cursor (col, row)

  /* register globals and callback */
  G_tb = &tb;
  G_cx = &cx;
  G_cy = &cy;
  RGFW_setKeyCallback(editor_key_callback);

  RGFW_event e;
  while (RGFW_window_shouldClose(win) == RGFW_FALSE) {
    while (RGFW_window_checkEvent(win, &e)) {
      /* handle non-key events here (keys handled via callback) */
      if (e.type == RGFW_quit) {
        RGFW_window_close(win);
        break;
      }
      /* keep mouse or other handlers as needed */
    }

    /* render */
    clear_buffer(pixels, win_w, win_h, 18, 18, 18);
    int x = MARGIN, y = MARGIN;
    int char_w = GLYPH_W * SCALE;
    int char_h = GLYPH_H * SCALE;

    for (size_t row = 0; row < tb.line_count; ++row) {
      const char *line = tb.lines[row];
      x = MARGIN;
      for (size_t col = 0; col < strlen(line); ++col) {
        unsigned char ch = (unsigned char)line[col];
        render_glyph(pixels, win_w, win_h, x, y, ch, SCALE, 230, 230, 230);
        x += char_w + 1;
        if (x + char_w + MARGIN > win_w)
          break;
      }
      /* cursor */
      if (row == cy) {
        int cx_pos = MARGIN + (int)cx * (char_w + 1);
        for (int yy = y; yy < y + char_h; ++yy) {
          for (int cc = 0; cc < 2; ++cc) {
            int px = cx_pos + cc;
            if (px >= 0 && px < win_w && yy >= 0 && yy < win_h) {
              int idx = (yy * win_w + px) * 3;
              pixels[idx + 0] = 255;
              pixels[idx + 1] = 200;
              pixels[idx + 2] = 60;
            }
          }
        }
      }
      y += char_h + 2;
      if (y + char_h + MARGIN > win_h)
        break;
    }

    RGFW_window_blitSurface(win, surface);
  }

  tb_free(&tb);
  RGFW_surface_free(surface);
  RGFW_FREE(pixels);
  RGFW_window_close(win);
  return 0;
}
