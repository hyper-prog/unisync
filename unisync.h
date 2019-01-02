/* **********************************************************
    UniSync - Universal direcotry sync-diff utility
     http://hyperprog.com

    (C) 2014-2019 Peter Deak (hyper80@gmail.com)

    License: GPLv2  http://www.gnu.org/licenses/gpl-2.0.html
************************************************************* */
#ifndef UNISYNC_UNISYNC_GLOBAL_H
#define UNISYNC_UNISYNC_GLOBAL_H

#define PROGRAMNAME "UniSync"
#define PROGRAMCMD  "unisync"
#define VERSION     "1.0"

#define HASH_EMPTY      0
#define HASH_MD5        1
#define HASH_SHA256     2

#define EXCL_FILE       0
#define EXCL_DIR        1
#define EXCL_PATH       2

class ExcludeNames
{
public:
    char name[256];
    ExcludeNames *n;
    int typ;
};

class UniSyncConfig
{
public:
    int guicall;
    int verbose;
    int hashmode;
    int watchtime;
    int skiphash;
    int exclude;
    int fixmtime;
    int usestd;
    int interactivesync;
    ExcludeNames *exl;

    UniSyncConfig(void);
    void save(void);
    void restore(void);

private:
    int s_verbose,s_hashmode,s_watchtime,s_skiphash,s_exclude,s_fixmtime;
};

#endif // UNISYNC_UNISYNC_GLOBAL_H
