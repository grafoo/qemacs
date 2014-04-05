/*
 * HTML Source mode for QEmacs.
 *
 * Copyright (c) 2000-2014 Charlie Gordon.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "qe.h"

static int get_html_entity(unsigned int *p)
{
    unsigned int *p_start, c;

    p_start = p;
    c = (u8)*p;

    if (c != '&')
        return 0;

    p++;
    c = (u8)*p;

    if (c == '#') {
        do {
            p++;
            c = (u8)*p;
        } while (qe_isdigit(c));
    } else
    if (qe_isalpha(c)) {
        do {
            p++;
            c = (u8)*p;
        } while (qe_isalnum(c));
    } else {
        /* not an entity */
        return 0;
    }
    if (c == ';') {
        p++;
    }
    return p - p_start;
}

/* color colorization states */
enum {
    IN_HTML_COMMENT   = 0x01,      /* <!-- <> --> */
    IN_HTML_COMMENT1  = 0x02,      /* <! ... > */
    IN_HTML_STRING    = 0x04,      /* " ... " */
    IN_HTML_STRING1   = 0x08,      /* ' ... ' */
    IN_HTML_TAG       = 0x10,      /* <tag ... > */
    IN_HTML_ENTITY    = 0x20,      /* &name[;] / &#123[;] */
    IN_HTML_SCRIPTTAG = 0x40,      /* <SCRIPT ...> */
    IN_HTML_SCRIPT    = 0x80,      /* <SCRIPT> [...] </SCRIPT> */
};

enum {
    HTML_STYLE_PREPROCESS = QE_STYLE_PREPROCESS,
    HTML_STYLE_SCRIPT     = QE_STYLE_HTML_SCRIPT,
    HTML_STYLE_COMMENT    = QE_STYLE_HTML_COMMENT,
    HTML_TYLE_ENTITY      = QE_STYLE_HTML_ENTITY,
    HTML_STYLE_STRING     = QE_STYLE_HTML_STRING,
    HTML_STYLE_TAG        = QE_STYLE_HTML_TAG,
    //HTML_STYLE_TEXT       = QE_STYLE_HTML_TEXT,
};

void htmlsrc_colorize_line(QEColorizeContext *cp,
                           unsigned int *str, int n, int mode_flags)
{
    int i = 0, start = i, c, len;
    int state = cp->colorize_state;

    /* Kludge for preprocessed html */
    if (str[i] == '#') {
        i = n;
        SET_COLOR(str, start, i, HTML_STYLE_PREPROCESS);
        goto the_end;
    }

    while (i < n) {
        start = i;
        c = str[i];

        if (state & IN_HTML_SCRIPTTAG) {
            while (i < n) {
                if (str[i++] == '>') {
                    state = IN_HTML_SCRIPT;
                    break;
                }
            }
            SET_COLOR(str, start, i, HTML_STYLE_SCRIPT);
            continue;
        }
        if (state & IN_HTML_SCRIPT) {
            for (; i < n; i++) {
                if (str[i] == '<' && ustristart(str + i, "</script>", NULL))
                    break;
            }
            state &= ~IN_HTML_SCRIPT;
            cp->colorize_state = state;
            c = str[i];     /* save char to set '\0' delimiter */
            str[i] = '\0';
            /* XXX: should have js_colorize_func */
            c_colorize_line(cp, str + start, i - start,
                            CLANG_JS | CLANG_REGEX);
            str[i] = c;
            state = cp->colorize_state;
            state |= IN_HTML_SCRIPT;
            if (i < n) {
                start = i;
                i += strlen("</script>");
                state = 0;
                SET_COLOR(str, start, i, HTML_STYLE_SCRIPT);
            }
            continue;
        }
        if (state & IN_HTML_COMMENT) {
            for (; i < n; i++) {
                if (str[i] == '-' && str[i + 1] == '-' && str[i + 2] == '>') {
                    i += 3;
                    state &= ~(IN_HTML_COMMENT | IN_HTML_COMMENT1);
                    break;
                }
            }
            SET_COLOR(str, start, i, HTML_STYLE_COMMENT);
            continue;
        }
        if (state & IN_HTML_COMMENT1) {
            for (; i < n; i++) {
                if (str[i] == '>') {
                    i++;
                    state &= ~IN_HTML_COMMENT1;
                    break;
                }
            }
            SET_COLOR(str, start, i, HTML_STYLE_COMMENT);
            continue;
        }
        if (state & IN_HTML_ENTITY) {
            if ((len = get_html_entity(str + i)) == 0)
                i++;
            else
                i += len;
            state &= ~IN_HTML_ENTITY;
            SET_COLOR(str, start, i, HTML_TYLE_ENTITY);
            continue;
        }
        if (state & (IN_HTML_STRING | IN_HTML_STRING1)) {
            int delim = (state & IN_HTML_STRING1) ? '\'' : '\"';

            for (; i < n; i++) {
                if (str[i] == '&' && get_html_entity(str + i)) {
                    state |= IN_HTML_ENTITY;
                    break;
                }
                if (str[i] == delim) {
                    i++;
                    state &= ~(IN_HTML_STRING | IN_HTML_STRING1);
                    break;
                }
                /* Premature end of string */
                if (str[i] == '>') {
                    state &= ~(IN_HTML_STRING | IN_HTML_STRING1);
                    break;
                }
            }
            SET_COLOR(str, start, i, HTML_STYLE_STRING);
            continue;
        }
        if (state & IN_HTML_TAG) {
            for (; i < n; i++) {
                if (str[i] == '&' && get_html_entity(str + i)) {
                    state |= IN_HTML_ENTITY;
                    break;
                }
                if (str[i] == '\"') {
                    state |= IN_HTML_STRING;
                    break;
                }
                if (str[i] == '\'') {
                    state |= IN_HTML_STRING1;
                    break;
                }
                if (str[i] == '>') {
                    i++;
                    state &= ~IN_HTML_TAG;
                    break;
                }
            }
            SET_COLOR(str, start, i, HTML_STYLE_TAG);
            if (state & (IN_HTML_STRING | IN_HTML_STRING1)) {
                SET_COLOR1(str, i, HTML_STYLE_STRING);
                i++;
            }
            continue;
        }
        /* Plain text stream */
        for (; i < n; i++) {
            if (str[i] == '<'
            &&  (qe_isalpha(str[i + 1]) || str[i + 1] == '!'
            ||   str[i + 1] == '/' || str[i + 1] == '?')) {
                //SET_COLOR(str, start, i, HTML_STYLE_TEXT);
                start = i;
                if (ustristart(str + i, "<script", NULL)) {
                    state |= IN_HTML_SCRIPTTAG;
                    break;
                }
                if (str[i + 1] == '!') {
                    state |= IN_HTML_COMMENT1;
                    i += 2;
                    if (str[i] == '-' && str[i + 1] == '-') {
                        i += 2;
                        state &= ~IN_HTML_COMMENT1;
                        state |= IN_HTML_COMMENT;
                    }
                    SET_COLOR(str, start, i, HTML_STYLE_COMMENT);
                    start = i;
                } else {
                    state |= IN_HTML_TAG;
                }
                break;
            }
            if (str[i] == '&' && get_html_entity(str + i)) {
                state |= IN_HTML_ENTITY;
                break;
            }
        }
        //SET_COLOR(str, start, i, HTML_STYLE_TEXT);
    }
 the_end:
    cp->colorize_state = state;
}

static int html_tagcmp(const char *s1, const char *s2)
{
    int d;

    while (*s2) {
        d = *s2 - qe_toupper(*s1);
        if (d)
            return d;
        s2++;
        s1++;
    }
    return 0;
}

static int htmlsrc_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    const char *buf = cs8(p->buf);

    /* first check file extension */
    if (match_extension(p->filename, mode->extensions))
        return 90;

    /* then try buffer contents */
    if (p->buf_size >= 5 &&
        (!html_tagcmp(buf, "<HTML") ||
         !html_tagcmp(buf, "<SCRIPT") ||
         !html_tagcmp(buf, "<?XML") ||
         !html_tagcmp(buf, "<!DOCTYPE"))) {
        return 90;
    }

    return 1;
}

/* specific htmlsrc commands */
/* CG: need move / kill by tag level */
static CmdDef htmlsrc_commands[] = {
    CMD_DEF_END,
};

static ModeDef htmlsrc_mode;

static int htmlsrc_init(void)
{
    /* html-src mode is almost like the text mode, so we copy and patch it */
    memcpy(&htmlsrc_mode, &text_mode, sizeof(ModeDef));
    htmlsrc_mode.name = "html-src";
    htmlsrc_mode.extensions = "html|htm|asp|shtml|hta|htp|phtml";
    htmlsrc_mode.mode_probe = htmlsrc_mode_probe;
    htmlsrc_mode.colorize_func = htmlsrc_colorize_line;

    qe_register_mode(&htmlsrc_mode);
    qe_register_cmd_table(htmlsrc_commands, &htmlsrc_mode);

    return 0;
}

qe_module_init(htmlsrc_init);
