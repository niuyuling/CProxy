#include "kill.h"

static pid_t opt_ns_pid = 0;

static int exact = 1, reg = 0, wait_until_dead = 1, process_group = 0, ignore_case = 0;
static long younger_than = 0, older_than = 0;

typedef struct NAMEINFO {
    const char *name;
    int name_length;
    struct stat st;
} NAMEINFO;

static double uptime()
{
    char *savelocale;
    char buf[2048];
    FILE *file;
    if (!(file = fopen(PROC_BASE "/uptime", "r"))) {
        exit(1);
    }
    savelocale = setlocale(LC_NUMERIC, "C");
    if (fscanf(file, "%2047s", buf) == EOF)
        perror("uptime");
    fclose(file);
    setlocale(LC_NUMERIC, savelocale);
    return atof(buf);
}

static double process_age(const unsigned long long jf)
{
    double age;
    double sc_clk_tck = sysconf(_SC_CLK_TCK);
    assert(sc_clk_tck > 0);
    age = uptime() - jf / sc_clk_tck;
    if (age < 0L)
        return 0L;
    return age;
}

enum ns_type {
    IPCNS = 0,
    MNTNS,
    NETNS,
    PIDNS,
    USERNS,
    UTSNS
};

static const char *ns_names[] = {
    [IPCNS] = "ipc",
    [MNTNS] = "mnt",
    [NETNS] = "net",
    [PIDNS] = "pid",
    [USERNS] = "user",
    [UTSNS] = "uts",
};

const char *get_ns_name(int id)
{
    if (id >= 6)
        return NULL;
    return ns_names[id];
}

static int get_ns(pid_t pid, int id)
{
    struct stat st;
    char buff[50];
    snprintf(buff, sizeof(buff), "/proc/%i/ns/%s", pid, get_ns_name(id));
    if (stat(buff, &st))
        return 0;
    else
        return st.st_ino;
}

static int match_process_uid(pid_t pid, uid_t uid)
{
    char buf[128];
    uid_t puid;
    FILE *f;
    int re = -1;

    snprintf(buf, sizeof buf, PROC_BASE "/%d/status", pid);
    if (!(f = fopen(buf, "r")))
        return 0;

    while (fgets(buf, sizeof buf, f)) {
        if (sscanf(buf, "Uid:\t%d", &puid)) {
            re = uid == puid;
            break;
        }
    }
    fclose(f);
    if (re == -1) {
        exit(1);
    }
    return re;
}

static void free_regexp_list(regex_t * reglist, int names)
{
    int i;
    for (i = 0; i < names; i++)
        regfree(&reglist[i]);
    free(reglist);
}

static regex_t *build_regexp_list(int names, char **namelist)
{
    int i;
    regex_t *reglist;
    int flag = REG_EXTENDED | REG_NOSUB;

    if (!(reglist = malloc(sizeof(regex_t) * names))) {
        perror("malloc");
        exit(1);
    }

    if (ignore_case)
        flag |= REG_ICASE;

    for (i = 0; i < names; i++) {
        if (regcomp(&reglist[i], namelist[i], flag) != 0) {
            free_regexp_list(reglist, i);
            exit(1);
        }
    }
    return reglist;
}

static NAMEINFO *build_nameinfo(const int names, char **namelist)
{
    int i;
    NAMEINFO *ni = NULL;
    if ((ni = malloc(sizeof(NAMEINFO) * names)) == NULL)
        return NULL;

    for (i = 0; i < names; i++) {
        ni[i].name = namelist[i];
        ni[i].st.st_dev = 0;
        if (!strchr(namelist[i], '/')) {
            ni[i].name_length = strlen(namelist[i]);
        } else if (stat(namelist[i], &(ni[i].st)) < 0) {
            perror(namelist[i]);
            free(ni);
            return NULL;
        }
    }
    return ni;
}

static int load_process_name_and_age(char *comm, double *process_age_sec, const pid_t pid, int load_age)
{
    FILE *file;
    char *path;
    char buf[1024];
    char *startcomm, *endcomm;
    unsigned lencomm;
    *process_age_sec = 0;

    if (asprintf(&path, PROC_BASE "/%d/stat", pid) < 0)
        return -1;
    if (!(file = fopen(path, "r"))) {
        free(path);
        return -1;
    }
    free(path);
    if (fgets(buf, 1024, file) == NULL) {
        fclose(file);
        return -1;
    }
    fclose(file);
    startcomm = strchr(buf, '(') + 1;
    endcomm = strrchr(startcomm, ')');
    lencomm = endcomm - startcomm;
    if (lencomm < 0)
        lencomm = 0;
    if (lencomm > COMM_LEN - 1)
        lencomm = COMM_LEN - 1;
    strncpy(comm, startcomm, lencomm);
    comm[lencomm] = '\0';

    endcomm += 2;               // skip ") "
    if (load_age) {
        unsigned long long proc_stt_jf = 0;
        if (sscanf(endcomm, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %Lu", &proc_stt_jf) != 1) {
            return -1;
        }
        *process_age_sec = process_age(proc_stt_jf);
    }
    return lencomm;
}

static int load_proc_cmdline(const pid_t pid, const char *comm, char **command, int *got_long)
{
    FILE *file;
    char *path, *p, *command_buf;
    int cmd_size = 128;
    int okay;

    if (asprintf(&path, PROC_BASE "/%d/cmdline", pid) < 0)
        return -1;
    if (!(file = fopen(path, "r"))) {
        free(path);
        return -1;
    }
    free(path);

    if ((command_buf = (char *)malloc(cmd_size)) == NULL)
        exit(1);

    while (1) {
        for (p = command_buf;; p++) {
            int c;
            if (p == (command_buf + cmd_size)) {
                char *new_command_buf;
                int cur_size = cmd_size;
                cmd_size *= 2;
                new_command_buf = (char *)realloc(command_buf, cmd_size);
                if (!new_command_buf) {
                    if (command_buf)
                        free(command_buf);
                    exit(1);
                }
                command_buf = new_command_buf;
                p = command_buf + cur_size;
            }
            c = fgetc(file);
            if (c == EOF || c == '\0') {
                *p = '\0';
                break;
            } else {
                *p = c;
            }
        }
        if (strlen(command_buf) == 0) {
            okay = 0;
            break;
        }
        p = strrchr(command_buf, '/');
        p = p ? p + 1 : command_buf;
        if (strncmp(p, comm, COMM_LEN - 1) == 0) {
            okay = 1;
            if (!(*command = strdup(p))) {
                free(command_buf);
                exit(1);
            }
            break;
        }
    }
    (void)fclose(file);
    free(command_buf);
    command_buf = NULL;

    if (exact && !okay) {
        *got_long = okay;
        return -1;
    }
    *got_long = okay;
    return 0;
}

static pid_t *create_pid_table(int *max_pids, int *pids)
{
    pid_t self, *pid_table;
    int pid;
    DIR *dir;
    struct dirent *de;

    self = getpid();
    if (!(dir = opendir(PROC_BASE))) {
        perror(PROC_BASE);
        exit(1);
    }
    *max_pids = 256;
    pid_table = malloc(*max_pids * sizeof(pid_t));
    if (!pid_table) {
        perror("malloc");
        exit(1);
    }
    *pids = 0;
    while ((de = readdir(dir)) != NULL) {
        if (!(pid = (pid_t) atoi(de->d_name)) || pid == self)
            continue;
        if (*pids == *max_pids) {
            if (!(pid_table = realloc(pid_table, 2 * *pids * sizeof(pid_t)))) {
                perror("realloc");
                exit(1);
            }
            *max_pids *= 2;
        }
        pid_table[(*pids)++] = pid;
    }
    (void)closedir(dir);
    return pid_table;
}

#define strcmp2(A,B,I) (I? strcasecmp((A),(B)):strcmp((A),(B)))
#define strncmp2(A,B,L,I) (I? strncasecmp((A),(B),(L)):strncmp((A),(B),(L)))
static int match_process_name(const char *proc_comm, const int comm_len, const char *proc_cmdline, const char *match_name, const int match_len, const int got_long)
{
    if (comm_len == OLD_COMM_LEN - 1 && match_len >= OLD_COMM_LEN - 1) {
        if (got_long) {
            return (0 == strncmp2(match_name, proc_cmdline, OLD_COMM_LEN - 1, ignore_case));
        } else {
            return (0 == strncmp2(match_name, proc_comm, OLD_COMM_LEN - 1, ignore_case));
        }
    }

    if (comm_len == COMM_LEN - 1 && match_len >= COMM_LEN - 1) {
        if (got_long) {
            return (0 == strncmp2(match_name, proc_cmdline, COMM_LEN - 1, ignore_case));
        } else {
            return (0 == strncmp2(match_name, proc_comm, COMM_LEN - 1, ignore_case));
        }
    }
    if (got_long) {
        return (0 == strcmp2(match_name, proc_cmdline, ignore_case));
    }
    return (0 == strcmp2(match_name, proc_comm, ignore_case));
}

int kill_all(int signal, int name_count, char **namelist, struct passwd *pwent)
{
    struct stat st;
    NAMEINFO *name_info = NULL;
    char *path, comm[COMM_LEN];
    char *command = NULL;
    pid_t *pid_table, *pid_killed;
    pid_t *pgids = NULL;
    int i, j, length, got_long, error;
    int pids, max_pids, pids_killed;
    unsigned long found;
    regex_t *reglist = NULL;
    long ns_ino = 0;

    if (opt_ns_pid)
        ns_ino = get_ns(opt_ns_pid, PIDNS);

    if (name_count && reg)
        reglist = build_regexp_list(name_count, namelist);
    else if ((name_info = build_nameinfo(name_count, namelist)) == NULL)
        exit(1);

    pid_table = create_pid_table(&max_pids, &pids);
    found = 0;
    pids_killed = 0;
    pid_killed = malloc(max_pids * sizeof(pid_t));
    if (!pid_killed) {
        perror("malloc");
        exit(1);
    }
    if (process_group) {
        pgids = calloc(pids, sizeof(pid_t));
        if (!pgids) {
            perror("malloc");
            exit(1);
        }
    }
    got_long = 0;
    for (i = 0; i < pids; i++) {
        pid_t id;
        int found_name = -1;
        double process_age_sec = 0;
        if (pwent && match_process_uid(pid_table[i], pwent->pw_uid) == 0)
            continue;
        if (opt_ns_pid && ns_ino && ns_ino != get_ns(pid_table[i], PIDNS))
            continue;
        length = load_process_name_and_age(comm, &process_age_sec, pid_table[i], (younger_than || older_than));
        if (length < 0)
            continue;
        if (younger_than && (process_age_sec > younger_than))
            continue;
        if (older_than && (process_age_sec < older_than))
            continue;

        got_long = 0;
        if (command) {
            free(command);
            command = NULL;
        }
        if (length == COMM_LEN - 1)
            if (load_proc_cmdline(pid_table[i], comm, &command, &got_long) < 0)
                continue;
        for (j = 0; j < name_count; j++) {
            if (reg) {
                if (regexec(&reglist[j], got_long ? command : comm, 0, NULL, 0)
                    != 0)
                    continue;
            } else {
                if (!name_info[j].st.st_dev) {
                    if (!match_process_name(comm, length, command, namelist[j], name_info[j].name_length, got_long))
                        continue;

                } else {
                    int ok = 1;
                    if (asprintf(&path, PROC_BASE "/%d/exe", pid_table[i]) < 0)
                        continue;
                    if (stat(path, &st) < 0)
                        ok = 0;
                    else if (name_info[j].st.st_dev != st.st_dev || name_info[j].st.st_ino != st.st_ino) {
                        size_t len = strlen(namelist[j]);
                        char *linkbuf = malloc(len + 1);

                        if (!linkbuf || readlink(path, linkbuf, len + 1) != (ssize_t) len || memcmp(namelist[j], linkbuf, len))
                            ok = 0;
                        free(linkbuf);
                    }
                    free(path);
                    if (!ok)
                        continue;
                }
            }
            found_name = j;
            break;
        }
        if (name_count && found_name == -1)
            continue;
        if (!process_group)
            id = pid_table[i];
        else {
            int j;

            id = getpgid(pid_table[i]);
            pgids[i] = id;
            for (j = 0; j < i; j++)
                if (pgids[j] == id)
                    break;
            if (j < i)
                continue;
        }

        if (kill(process_group ? -id : id, signal) >= 0) {
            if (found_name >= 0)
                found |= 1UL << found_name;
            pid_killed[pids_killed++] = id;
        }
    }
    if (command)
        free(command);
    if (reglist)
        free_regexp_list(reglist, name_count);
    free(pgids);
    if (name_count)
        error = found == ((1UL << (name_count - 1)) | ((1UL << (name_count - 1)) - 1)) ? 0 : 1;
    else
        error = pids_killed ? 0 : 1;
    while (pids_killed && wait_until_dead) {
        for (i = 0; i < pids_killed;) {
            if (kill(process_group ? -pid_killed[i] : pid_killed[i], 0) < 0 && errno == ESRCH) {
                pid_killed[i] = pid_killed[--pids_killed];
                continue;
            }
            i++;
        }
        //sleep(1);
    }
    free(pid_killed);
    free(pid_table);
    free(name_info);
    return error;
}
