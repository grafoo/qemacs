/*
 * Perl Source mode for QEmacs.
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

/*---------------- Perl colors ----------------*/

enum {
    PERL_STYLE_TEXT    = QE_STYLE_DEFAULT,
    PERL_STYLE_COMMENT = QE_STYLE_COMMENT,
    PERL_STYLE_STRING  = QE_STYLE_STRING,
    PERL_STYLE_REGEX   = QE_STYLE_STRING,
    PERL_STYLE_DELIM   = QE_STYLE_KEYWORD,
    PERL_STYLE_KEYWORD = QE_STYLE_KEYWORD,
    PERL_STYLE_VAR     = QE_STYLE_VARIABLE,
    PERL_STYLE_NUMBER  = QE_STYLE_NUMBER,
};

enum {
    IN_PERL_STRING1 = 0x01,    /* single quote */
    IN_PERL_STRING2 = 0x02,    /* double quote */
    IN_PERL_FORMAT  = 0x04,    /* format = ... */
    IN_PERL_HEREDOC = 0x08,
    IN_PERL_POD     = 0x10,
};

/* CG: bogus if multiple regions are colorized, should use signature */
/* XXX: should move this to mode data */
static unsigned int perl_eos[100];
static int perl_eos_len;

static int perl_var(const unsigned int *str, int j, int n)
{
    if (qe_isdigit(str[j]))
        return j;
    for (; j < n; j++) {
        if (qe_isalnum_(str[j]))
            continue;
        if (str[j] == '\'' && qe_isalpha_(str[j + 1]))
            j++;
        else
            break;
    }
    return j;
}

static int perl_number(const unsigned int *str, int j, __unused__ int n)
{
    if (str[j] == '0') {
        j++;
        if (str[j] == 'x' || str[j] == 'X') {
            do { j++; } while (qe_isxdigit(str[j]));
            return j;
        }
        if (str[j] >= '0' && str[j] <= '7') {
            do { j++; } while (str[j] >= '0' && str[j] <= '7');
            return j;
        }
    }
    while (qe_isdigit(str[j]))
        j++;

    if (str[j] == '.')
        do { j++; } while (qe_isdigit(str[j]));

    if (str[j] == 'E' || str[j] == 'e') {
        j++;
        if (str[j] == '-' || str[j] == '+')
            j++;
        while (qe_isdigit(str[j]))
            j++;
    }
    return j;
}

/* return offset of matching delimiter or end of string */
static int perl_string(const unsigned int *str, unsigned int delim,
                       int j, int n)
{
    for (; j < n; j++) {
        if (str[j] == '\\')
            j++;
        else
        if (str[j] == delim)
            return j;
    }
    return j;
}

static void perl_colorize_line(QEColorizeContext *cp,
                               unsigned int *str, int n, int mode_flags)
{
    int i = 0, c, c1, c2, j = i, s1, s2, delim = 0;
    int colstate = cp->colorize_state;

    if (colstate & (IN_PERL_STRING1 | IN_PERL_STRING2)) {
        delim = (colstate & IN_PERL_STRING1) ? '\'' : '\"';
        i = perl_string(str, delim, j, n);
        if (i < n) {
            i++;
            colstate &= ~(IN_PERL_STRING1 | IN_PERL_STRING2);
        }
        SET_COLOR(str, j, i, PERL_STYLE_STRING);
    } else
    if (colstate & IN_PERL_FORMAT) {
        i = n;
        if (n == 1 && str[0] == '.')
            colstate &= ~IN_PERL_FORMAT;
        SET_COLOR(str, j, i, PERL_STYLE_STRING);
    }
    if (colstate & IN_PERL_HEREDOC) {
        i = n;
        if (n == perl_eos_len && !umemcmp(perl_eos, str, n)) {
            colstate &= ~IN_PERL_HEREDOC;
            SET_COLOR(str, j, i, PERL_STYLE_KEYWORD);
        } else {
            SET_COLOR(str, j, i, PERL_STYLE_STRING);
        }
    }
    if (str[i] == '=' && qe_isalpha(str[i + 1])) {
        colstate |= IN_PERL_POD;
    }
    if (colstate & IN_PERL_POD) {
        if (ustrstart(str + i, "=cut", NULL)) {
            colstate &= ~IN_PERL_POD;
        }
        if (str[i] == '=' && qe_isalpha(str[i + 1])) {
            i = n;
            SET_COLOR(str, j, i, PERL_STYLE_KEYWORD);
        } else {
            i = n;
            SET_COLOR(str, j, i, PERL_STYLE_COMMENT);
        }
    }

    while (i < n) {
        j = i + 1;
        c1 = str[j];
        switch (c = str[i]) {
        case '$':
            if (c1 == '^' && qe_isalpha(str[i + 2])) {
                j = i + 3;
                goto keyword;
            }
            if (c1 == '#' && qe_isalpha_(str[i + 2]))
                j++;
            else
            if (memchr("|%=-~^123456789&`'+_./\\,\"#$?*0[];!@", c1, 35)) {
                /* Special variable */
                j = i + 2;
                goto keyword;
            }
            /* FALL THRU */
        case '*':
        case '@':       /* arrays */
        case '%':       /* associative arrays */
        case '&':
            if (j >= n)
                break;
            s1 = perl_var(str, j, n);
            if (s1 > j) {
                SET_COLOR(str, i, s1, PERL_STYLE_VAR);
                i = s1;
                continue;
            }
            break;
        case '-':
            if (c1 == '-') {
                i += 2;
                continue;
            }
            if (qe_isalpha(c1) && !qe_isalnum(str[i + 2])) {
                j = i + 2;
                goto keyword;
            }
            break;
        case '#':
            SET_COLOR(str, i, n, PERL_STYLE_COMMENT);
            i = n;
            continue;
        case '<':
            if (c1 == '<') {
                /* Should check for unary context */
                s1 = i + 2;
                while (qe_isspace(str[s1]))
                    s1++;
                c2 = str[s1];
                if (c2 == '"' || c2 == '\'' || c2 == '`') {
                    s2 = perl_string(str, c2, ++s1, n);
                } else {
                    s2 = perl_var(str, s1, n);
                }
                if (s2 > s1) {
                    perl_eos_len = min((int)(s2 - s1), countof(perl_eos) - 1);
                    umemcpy(perl_eos, str + s1, perl_eos_len);
                    perl_eos[perl_eos_len] = '\0';
                    colstate |= IN_PERL_HEREDOC;
                }
                i += 2;
                continue;
            }
            delim = '>';
            goto string;
        case '/':
        case '?':
            /* Should check for unary context */
            /* parse regex */
            s1 = perl_string(str, c, j, n);
            if (s1 >= n)
                break;
            SET_COLOR1(str, i, PERL_STYLE_DELIM);
            SET_COLOR(str, i + 1, s1, PERL_STYLE_REGEX);
            i = s1;
            while (++i < n && qe_isalpha(str[i]))
                continue;
            SET_COLOR(str, s1, i, PERL_STYLE_DELIM);
            continue;
        case '\'':
        case '`':
        case '"':
            delim = c;
        string:
            /* parse string const */
            s1 = perl_string(str, delim, j, n);
            if (s1 >= n) {
                if (c == '\'') {
                    SET_COLOR(str, i, n, PERL_STYLE_STRING);
                    i = n;
                    colstate |= IN_PERL_STRING1;
                    continue;
                }
                if (c == '\"') {
                    SET_COLOR(str, i, n, PERL_STYLE_STRING);
                    i = n;
                    colstate |= IN_PERL_STRING2;
                    continue;
                }
                /* ` string spanning more than one line treated as
                 * operator.
                 */
                break;
            }
            s1++;
            SET_COLOR(str, i, s1, PERL_STYLE_STRING);
            i = s1;
            continue;
        case '.':
            if (qe_isdigit(c1))
                goto number;
            break;

        default:
            if (qe_isdigit(c)) {
            number:
                j = perl_number(str, i, n);
                SET_COLOR(str, i, j, PERL_STYLE_NUMBER);
                i = j;
                continue;
            }
            if (!qe_isalpha_(c))
                break;

            j = perl_var(str, i, n);
            if (j == i)
                break;

            if (j >= n)
                goto keyword;

            /* Should check for context */
            if ((j == i + 1 && (c == 'm' || c == 'q'))
            ||  (j == i + 2 && c == 'q' && (c1 == 'q' || c1 == 'x'))) {
                s1 = perl_string(str, str[j], j + 1, n);
                if (s1 >= n)
                    goto keyword;
                SET_COLOR(str, i, j + 1, PERL_STYLE_DELIM);
                SET_COLOR(str, j + 1, s1, PERL_STYLE_REGEX);
                i = s1;
                while (++i < n && qe_isalpha(str[i]))
                    continue;
                SET_COLOR(str, s1, i, PERL_STYLE_DELIM);
                continue;
            }
            /* Should check for context */
            if ((j == i + 1 && (c == 's' /* || c == 'y' */))
            ||  (j == i + 2 && c == 't' && c1 == 'r')) {
                s1 = perl_string(str, str[j], j + 1, n);
                if (s1 >= n)
                    goto keyword;
                s2 = perl_string(str, str[j], s1 + 1, n);
                if (s2 >= n)
                    goto keyword;
                SET_COLOR(str, i, j + 1, PERL_STYLE_DELIM);
                SET_COLOR(str, j + 1, s1, PERL_STYLE_REGEX);
                SET_COLOR1(str, s1, PERL_STYLE_DELIM);
                SET_COLOR(str, s1 + 1, s2, PERL_STYLE_REGEX);
                i = s2;
                while (++i < n && qe_isalpha(str[i]))
                    continue;
                SET_COLOR(str, s2, i, PERL_STYLE_DELIM);
                continue;
            }
        keyword:
            if (j - i == 6 && ustristart(str + i, "format", NULL)) {
                for (s1 = 0; s1 < i; s1++) {
                    if (!qe_isspace(str[s1]))
                        break;
                }
                if (s1 == i) {
                    /* keyword is first on line */
                    colstate |= IN_PERL_FORMAT;
                }
            }
            SET_COLOR(str, i, j, PERL_STYLE_KEYWORD);
            i = j;
            continue;
        }
        i++;
        continue;
    }
    cp->colorize_state = colstate;
}

static int perl_mode_probe(ModeDef *mode, ModeProbeData *p)
{
    /* just check file extension */
    if (match_extension(p->filename, mode->extensions))
        return 80;

    if (p->buf[0] == '#' && p->buf[1] == '!'
    &&  memstr(p->buf, p->line_len, "bin/perl"))
        return 80;

    return 1;
}

/* specific perl commands */
static CmdDef perl_commands[] = {
    CMD_DEF_END,
};

static ModeDef perl_mode;

static int perl_init(void)
{
    /* perl mode is almost like the text mode, so we copy and patch it */
    memcpy(&perl_mode, &text_mode, sizeof(ModeDef));
    perl_mode.name = "Perl";
    perl_mode.extensions = "pl|perl|pm";
    perl_mode.mode_probe = perl_mode_probe;
    perl_mode.colorize_func = perl_colorize_line;

    qe_register_mode(&perl_mode);
    qe_register_cmd_table(perl_commands, &perl_mode);

    return 0;
}

qe_module_init(perl_init);
