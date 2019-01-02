/* **********************************************************
    UniSync - Universal direcotry sync-diff utility
     http://hyperprog.com

    (C) 2014-2019 Peter Deak (hyper80@gmail.com)

    License: GPLv2  http://www.gnu.org/licenses/gpl-2.0.html
************************************************************* */
#ifndef UNISYNC_UTILS_H
#define UNISYNC_UTILS_H

#include "unisync.h"

char dtoh(int v);
void trimenddir(char *str);
char *unifypath(char *path);
char *chop(char *str);
int my_dtoa(double v,char *buffer,int bufflen,int min,int max,int group);
int gethash(const char *fullpath,char *hexhash,int hashmode=HASH_SHA256,int needprefix = 1);
char read_and_echo_character();

struct PathMakerCacheItem
{
    char path[512];
    struct PathMakerCacheItem* n;
};

class PathMaker
{
public:
    static int mkpath(const char *path,bool contains_filename);
    static void clearCache(void);
private:
    static bool   findPath(char *path);
    static struct PathMakerCacheItem* cache;
};

class FileCopier
{
public:
    static double ckbytes;
    time_t ts,te;

    FileCopier(UniSyncConfig *ucp);
    int copy(const char *source,const char *dest);
    int copy_std(const char *source,const char *dest);
    int copy_spec(const char *source,const char *dest);
    int fixtime(const char *source,const char *dest);
    void resetCounters(void);
    void printStatistics();
    int deletefile(const char *path);
    int deletefolder(const char *path);

private:
    UniSyncConfig *uc;
};

#endif // UNISYNC_UTILS_H
