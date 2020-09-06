/*

 imap-mh

 Copyright (c) 2020 Arthur Choung. All rights reserved.

 Email: arthur -at- hotdoglinux.com

 This file is part of imap-mh.

 imap-mh is free software: you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>

#define DIGITCHARS "1234567890"

#define BUFSIZE 1024

static char _buf[BUFSIZE];
static FILE *_infp;
static FILE *_outfp;

static void die(char *fmt, ...)
{
    va_list args;
    fprintf(stderr, "Died: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

static void debuglog(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

static void read_line()
{
    if (!fgets(_buf, BUFSIZE, _infp)) {
        die("Unable to read line");
    }
debuglog("recv '%s'", _buf);
}

static void write_string(char *fmt, ...)
{
    va_list args1;
    va_start(args1, fmt);
    vfprintf(_outfp, fmt, args1);
    va_end(args1);
    fflush(_outfp);

    fprintf(stderr, "send '");
    va_list args2;
    va_start(args2, fmt);
    vfprintf(stderr, fmt, args2);
    va_end(args2);
    fprintf(stderr, "'\n");
}

static int file_exists(char *path)
{
    struct stat statbuf;
    if (!stat(path, &statbuf)) {
        return 1;
    }
    return 0;
}

static int is_file_symlink(char *path)
{
    struct stat statbuf;
    if (!lstat(path, &statbuf)) {
        if (S_ISLNK(statbuf.st_mode)) {
            return 1;
        }
    }
    return 0;
}

static FILE *open_file_for_writing(char *path)
{
    int fd = open(path, O_WRONLY|O_CREAT|O_EXCL, 0600);
    if (fd < 0) {
        return NULL;
    }
    FILE *fp = fdopen(fd, "w");
    return fp;
}

static char *string_prefix_endp(char *str, char *prefix)
{
    int len = strlen(prefix);
    if (!len) {
        return NULL;
    }
    if (!strncmp(str, prefix, len)) {
        return str + len;
    }
    return NULL;
}

static char *string_suffix(char *str, char *suffix)
{
    int suffixlen = strlen(suffix);
    if (!suffixlen) {
        return NULL;
    }
    int len = strlen(str);
    if (len < suffixlen) {
        return NULL;
    }
    char *p = str + len-suffixlen;
    if (!strcmp(p, suffix)) {
        return p;
    }
    return NULL;
}

static char *str_validchars_endchar(char *str, char *validchars, char endchar)
{
    char *p = str;
    for(;;) {
        if (*p == endchar) {
            if (p == str) {
                return NULL;
            }
            return p;
        }
        if (!*p) {
            return NULL;
        }
        if (!strchr(validchars, *p)) {
            return NULL;
        }
        p++;
    }
    // not reached
    return NULL;
}

static void chomp_string(char *str)
{
    int len = strlen(str);
    if (len > 0) {
        if (str[len-1] == '\n') {
            str[len-1] = 0;
        }
    }
}

static int is_number_in_range(char *numberstr, char *rangestr)
{
    char *str = rangestr;
    unsigned long number = strtoul(numberstr, NULL, 10);
//debuglog("number %lu", number);

    char *p = str;
    for(;;) {
        char *endp = NULL;
        unsigned long number1 = strtoul(p, &endp, 10);
        if (endp == p) {
            return 0;
        }
        if (*endp == ':') {
            p = endp+1;
            endp = NULL;
            unsigned long number2 = strtoul(p, &endp, 10);
            if (endp == p) {
//debuglog("missing second number of range %lu", number1);
                return 0;
            }
//debuglog("range from %lu to %lu", number1, number2);
            if ((number >= number1) && (number <= number2)) {
                return 1;
            }
            if (*endp == ',') {
                p = endp+1;
                continue;
            }
            return 0;
        } else if (*endp == ',') {
//debuglog("number %lu *", number1);
            if (number == number1) {
                return 1;
            }
            p = endp+1;
            continue;
        } else {
//debuglog("number %lu next char '%c'", number1, *endp);
            if (number == number1) {
                return 1;
            }
            return 0;
        }
    }
    // not reached
    return 0;
}

static int is_filename_uid(char *str)
{
    char *p = str;
    if (*p == '.') {
        p++;
        for(;;) {
            if (!*p) {
                if (p - str >= 2) {
                    return 1;
                }
                return 0;
            }
            if ((*p >= '0') && (*p <= '9')) {
                p++;
                continue;
            }
            return 0;
        }
    }
    return 0;
}

static void unlink_files_in_range(char *range)
{
    DIR *dir = opendir(".");
    if (!dir) {
        die("Unable to open current directory");
    }
    for(;;) {
        struct dirent *ent = readdir(dir);
        if (!ent) {
            break;
        }
        char *p = ent->d_name;
//debuglog("dirent '%s' %d", p, is_filename_uid(p));
        if (is_filename_uid(p)) {
            char *q = p + 1;
//debuglog("q '%s'", q);
            if (is_number_in_range(q, range)) {
debuglog("'%s' in range '%s'", q, range);
                if (unlink(p) != 0) {
                    die("Unable to unlink '%s'", p);
                }
debuglog("unlinked '%s'", p);
            }
        }
    }
    closedir(dir);
}

static void unlink_message_symlinks()
{
    DIR *dir = opendir(".");
    if (!dir) {
        die("Unable to open current directory");
    }
    for(;;) {
        struct dirent *ent = readdir(dir);
        if (!ent) {
            break;
        }
        char *p = ent->d_name;
        if (str_validchars_endchar(p, DIGITCHARS, 0)) {
            if (!is_file_symlink(p)) {
                die("File '%s' is not a symlink", p);
            }
            if (unlink(p) != 0) {
                die("Unable to unlink '%s'", p);
            }
        }
    }
    closedir(dir);
}

static void read_first_line_from_file(char *filename, char *buf)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        die("Unable to open '%s'", filename);
    }
    if (!fgets(buf, BUFSIZE, fp)) {
        die("Unable to read line from '%s'", filename);
    }
    fclose(fp);
    chomp_string(buf);
}

static void write_string_to_new_file(char *str, char *path)
{
    FILE *fp = open_file_for_writing(path);
    if (!fp) {
        die("Unable to create file '%s'", path);
    }
    fprintf(fp, "%s", str);
    fclose(fp);
}

static void wait_for_initial_ok()
{
    read_line();
    if (!string_prefix_endp(_buf, "* OK")) {
        die("Expecting OK but received '%s'", _buf);
    }
}

static void do_login(char *username, char *password)
{
    write_string("login login %s %s\r\n", username, password);
    for(;;) {
        read_line();
        if (string_prefix_endp(_buf, "login OK")) {
            break;
        }
        if (string_prefix_endp(_buf, "login NO")
         || string_prefix_endp(_buf, "login BAD"))
        {
            die("Unable to login '%s'", _buf);
        }
    }
}

static void do_logout()
{
    write_string("logout logout\r\n");
    for(;;) {
        read_line();
        if (string_prefix_endp(_buf, "logout OK")) {
            break;
        }
        if (string_prefix_endp(_buf, "logout NO")
         || string_prefix_endp(_buf, "logout BAD"))
        {
            debuglog("Unable to logout '%s'", _buf);
            break;
        }
    }
}

static void do_enable_qresync()
{
    write_string("qresync enable qresync\r\n");
    for(;;) {
        read_line();
        if (string_prefix_endp(_buf, "qresync OK")) {
            break;
        }
        if (string_prefix_endp(_buf, "qresync NO")
         || string_prefix_endp(_buf, "qresync BAD"))
        {
            die("Unable to enable qresync '%s'", _buf);
        }
    }
}

static void do_fetch(char *range)
{
    write_string("fetch uid fetch %s RFC822\r\n", range);
    for(;;) {
        read_line();
        if (string_prefix_endp(_buf, "fetch OK")) {
            break;
        }
        if (string_prefix_endp(_buf, "fetch NO")
         || string_prefix_endp(_buf, "fetch BAD"))
        {
            die("Unable to uid fetch %s '%s'", range, _buf);
        }
        char *p = string_prefix_endp(_buf, "* ");
        if (!p) {
debuglog("Error, '* ' not found");
            continue;
        }
        if (!strchr(DIGITCHARS, *p)) {
debuglog("Error, digit not found");
            continue;
        }
        p++;

        p = strstr(p, " FETCH ");
        if (!p) {
debuglog("Error, ' FETCH ' not found");
            continue;
        }
        p += 7;

        char *uid_p = strstr(p, "UID ");
        if (!uid_p) {
debuglog("Error, 'UID ' not found");
            continue;
        }
        uid_p += 4;
        char *uid_endp = NULL;
        strtoul(uid_p, &uid_endp, 10);
        if (uid_p == uid_endp) {
debuglog("Error, uid_endp not found");
            continue;
        }
        *uid_endp = 0;
debuglog("uid '%s'", uid_p);
        p = uid_endp+1;

        p = strstr(p, "RFC822 {");
        if (!p) {
debuglog("No 'RFC822 {'");
            continue;
        }
        p += 8;

        char *fetch_size_endp = NULL;
        int fetch_size = strtoul(p, &fetch_size_endp, 10);
        if (p == fetch_size_endp) {
debuglog("Error, fetch_size_endp not found");
            continue;
        }
        p = fetch_size_endp;
        if (strcmp(p, "}\r\n") != 0) {
debuglog("Error, '}\\r\\n' not found");
            continue;
        }

debuglog("uid '%s' fetch_size %d", uid_p, fetch_size);
        {
            char *q = uid_p-1;
            *q = '.';

            if (file_exists(q)) {
                die("File '%s' already exists", q);
            }

            int emailfd = open(q, O_WRONLY|O_CREAT|O_TRUNC, 0600);
            FILE *emailfp = fdopen(emailfd, "w");
            if (!emailfp) {
                die("Unable to create file '%s'", q);
            }

            int fetch_bytes_read = 0;
            for(;;) {
                if (fetch_bytes_read == fetch_size) {
debuglog("success");
                    break;
                }
                if (fetch_bytes_read >= fetch_size) {
                    die("Read too many bytes");
                }

                if (!fgets(_buf, BUFSIZE, _infp)) {
                    die("fgets failed");
                }

                int len = strlen(_buf);

                fetch_bytes_read += len;

                if (len >= 2) {
                    if (_buf[len-1] == '\n') {
                        if (_buf[len-2] == '\r') {
                            _buf[len-2] = '\n';
                            _buf[len-1] = 0;
                            len--;
                        }
                    }
                }
                int n = fwrite(_buf, 1, len, emailfp);
                if (n != len) {
                    die("fwrite error n %d len %d", n, len);
                }

            }

            fclose(emailfp);
        }

        read_line();
        if (!string_suffix(_buf, ")\r\n")) {
            die("Expecting line ending with ')'");
        }
    }
}

static void process_qresync_fetch()
{
    FILE *fp = fopen(".qresync", "r");
    if (!fp) {
debuglog("unable to open .qresync");
        return;
    }
    for(;;) {
        if (!fgets(_buf, BUFSIZE, fp)) {
            break;
        }
        char *p = string_prefix_endp(_buf, "fetch ");
        if (!p) {
            continue;
        }
        char *q = str_validchars_endchar(p, DIGITCHARS, '\n');
        if (!q) {
debuglog("invalid fetch line '%s'", _buf);
            continue;
        }
        *q = 0;
debuglog("fetch '%s'", p);
        char *filename = p-1;
        *filename = '.';
debuglog("Checking file '%s'", filename);
        if (file_exists(filename)) {
debuglog("File '%s' exists, skipping fetch", filename);
        } else {
debuglog("Performing fetch '%s'", p);
            do_fetch(p);
        }
    }
}

static void process_qresync_vanished()
{
    FILE *fp = fopen(".qresync", "r");
    if (!fp) {
debuglog("unable to open .qresync");
        return;
    }
    for(;;) {
        if (!fgets(_buf, BUFSIZE, fp)) {
            break;
        }
        char *p = string_prefix_endp(_buf, "vanished ");
        if (!p) {
            continue;
        }
        char *q = str_validchars_endchar(p, DIGITCHARS ":,", '\n');
        if (!q) {
debuglog("invalid vanished line '%s'", _buf);
            continue;
        }
        *q = 0;
debuglog("vanished '%s'", p);
        unlink_files_in_range(p);
    }
}

static void process_qresync_highestmodseq()
{
    FILE *fp = fopen(".qresync", "r");
    if (!fp) {
debuglog("unable to open .qresync");
        return;
    }
    for(;;) {
        if (!fgets(_buf, BUFSIZE, fp)) {
            break;
        }
        char *p = string_prefix_endp(_buf, "highestmodseq ");
        if (!p) {
            continue;
        }
        char *q = str_validchars_endchar(p, DIGITCHARS, '\n');
        if (!q) {
debuglog("invalid highestmodseq line '%s'", _buf);
            break;
        }
        *q = 0;
debuglog("highestmodseq '%s'", p);
        unlink(".highestmodseq");
        write_string_to_new_file(p, ".highestmodseq");
        return;
    }
}

static int is_directory_empty(char *path)
{
    DIR *dir = opendir(path);
    if (!dir) {
debuglog("Error, unable to open directory '%s'", path);
        return 0;
    }
    for(;;) {
        struct dirent *ent = readdir(dir);
        if (!ent) {
            break;
        }
        if (!strcmp(ent->d_name, ".")) {
            continue;
        }
        if (!strcmp(ent->d_name, "..")) {
            continue;
        }
        return 0;
    }
    closedir(dir);
    return 1;
}

static int is_directory_empty_except_for_init(char *path)
{
    DIR *dir = opendir(path);
    if (!dir) {
debuglog("Error, unable to open directory '%s'", path);
        return 0;
    }
    for(;;) {
        struct dirent *ent = readdir(dir);
        if (!ent) {
            break;
        }
        if (!strcmp(ent->d_name, ".")) {
            continue;
        }
        if (!strcmp(ent->d_name, "..")) {
            continue;
        }
        if (!strcmp(ent->d_name, ".username")) {
            continue;
        }
        if (!strcmp(ent->d_name, ".password")) {
            continue;
        }
        if (!strcmp(ent->d_name, ".mailbox")) {
            continue;
        }
        return 0;
    }
    closedir(dir);
    return 1;
}

static void imap_mh_download()
{
    if (!is_directory_empty_except_for_init(".")) {
        die("Current directory is not empty (excluding .username .password .mailbox)");
    }

    char usernamebuf[BUFSIZE];
    char passwordbuf[BUFSIZE];
    char mailboxbuf[BUFSIZE];
    read_first_line_from_file(".username", usernamebuf);
    read_first_line_from_file(".password", passwordbuf);
    read_first_line_from_file(".mailbox", mailboxbuf);

    _infp = stdin;
    _outfp = stdout;

    wait_for_initial_ok();

    do_login(usernamebuf, passwordbuf);

    do_enable_qresync();

    write_string("select select %s\r\n", mailboxbuf);
    for(;;) {
        read_line();
        if (string_prefix_endp(_buf, "select OK")) {
            break;
        }
        if (string_prefix_endp(_buf, "select NO")
         || string_prefix_endp(_buf, "select BAD"))
        {
            die("Unable to select mailbox %s '%s'", mailboxbuf, _buf);
        }

        char *p = string_prefix_endp(_buf, "* ");
        if (p) {
            char *q = str_validchars_endchar(p, DIGITCHARS, ' ');
            if (q) {
                if (!strcmp(q+1, "EXISTS\r\n")) {
                    int num_exists = strtoul(p, NULL, 10);
debuglog("num_exists %d", num_exists);
                    continue;
                }
            }
        }

        p = string_prefix_endp(_buf, "* OK [UIDVALIDITY ");
        if (p) {
            char *q = strchr(p, ']');
            if (q) {
                *q = 0;
debuglog("uidvalidity '%s'", p);
                write_string_to_new_file(p, ".uidvalidity");
                continue;
            }
        }
        p = string_prefix_endp(_buf, "* OK [HIGHESTMODSEQ ");
        if (p) {
            char *q = strchr(p, ']');
            if (q) {
                *q = 0;
debuglog("highestmodseq '%s'", p);
                write_string_to_new_file(p, ".highestmodseq");
                continue;
            }
        }
    }

    do_fetch("1:*");

    do_logout();

    exit(0);
}

static void imap_mh_update()
{
    if (file_exists(".qresync")) {
        die(".qresync already exists");
    }

    int same_highestmodseq = 0;

    char usernamebuf[BUFSIZE];
    char passwordbuf[BUFSIZE];
    char mailboxbuf[BUFSIZE];
    char uidvaliditybuf[BUFSIZE];
    char highestmodseqbuf[BUFSIZE];
    read_first_line_from_file(".username", usernamebuf);
    read_first_line_from_file(".password", passwordbuf);
    read_first_line_from_file(".mailbox", mailboxbuf);
    {
        read_first_line_from_file(".uidvalidity", uidvaliditybuf);
        char *q = str_validchars_endchar(uidvaliditybuf, DIGITCHARS, 0);
        if (!q) {
            die("Invalid .uidvalidity '%s'", uidvaliditybuf);
        }
        *q = 0;
    }
    {
        read_first_line_from_file(".highestmodseq", highestmodseqbuf);
        char *q = str_validchars_endchar(highestmodseqbuf, DIGITCHARS, 0);
        if (!q) {
            die("Invalid .highestmodseq '%s'", highestmodseqbuf);
        }
        *q = 0;
    }

    _infp = stdin;
    _outfp = stdout;

    wait_for_initial_ok();

    do_login(usernamebuf, passwordbuf);

    do_enable_qresync();

    FILE *qresyncfp = open_file_for_writing(".qresync");
    if (!qresyncfp) {
        die("Unable to open .qresync");
    }

    write_string("select select %s (qresync (%s %s))\r\n", mailboxbuf, uidvaliditybuf, highestmodseqbuf);
    for(;;) {
        read_line();
        if (string_prefix_endp(_buf, "select OK")) {
            break;
        }
        if (string_prefix_endp(_buf, "select NO")
         || string_prefix_endp(_buf, "select BAD"))
        {
            die("Unable to select mailbox %s uidvalidity %s highestmodseq %s '%s'", mailboxbuf, uidvaliditybuf, highestmodseqbuf, _buf);
        }
        char *p = string_prefix_endp(_buf, "* ");
        if (p) {
            char *q = str_validchars_endchar(p, DIGITCHARS, ' ');
            if (q) {
                if (!strcmp(q+1, "EXISTS\r\n")) {
                    int num_exists = strtoul(p, NULL, 10);
debuglog("num_exists %d", num_exists);
                    continue;
                }
            }
        }
        p = string_prefix_endp(_buf, "* OK [UIDVALIDITY ");
        if (p) {
            char *q = strchr(p, ']');
            if (q) {
                *q = 0;
                if (strcmp(p, uidvaliditybuf) != 0) {
                    die("UIDVALIDITY '%s' does not match .uidvalidity '%s', the mailbox may have changed", p, uidvaliditybuf);
                }
                fprintf(qresyncfp, "uidvalidity %s\n", p);
                continue;
            }
        }
        p = string_prefix_endp(_buf, "* OK [HIGHESTMODSEQ ");
        if (p) {
            char *q = strchr(p, ']');
            if (q) {
                *q = 0;
                if (!strcmp(p, highestmodseqbuf)) {
debuglog("HIGHESTMODSEQ '%s' is the same as before", p);
                    same_highestmodseq = 1;
                } else {
                    fprintf(qresyncfp, "highestmodseq %s\n", p);
                }
                continue;
            }
        }

        p = strstr(_buf, " FETCH ");
        if (p) {
            p += 7;

            char *uid_p = strstr(p, "UID ");
            if (!uid_p) {
debuglog("Error, 'UID ' not found");
                continue;
            }
            uid_p += 4;
            char *uid_endp = NULL;
            strtoul(uid_p, &uid_endp, 10);
            if (uid_p == uid_endp) {
debuglog("Error, uid_endp not found");
                continue;
            }
            *uid_endp = 0;
            fprintf(qresyncfp, "fetch %s\n", uid_p);
        }

        p = string_prefix_endp(_buf, "* VANISHED (EARLIER) ");
        if (p) {
            char *q = strchr(p, '\r');
            if (q) {
                *q = 0;
                fprintf(qresyncfp, "vanished %s\n", p);
                continue;
            }
        }
    }

    fclose(qresyncfp);

    if (!same_highestmodseq) {
        process_qresync_fetch();
    }

    do_logout();

    if (!same_highestmodseq) {
        process_qresync_vanished();
        process_qresync_highestmodseq();
        unlink_message_symlinks();
    }

    unlink(".qresync");

    exit(0);
}

static void imap_mh_idle()
{
    char usernamebuf[BUFSIZE];
    char passwordbuf[BUFSIZE];
    char mailboxbuf[BUFSIZE];
    read_first_line_from_file(".username", usernamebuf);
    read_first_line_from_file(".password", passwordbuf);
    read_first_line_from_file(".mailbox", mailboxbuf);
    _infp = stdin;
    _outfp = stdout;

    wait_for_initial_ok();

    do_login(usernamebuf, passwordbuf);

    write_string("select select %s\r\n", mailboxbuf);
    for(;;) {
        read_line();
        if (string_prefix_endp(_buf, "select OK")) {
            break;
        }
        if (string_prefix_endp(_buf, "select NO")
         || string_prefix_endp(_buf, "select BAD"))
        {
            die("Unable to select mailbox %s '%s'", mailboxbuf, _buf);
        }
    }

    write_string("idle idle\r\n");
    for(;;) {
        read_line();
        if (string_prefix_endp(_buf, "+ ")) {
            break;
        }
    }
    for(;;) {
        read_line();
        char *p = string_prefix_endp(_buf, "* ");
        if (p) {
            char *q = str_validchars_endchar(p, DIGITCHARS, ' ');
            if (q) {
                if (!strcmp(q+1, "EXISTS\r\n")) {
                    int num_exists = strtoul(p, NULL, 10);
debuglog("num_exists %d", num_exists);
                    break;
                }
            }
        }
    }

    write_string("DONE\r\n");
    for(;;) {
        read_line();
        if (string_prefix_endp(_buf, "idle OK")) {
            break;
        }
        if (string_prefix_endp(_buf, "idle NO")
         || string_prefix_endp(_buf, "idle BAD")) {
            die("Somehow IDLE failed '%s'", _buf);
        }
    }

    do_logout();

    exit(0);
}

static char *input_password(char *buf)
{
    struct termios oldterm;
    struct termios newterm;

    tcgetattr(0, &oldterm);
    newterm = oldterm;
    newterm.c_lflag &= ~(ECHO);
    tcsetattr(0, TCSANOW, &newterm);

    char *result = fgets(buf, BUFSIZE, stdin);

    tcsetattr(0, TCSANOW, &oldterm);

    return result;
}

static void imap_mh_init()
{
    if (!is_directory_empty(".")) {
        die("Current directory is not empty");
    }

    char usernamebuf[BUFSIZE];
    char passwordbuf[BUFSIZE];
    char mailboxbuf[BUFSIZE];

    printf("Enter IMAP username: ");
    if (!fgets(usernamebuf, BUFSIZE, stdin)) {
        die("Unable to read line");
    }
    chomp_string(usernamebuf);
    printf("Enter IMAP password: ");
    if (!input_password(passwordbuf)) {
        die("Unable to read line");
    }
    chomp_string(passwordbuf);
    printf("\n");
    printf("Enter IMAP mailbox: ");
    if (!fgets(mailboxbuf, BUFSIZE, stdin)) {
        die("Unable to read line");
    }
    chomp_string(mailboxbuf);

    write_string_to_new_file(usernamebuf, ".username");
    write_string_to_new_file(passwordbuf, ".password");
    write_string_to_new_file(mailboxbuf, ".mailbox");

    exit(0);
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        if (!strcmp(argv[1], "init")) {
            imap_mh_init();
        }
        if (!strcmp(argv[1], "download")) {
            imap_mh_download();
        }
        if (!strcmp(argv[1], "update")) {
            imap_mh_update();
        }
        if (!strcmp(argv[1], "idle")) {
            imap_mh_idle();
        }
    }
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "imap-mh init\n");
    fprintf(stderr, "socat openssl:example.com:993 system:'imap-mh download'\n");
    fprintf(stderr, "socat openssl:example.com:993 system:'imap-mh update'\n");
    fprintf(stderr, "socat openssl:example.com:993 system:'imap-mh idle'\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "To disable certificate verification:\n");
    fprintf(stderr, "socat openssl:example.com:993,verify=0 system:'imap-mh download'\n");
    fprintf(stderr, "socat openssl:example.com:993,verify=0 system:'imap-mh update'\n");
    fprintf(stderr, "socat openssl:example.com:993,verify=0 system:'imap-mh idle'\n");
    return 0;
}

