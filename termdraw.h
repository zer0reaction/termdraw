/* -------------------------------------------------------------------------------- */
/* Header                                                                           */
/* -------------------------------------------------------------------------------- */

#ifndef TERMDRAW_H_
#define TERMDRAW_H_


#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

extern int  termdraw_initialize(void);
extern void termdraw_destroy(void);

extern int  termdraw_display(void);

extern int  termdraw_get_height(void);
extern int  termdraw_get_width(void);

extern int  termdraw_input_get_char(char *c);

extern int  termdraw_cursor_hide(void);
extern int  termdraw_cursor_show(void);

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

int termdraw_initialize(void)
{
    {
        struct termios raw_mode_settings = {0};

        if (tcgetattr(STDIN_FILENO, &termdraw__original_settings) == -1)
        {
            return -1;
        }

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
        termdraw__output_buf.data =
            malloc(termdraw__output_buf.capacity *
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

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &termdraw__original_settings);
}

int termdraw__output_buf_append(const char *buf, size_t size)
{
    if (termdraw__output_buf.used + size >
        termdraw__output_buf.capacity)
    {
        while (termdraw__output_buf.used + size >
               termdraw__output_buf.capacity)
        {
            termdraw__output_buf.capacity *= 2;
        }

        termdraw__output_buf.data =
            realloc(termdraw__output_buf.data,
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

int termdraw_get_height(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws))
    {
        return -1;
    }

    return ws.ws_row;
}

int termdraw_get_width(void)
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
    return termdraw__output_buf_append("\033[?25l", 6);
}

int termdraw_cursor_show(void)
{
    return termdraw__output_buf_append("\033[?25h", 6);
}


#endif /* TERMDRAW_IMPLEMENTATION */
