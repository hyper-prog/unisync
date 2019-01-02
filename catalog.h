/* **********************************************************
    UniSync - Universal direcotry sync-diff utility
     http://hyperprog.com

    (C) 2014-2019 Peter Deak (hyper80@gmail.com)

    License: GPLv2  http://www.gnu.org/licenses/gpl-2.0.html
************************************************************* */
#ifndef UNISYNC_CATALOG_H
#define UNISYNC_CATALOG_H

#define DIRECTION_CAT_TO_DIFF   0
#define DIRECTION_DIFF_TO_CAT   1

#define STATUS_NULL             0
#define STATUS_MATCH            1
#define STATUS_SIZEDIFF         2
#define STATUS_HASHDIFF         3
#define STATUS_TIMEDIFF         4
#define STATUS_FIXTIME          9

struct cItem
{
    char pathname[300];
    unsigned int size;
    char time[32];
    char htype;
    char hash[70];
    char status;
    struct cItem *n,*p;
};

class UniCatalog
{
public:
    UniCatalog(UniSyncConfig *ucp);
    ~UniCatalog(void);

    void clear(void);
    int  read(const char *filename);
    int  scandir(const char *basedir,FILE *catstream,bool build_icat=false);
    int  scandir_diff(const char *basedir);
    int  scandir_sync(const char *sourcefolder_bp,const char *targetfolder_bp,int direction);
    int  make_update_package(const char *sourcefolder_bp,const char *updatepack_bp);
    int  apply_update_package(const char *updatepack_bp,const char *targetfolder_bp);
    int  print_sync_procedures(const char *sourcefolder_bp,const char *targetfolder_bp,int direction);

    void rawPrint(void);
    void diffresultPrint(void);

private:
    int  scandir_in(const char *basedir,const char *dirname,FILE *catstream,bool build_icat);
    int  scandir_diff_in(const char *basedir,const char *dirname);

#ifdef _WIN32
    //Platform specific (windows)
    int  scandir_in_win(const char *basedir,const char *dirname,FILE *catstream,bool build_icat);
    int  scandir_diff_in_win(const char *basedir,const char *dirname);
#endif

    void free_catalog(struct cItem** cpointer);
    void catalog_push(struct cItem** cpointer,struct cItem *item);
    struct cItem * catalog_search(struct cItem** cat,char *pathname);
    void catalog_delete(struct cItem** fromcatalog,struct cItem* item);
    void catalog_move(struct cItem** fromcatalog,struct cItem* item,struct cItem** targetcatalog);
    void printStatistics(const char *funcname);

    bool needExclude(int typ,char *name);

    void createFullPath(char *fullpath,const char *basedir,const char *path,bool appendsuball = false);

private:
    UniSyncConfig *uc;

    struct cItem *cat_file;
    struct cItem *cat_file_ok;
    struct cItem *cat_file_mod;
    struct cItem *cat_file_new;
    struct cItem *cat_file_fixtime;

    struct cItem *cat_dir;
    struct cItem *cat_dir_ok;
    struct cItem *cat_dir_mod;
    struct cItem *cat_dir_new;

    double sizec;
    time_t ts,te;
};

#endif // UNISYNC_CATALOG_H
