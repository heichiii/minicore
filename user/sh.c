#include "lib.h"

#define COMMANDS 4
#define ARGS 8
#define LINE 256
struct command { char *argv[ARGS + 1]; char *input, *output; int argc, append; };

/* Tokenize in place. Operators are separate tokens even without whitespace.
 * This teaching shell intentionally has no quoting, expansion, or job control. */
static char *token(char **cursor, int *kind)
{
    char *p = *cursor;
    while (*p == ' ' || *p == '\t') ++p;
    *kind = 0;
    if (!*p) { *cursor = p; return 0; }
    char *start = p;
    if (*p == '|' || *p == '<' || *p == '>') {
        *kind = *p++;
        if (*kind == '>' && *p == '>') { ++p; *kind = '+'; }
    } else {
        while (*p && *p != ' ' && *p != '\t' && *p != '|' && *p != '<' && *p != '>') ++p;
    }
    *cursor = p;
    return start;
}

static int run(char *line)
{
    struct command commands[COMMANDS] = {0};
    int32_t pipes[COMMANDS - 1][2];
    long pids[COMMANDS];
    int count = 1, made = 0, started = 0, result = 0;
    char *cursor = line, *word;
    int kind;
    /* Separate storage avoids overwriting an adjacent operator while adding
     * a terminator to the preceding word (e.g. echo hi|cat). */
    char words[LINE];
    size_t used = 0;
    while ((word = token(&cursor, &kind))) {
        struct command *c = &commands[count - 1];
        if (kind == '|') {
            if (!c->argc || count == COMMANDS) return 2;
            ++count;
            continue;
        }
        int redirect = kind;
        if (redirect) {
            word = token(&cursor, &kind);
            if (!word || kind) return 2;
        }
        size_t n = (size_t)(cursor - word);
        if (used + n + 1 > sizeof(words)) return 2;
        char *value = words + used;
        memcpy(value, word, n); value[n] = 0; used += n + 1;
        if (redirect == '<') c->input = value;
        else if (redirect) { c->output = value; c->append = redirect == '+'; }
        else {
            if (c->argc == ARGS) return 2;
            c->argv[c->argc++] = value;
        }
    }
    if (!commands[count - 1].argc) return count == 1 ? 0 : 2;
    /* cwd belongs to the shell, so cd must run in the parent. */
    if (count == 1 && equal(commands[0].argv[0], "cd"))
        return commands[0].argc == 2 && chdir(commands[0].argv[1]) == 0 ? 0 : 1;
    for (; made < count - 1; ++made)
        if (pipe(pipes[made]) < 0) { result = 1; goto cleanup; }
    for (int i = 0; i < count; ++i) {
        long pid = fork();
        if (pid < 0) { result = 1; goto cleanup; }
        if (pid == 0) {
            struct command *c = &commands[i];
            if (i && dup2(pipes[i - 1][0], 0) < 0) exit(1);
            if (i + 1 < count && dup2(pipes[i][1], 1) < 0) exit(1);
            /* Every inherited pipe fd must close, including unused ends.
             * Otherwise downstream readers never observe EOF. */
            for (int j = 0; j < made; ++j) { close(pipes[j][0]); close(pipes[j][1]); }
            if (c->input) {
                long fd = open(c->input, O_RDONLY);
                if (fd < 0 || dup2(fd, 0) < 0) exit(1);
                if (fd != 0) close(fd);
            }
            if (c->output) {
                long fd = open(c->output, O_WRONLY | O_CREAT | (c->append ? O_APPEND : O_TRUNC));
                if (fd < 0 || dup2(fd, 1) < 0) exit(1);
                if (fd != 1) close(fd);
            }
            char path[64];
            const char *name = c->argv[0];
            bool slash = false;
            for (size_t j = 0; name[j]; ++j) if (name[j] == '/') slash = true;
            if (!slash) {
                if (strlen(name) + 6 > sizeof(path)) exit(127);
                memcpy(path, "/bin/", 5); memcpy(path + 5, name, strlen(name) + 1);
                name = path;
            }
            exec(name, c->argv);
            write_all(2, "sh: exec failed\n", 16);
            exit(127);
        }
        pids[started++] = pid;
    }
cleanup:
    /* Start all stages before waiting: a producer may fill its pipe before
     * the consumer runs. The parent retains no endpoints while waiting. */
    for (int i = 0; i < made; ++i) { close(pipes[i][0]); close(pipes[i][1]); }
    for (int i = 0; i < started; ++i) {
        long status;
        if (wait(pids[i], &status) < 0 || status) result = 1;
    }
    return result;
}

int main(int argc, char **argv)
{
    char line[LINE];
    if (argc == 3 && equal(argv[1], "-c")) {
        if (strlen(argv[2]) >= sizeof(line)) return 2;
        memcpy(line, argv[2], strlen(argv[2]) + 1);
        return run(line);
    }
    if (argc != 1) return 2;
    size_t used = 0;
    int failed = 0;
    for (;;) {
        char c;
        long n = read(0, &c, 1);
        if (n < 0) return 1;
        if (!n || c == '\n' || c == '\r') {
            line[used] = 0;
            if (used && run(line)) failed = 1;
            used = 0;
            if (!n) return failed;
        } else if (used + 1 < sizeof(line)) line[used++] = c;
        else return 2;
    }
}
