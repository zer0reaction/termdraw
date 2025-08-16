/* -------------------------------------------------------------------------------- */
/* Header                                                                           */
/* -------------------------------------------------------------------------------- */

#ifndef TERMDRAW_H_
#define TERMDRAW_H_


#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <assert.h>
#include <string.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

#define TERMDRAW_MODE_BOLD          1
#define TERMDRAW_MODE_DIM           2
#define TERMDRAW_MODE_ITALIC        3
#define TERMDRAW_MODE_UNDERLINE     4
#define TERMDRAW_MODE_BLINKING      5
#define TERMDRAW_MODE_REVERSE       7
#define TERMDRAW_MODE_INVISIBLE     8
#define TERMDRAW_MODE_STRIKETHROUGH 9

#define TERMDRAW_MODE_FG_BLACK   30
#define TERMDRAW_MODE_FG_RED     31
#define TERMDRAW_MODE_FG_GREEN   32
#define TERMDRAW_MODE_FG_YELLOW  33
#define TERMDRAW_MODE_FG_BLUE    34
#define TERMDRAW_MODE_FG_MAGENTA 35
#define TERMDRAW_MODE_FG_CYAN    36
#define TERMDRAW_MODE_FG_WHITE   37
#define TERMDRAW_MODE_FG_DEFAULT 39

#define TERMDRAW_MODE_BG_BLACK   40
#define TERMDRAW_MODE_BG_RED     41
#define TERMDRAW_MODE_BG_GREEN   42
#define TERMDRAW_MODE_BG_YELLOW  43
#define TERMDRAW_MODE_BG_BLUE    44
#define TERMDRAW_MODE_BG_MAGENTA 45
#define TERMDRAW_MODE_BG_CYAN    46
#define TERMDRAW_MODE_BG_WHITE   47
#define TERMDRAW_MODE_BG_DEFAULT 49

#define TERMDRAW_MODE_FG_BRIGHT_BLACK   90
#define TERMDRAW_MODE_FG_BRIGHT_RED     91
#define TERMDRAW_MODE_FG_BRIGHT_GREEN   92
#define TERMDRAW_MODE_FG_BRIGHT_YELLOW  93
#define TERMDRAW_MODE_FG_BRIGHT_BLUE    94
#define TERMDRAW_MODE_FG_BRIGHT_MAGENTA 95
#define TERMDRAW_MODE_FG_BRIGHT_CYAN    96
#define TERMDRAW_MODE_FG_BRIGHT_WHITE   97

#define TERMDRAW_MODE_BG_BRIGHT_BLACK   100
#define TERMDRAW_MODE_BG_BRIGHT_RED     101
#define TERMDRAW_MODE_BG_BRIGHT_GREEN   102
#define TERMDRAW_MODE_BG_BRIGHT_YELLOW  103
#define TERMDRAW_MODE_BG_BRIGHT_BLUE    104
#define TERMDRAW_MODE_BG_BRIGHT_MAGENTA 105
#define TERMDRAW_MODE_BG_BRIGHT_CYAN    106
#define TERMDRAW_MODE_BG_BRIGHT_WHITE   107

typedef uint32_t termdraw_rune_t;

typedef struct
{
    bool raw_mode;
} termdraw__options_initialize_t;

extern int  termdraw__initialize(termdraw__options_initialize_t options);

#define     termdraw_initialize(...) termdraw__initialize((termdraw__options_initialize_t){ __VA_ARGS__ })
extern void termdraw_destroy(void);

extern int  termdraw_display(void);

extern int  termdraw_get_height(void);
extern int  termdraw_get_width(void);

extern int  termdraw_input_get_char(char *c);

extern int  termdraw_cursor_hide(void);
extern int  termdraw_cursor_show(void);
extern int  termdraw_cursor_get_row(void);
extern int  termdraw_cursor_get_col(void);
extern int  termdraw_cursor_move(unsigned row, unsigned col);

extern int  termdraw_add_rune(termdraw_rune_t rune);

extern int  termdraw_mode_set(unsigned mode);
extern int  termdraw_mode_reset(void);

#endif /* TERMDRAW_H_ */



/* -------------------------------------------------------------------------------- */
/* Implementation                                                                   */
/* -------------------------------------------------------------------------------- */

#ifdef TERMDRAW_IMPLEMENTATION


static struct
{
    char   *data;
    size_t  used;
    size_t  capacity;
} termdraw__output_buf =
{
    .data     = NULL,
    .used     = 0,
    .capacity = 1024            /* NOTE: initial capacity */
};

static struct termios termdraw__original_settings = {0};

/* NOTE: row and col start from 1 */
static unsigned termdraw__cursor_row = -1;
static unsigned termdraw__cursor_col = -1;

int termdraw__initialize(termdraw__options_initialize_t options)
{
    if (tcgetattr(STDIN_FILENO, &termdraw__original_settings) == -1)
    {
        return -1;
    }

    if (options.raw_mode)
    {
        struct termios raw_mode_settings = {0};

        raw_mode_settings = termdraw__original_settings;

        /* Scary! */
        raw_mode_settings.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
        raw_mode_settings.c_iflag &= ~(IXON) | ICRNL;
        raw_mode_settings.c_oflag &= ~(OPOST);
        raw_mode_settings.c_cc[VMIN] = 1;
        raw_mode_settings.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode_settings) == -1)
        {
            return -1;
        }
    }

    {
        termdraw__output_buf.data = malloc(termdraw__output_buf.capacity *
                                           sizeof *termdraw__output_buf.data);

        if (termdraw__output_buf.data == NULL)
        {
            return -1;
        }
    }

    return 0;
}

void termdraw_destroy(void)
{
    free(termdraw__output_buf.data);
    termdraw__output_buf.used = 0;
    termdraw__output_buf.capacity = 0;

    (void)tcsetattr(STDIN_FILENO,
                    TCSAFLUSH,
                    &termdraw__original_settings);
}

int termdraw__output_buf_append_str(const char *buf, size_t size)
{
    if (termdraw__output_buf.used + size >
        termdraw__output_buf.capacity)
    {
        while (termdraw__output_buf.used + size >
               termdraw__output_buf.capacity)
        {
            termdraw__output_buf.capacity *= 2;
        }

        termdraw__output_buf.data = realloc(termdraw__output_buf.data,
                                            termdraw__output_buf.capacity *
                                            sizeof(*termdraw__output_buf.data));

        if (termdraw__output_buf.data == NULL)
        {
            return -1;
        }
    }

    (void)memcpy(&termdraw__output_buf.data[termdraw__output_buf.used],
                 buf, size);

    termdraw__output_buf.used += size;
    return 0;
}

int termdraw__output_buf_append_uint(unsigned n)
{
    unsigned i = 0;
    unsigned len = 0;
    char buf[32] = {0};

    while (n > 0)
    {
        buf[len] = '0' + n % 10;
        len++;
        n /= 10;
    }

    for (i = 0; i < len / 2; i++)
    {
        char temp = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = temp;
    }

    return termdraw__output_buf_append_str(buf, len);
}

int termdraw__output_buf_flush(void)
{
    if (termdraw__output_buf.used == 0)
    {
        return 0;
    }

    {
        size_t written = 0;

        written = write(STDOUT_FILENO,
                        termdraw__output_buf.data,
                        termdraw__output_buf.used);

        if (written != termdraw__output_buf.used)
        {
            return -1;
        }

        termdraw__output_buf.used = 0;

        return 0;
    }
}

int termdraw_display(void)
{
    if (termdraw__output_buf_flush() != 0)
    {
        return -1;
    }

    return 0;
}

int termdraw_get_rows(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws))
    {
        return -1;
    }

    return ws.ws_row;
}

int termdraw_get_cols(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws))
    {
        return -1;
    }

    return ws.ws_col;
}

int termdraw_input_get_char(char *c)
{
    if (read(STDIN_FILENO, c, 1) != 1)
    {
        return -1;
    }

    return 0;
}

int termdraw_cursor_hide(void)
{
    return termdraw__output_buf_append_str("\033[?25l", 6);
}

int termdraw_cursor_show(void)
{
    return termdraw__output_buf_append_str("\033[?25h", 6);
}

int termdraw_cursor_move(unsigned row, unsigned col)
{
    int row_count, col_count;
    bool in_bounds = false;

    if (row == termdraw__cursor_row &&
        col == termdraw__cursor_col)
    {
        return 0;
    }

    row_count = termdraw_get_rows();
    col_count = termdraw_get_cols();
    in_bounds = (row <= (unsigned)row_count &&
                 col <= (unsigned)col_count);

    if (row_count == -1 || col_count == -1 || !in_bounds)
    {
        return -1;
    }

    /* ESC[{line};{column}H */

    /* TODO: add termdraw__output_buf_append_fmt() */
    if (termdraw__output_buf_append_str("\033[", 2) == -1 ||
        termdraw__output_buf_append_uint(row)       == -1 ||
        termdraw__output_buf_append_str(";", 1)     == -1 ||
        termdraw__output_buf_append_uint(col)       == -1 ||
        termdraw__output_buf_append_str("H", 1)     == -1)
    {
        return -1;
    }

    termdraw__cursor_row = row;
    termdraw__cursor_col = col;

    return 0;
}

int termdraw_cursor_get_row(void)
{
    return termdraw__cursor_row;
}

int termdraw_cursor_get_col(void)
{
    return termdraw__cursor_col;
}

int termdraw_add_rune(termdraw_rune_t rune)
{
    if (rune > 127)
    {
        assert(0 && "currently supporting only ASCII characters");
    }

    {
        char c = (char)rune;
        return termdraw__output_buf_append_str(&c, 1);
    }
}

int termdraw_mode_reset(void)
{
    return termdraw__output_buf_append_str("\033[0m", 4);
}

/* TODO: add support for | */
int termdraw_mode_set(unsigned mode)
{
    if (termdraw__output_buf_append_str("\033[", 2) == -1 ||
        termdraw__output_buf_append_uint(mode)      == -1 ||
        termdraw__output_buf_append_str("m", 1)     == -1)
    {
        return -1;
    }

    return 0;
}


#endif /* TERMDRAW_IMPLEMENTATION */
