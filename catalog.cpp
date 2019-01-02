/* **********************************************************
    UniSync - Universal direcotry sync-diff utility
     http://hyperprog.com

    (C) 2014-2019 Peter Deak (hyper80@gmail.com)

    License: GPLv2  http://www.gnu.org/licenses/gpl-2.0.html
************************************************************* */
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#endif

#include "unisync.h"
#include "catalog.h"
#include "utils.h"

void time_to_str(const time_t * t,char *buffer) //need >32 byte char buffer
{
    struct tm * timeinfo;
    timeinfo = localtime(t);
    if(timeinfo == NULL)
    {
        snprintf(buffer,32,"2000-01-01_00:00:00");
        return;
    }

// I don't understand why this difference exist between platforms, but the same code give different
// time values for same files on windows and linux
#ifdef _WIN32
    time_t dst = *t;
    dst += 3600 * timeinfo->tm_isdst;
    timeinfo = localtime(&dst);
    if(timeinfo == NULL)
    {
        snprintf(buffer,32,"2000-01-01_00:00:00");
        return;
    }
    strftime(buffer,32,"%Y-%m-%d_%H:%M:%S",timeinfo);
#else
    strftime(buffer,32,"%Y-%m-%d_%H:%M:%S",timeinfo);
#endif
}

#ifdef _WIN32
void time_to_str_win(FILETIME *t,char *buffer) //need >32 byte char buffer
{
    SYSTEMTIME st0,ts;
    FileTimeToSystemTime(t,&st0);
    SystemTimeToTzSpecificLocalTime(NULL,&st0,&ts);
    snprintf(buffer,32,"%4d-%02d-%02d_%02d:%02d:%02d",
             ts.wYear,ts.wMonth,ts.wDay,ts.wHour,ts.wMinute,ts.wSecond);
}
#endif

UniCatalog::UniCatalog(UniSyncConfig *ucp)
{
    uc = ucp;
    cat_file         = NULL;
    cat_file_ok      = NULL;
    cat_file_mod     = NULL;
    cat_file_new     = NULL;
    cat_file_fixtime = NULL;
    cat_dir          = NULL;
    cat_dir_ok       = NULL;
    cat_dir_mod      = NULL;
    cat_dir_new      = NULL;
}

UniCatalog::~UniCatalog(void)
{
    clear();
}

bool UniCatalog::needExclude(int typ,char *name)
{
    ExcludeNames *r = uc->exl;
    while(r != NULL)
    {
        if(r->typ == typ && !strcmp(r->name,name))
        {
            return true;
            break;
        }
        r = r->n;
    }
    return false;
}

int UniCatalog::read(const char *filename)
{
    int i;
    char *tok,buffer[1024];

    if(uc->verbose > 0)
    {
        printf("Reading catalog file...\n");
        if(uc->guicall)
            fflush(stdout);
    }
    clear();
    FILE *cf = NULL;
    if(NULL != (cf = fopen(filename,"r")))
    {
        while(!feof(cf))
        {
            if(fgets(buffer,1024,cf)!=NULL)
            {
                if(buffer[0] == 'F')
                {
                    cItem *item = new cItem();
                    item->status = STATUS_NULL;
                    item->size = 0;
                    item->htype = HASH_EMPTY;
                    tok = buffer;
                    i=0;
                    tok = strtok(buffer,"*");
                    while(tok != NULL)
                    {
                        if(i == 1)
                        {
                            char *utok = unifypath(tok);
                            strcpy(item->pathname,utok);
                        }
                        if(i == 2)
                            strcpy(item->time,tok);
                        if(i == 3)
                            item->size = atoi(tok);
                        if(i > 3)
                        {
                            if(!strncmp(tok,"MD5:",4))
                            {
                                item->htype = HASH_MD5;
                                strcpy(item->hash,tok+4);
                            }
                            if(!strncmp(tok,"SHA2:",5))
                            {
                                item->htype = HASH_SHA256;
                                strcpy(item->hash,tok+5);
                            }
                        }
                        ++i;
                        tok = strtok(NULL,"*");
                    }

                    catalog_push(&cat_file,item);
                }
                if(buffer[0] == 'D')
                {
                    cItem *item = new cItem();
                    item->status = STATUS_NULL;
                    item->size = 0;
                    item->htype = HASH_EMPTY;
                    tok = buffer;
                    i=0;
                    tok = strtok(buffer,"*");
                    while(tok != NULL)
                    {
                        if(i == 1)
                        {
                            char *utok = unifypath(tok);
                            strcpy(item->pathname,utok);
                        }
                        if(i == 2)
                            strcpy(item->time,tok);
                        ++i;
                        tok = strtok(NULL,"*");
                    }
                    catalog_push(&cat_dir,item);
                }
            }
        }
        fclose(cf);
        return 0;
    }
    fprintf(stderr,"Error, cannot open catalog file: %s\n",filename);
    return 1;
}

void UniCatalog::createFullPath(char *fullpath,const char *basedir,const char *path,bool appendsuball)
{
    int windows = 0;
#ifdef _WIN32
    windows = 1;
#endif

    if(!strcmp(basedir,"/") || !strcmp(basedir,"\\"))
    {
        if(strlen(path) == 0)
            snprintf(fullpath,510,"/");
        else
            snprintf(fullpath,510,"/%s",path);
    }
    else if(!strcmp(basedir,""))
    {
        if(strlen(path) == 0)
            snprintf(fullpath,510,".");
        else
            snprintf(fullpath,510,"./%s",path);
    }
    else if(windows && strlen(basedir) == 2 && isalpha(basedir[0]) && basedir[1] == ':')
    {
        if(strlen(path) == 0)
            snprintf(fullpath,510,"%s/",basedir);
        else
            snprintf(fullpath,510,"%s/%s",basedir,path);
    }
    else
    {
        if(strlen(path) == 0)
            snprintf(fullpath,510,"%s",basedir);
        else
            snprintf(fullpath,510,"%s/%s",basedir,path);
    }

    if(appendsuball)
    {
        int len = strlen(fullpath);
        if(fullpath[len-1] != '/' && fullpath[len-1] != '\\')
            fullpath[len++] = '/';
        fullpath[len++] = '*';
        fullpath[len] = '\0';
    }
}

int UniCatalog::scandir(const char *basedir,FILE *catstream,bool build_icat)
{
    int r;
    sizec = 0.0;
    ts = time(NULL);
#ifdef _WIN32
    if(uc->usestd)
        r = scandir_in(basedir,"",catstream,build_icat);
    else
        r = scandir_in_win(basedir,"",catstream,build_icat);
#else
    r = scandir_in(basedir,"",catstream,build_icat);
#endif
    te = time(NULL);
    if(r == 0 && uc->verbose > 0)
        printStatistics("scanned");
    return r;
}

void UniCatalog::printStatistics(const char *funcname)
{
    char buff[64];
    double lctime,speed=0;
    lctime = difftime(te,ts);
    if(lctime > 0)
        speed = (sizec/1024) / lctime;
    my_dtoa(sizec,(char *)buff,64,0,2,1);
    if(speed > 0)
        printf("%s kbyte %s in %.2f sec (%.2f Mbyte/sec)\n",buff,funcname,lctime,speed);
    else
        printf("%s kbyte %s in %.2f sec\n",buff,funcname,lctime);
    if(uc->guicall)
        fflush(stdout);
}

int UniCatalog::scandir_in(const char *basedir,const char *dirname,FILE *catstream,bool build_icat)
{
    char hexhash[300];
    char fullpath[512];
    char mypath[512];
    char *umypath;
    char timestrbuf[32];
    char sizestrbuf[32];
    DIR *dir;
    struct dirent *ent;
    struct stat s;

    if(dirname[0] == '\0' && uc->verbose > 0)
    {
        printf("Scanning directory to build catalog...\n");
        if(uc->guicall)
            fflush(stdout);
    }

    createFullPath(fullpath,basedir,dirname,false);
    if ((dir = opendir(fullpath)) != NULL)
    {
        if(uc->verbose > 1 && dirname[0] != '\0')
        {
            printf("%s\n",dirname);
            if(uc->guicall)
                fflush(stdout);
        }

        while ((ent = readdir (dir)) != NULL)
        {
            snprintf(mypath,512,"%s/%s",dirname,ent->d_name);
            umypath = unifypath(mypath);
            if(!strcmp(basedir,"/") || !strcmp(basedir,"\\"))
                snprintf(fullpath,512,"/%s",umypath);
            else
                snprintf(fullpath,512,"%s/%s",basedir,umypath);
            if(!stat(fullpath,&s))
            {
                if( !(s.st_mode & S_IFDIR) && !(s.st_mode & S_IFREG) )
                    continue;
                if( s.st_mode & S_IFDIR )
                {
                    if(strcmp(ent->d_name,".") && strcmp(ent->d_name,".."))
                    {
                        if(uc->exclude)
                        {
                            if(needExclude(EXCL_DIR,ent->d_name))
                                continue;
                            if(needExclude(EXCL_PATH,umypath))
                                continue;
                        }

                        time_to_str(&s.st_mtime,timestrbuf);
                        if(catstream != NULL)
                        {
                            fputs("D*"      ,catstream);
                            fputs(umypath   ,catstream);
                            fputs("*"       ,catstream);
                            fputs(timestrbuf,catstream);
                            fputs("*\n"     ,catstream);
                        }

                        if(build_icat)
                        {
                            cItem *item = new cItem();
                            item->status = STATUS_NULL;
                            item->size = 0;
                            item->htype = HASH_EMPTY;
                            strcpy(item->pathname,umypath);
                            strcpy(item->time,timestrbuf);
                            catalog_push(&cat_dir,item);
                        }

                        if(scandir_in(basedir,umypath,catstream,build_icat))
                            return 1;
                    }
                }
                if(s.st_mode & S_IFREG )
                {
                    if(uc->exclude)
                        if(needExclude(EXCL_FILE,ent->d_name))
                            continue;

                    gethash(fullpath,hexhash,uc->hashmode);
                    time_to_str(&s.st_mtime,timestrbuf);
                    snprintf(sizestrbuf,32,"%d",(unsigned int)s.st_size);
                    sizec += ((double)((unsigned int)s.st_size)) / 1024;

                    if(catstream != NULL)
                    {
                        fputs("F*"      ,catstream);
                        fputs(umypath   ,catstream);
                        fputs("*"       ,catstream);
                        fputs(timestrbuf,catstream);
                        fputs("*"       ,catstream);
                        fputs(sizestrbuf,catstream);
                        fputs("*"       ,catstream);
                        fputs(hexhash   ,catstream);
                        fputs("*\n"     ,catstream);
                    }
                    if(build_icat)
                    {
                        cItem *item = new cItem();
                        item->status = STATUS_NULL;
                        item->size = (unsigned int)s.st_size;

                        strcpy(item->pathname,umypath);
                        strcpy(item->time,timestrbuf);
                        item->htype = uc->hashmode;
                        char *hb = hexhash;
                        if(uc->hashmode == HASH_MD5)    hb += 4;
                        if(uc->hashmode == HASH_SHA256) hb += 5;
                        strcpy(item->hash,hb);
                        catalog_push(&cat_file,item);
                    }
                }
            }
            else
            {
                fprintf(stderr,"Error, Cannot stat: %s\n",fullpath);
                if(uc->guicall)
                    fflush(stderr);
                return 1;
            }
        }
        closedir(dir);
    }
    return 0;
}

#ifdef _WIN32
int UniCatalog::scandir_in_win(const char *basedir,const char *dirname,FILE *catstream,bool build_icat)
{
    char hexhash[300];
    char fullpath[512];
    char mypath[512];
    char *umypath;
    char timestrbuf[32];
    char sizestrbuf[32];

    WIN32_FIND_DATAA FindFileData;
    HANDLE hFind;
    LARGE_INTEGER filesize;

    if(dirname[0] == '\0' && uc->verbose > 0)
    {
        printf("Scanning directory to build catalog...\n");
        if(uc->guicall)
            fflush(stdout);
    }

    createFullPath(fullpath,basedir,dirname,true);
    if((hFind = FindFirstFileExA(fullpath,FindExInfoStandard,&FindFileData,FindExSearchNameMatch,NULL,0)) != INVALID_HANDLE_VALUE )
    {
        if(uc->verbose > 1 && dirname[0] != '\0')
        {
            printf("%s\n",dirname);
            if(uc->guicall)
                fflush(stdout);
        }

        do
        {
            snprintf(mypath,512,"%s/%s",dirname,FindFileData.cFileName);
            umypath = unifypath(mypath);
            if(!strcmp(basedir,"/") || !strcmp(basedir,"\\"))
                snprintf(fullpath,512,"/%s",umypath);
            else
                snprintf(fullpath,512,"%s/%s",basedir,umypath);

            if( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DEVICE )
                    continue;

            if( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                if(strcmp(FindFileData.cFileName,".") && strcmp(FindFileData.cFileName,".."))
                {
                    if(uc->exclude)
                    {
                        if(needExclude(EXCL_DIR,FindFileData.cFileName))
                            continue;
                        if(needExclude(EXCL_PATH,umypath))
                            continue;
                    }
                    time_to_str_win(&FindFileData.ftLastWriteTime,timestrbuf);
                    if(catstream != NULL)
                    {
                        fputs("D*"      ,catstream);
                        fputs(umypath   ,catstream);
                        fputs("*"       ,catstream);
                        fputs(timestrbuf,catstream);
                        fputs("*\n"     ,catstream);
                    }

                    if(build_icat)
                    {
                        cItem *item = new cItem();
                        item->status = STATUS_NULL;
                        item->size = 0;
                        item->htype = HASH_EMPTY;
                        strcpy(item->pathname,umypath);
                        strcpy(item->time,timestrbuf);
                        catalog_push(&cat_dir,item);
                    }

                    if(scandir_in_win(basedir,umypath,catstream,build_icat))
                        return 1;
                }
            }
            else
            {
                if(uc->exclude)
                    if(needExclude(EXCL_FILE,FindFileData.cFileName))
                        continue;

                filesize.LowPart = FindFileData.nFileSizeLow;
                filesize.HighPart = FindFileData.nFileSizeHigh;
                gethash(fullpath,hexhash,uc->hashmode);
                time_to_str_win(&FindFileData.ftLastWriteTime,timestrbuf);
                snprintf(sizestrbuf,32,"%d",(unsigned int)filesize.QuadPart );
                sizec += ((double)((unsigned int)filesize.QuadPart)) / 1024;

                if(catstream != NULL)
                {
                    fputs("F*"      ,catstream);
                    fputs(umypath   ,catstream);
                    fputs("*"       ,catstream);
                    fputs(timestrbuf,catstream);
                    fputs("*"       ,catstream);
                    fputs(sizestrbuf,catstream);
                    fputs("*"       ,catstream);
                    fputs(hexhash   ,catstream);
                    fputs("*\n"     ,catstream);
                }
                if(build_icat)
                {
                    cItem *item = new cItem();
                    item->status = STATUS_NULL;
                    item->size = (unsigned int)filesize.QuadPart;

                    strcpy(item->pathname,umypath);
                    strcpy(item->time,timestrbuf);
                    item->htype = uc->hashmode;
                    char *hb = hexhash;
                    if(uc->hashmode == HASH_MD5)    hb += 4;
                    if(uc->hashmode == HASH_SHA256) hb += 5;
                    strcpy(item->hash,hb);
                    catalog_push(&cat_file,item);
                }
            }
        }
        while(FindNextFileA(hFind, &FindFileData) != 0);
        FindClose(hFind);
    }
    else
    {
        fprintf(stderr,"Error, FindFirstFileEx call failed: %s (Error code:%d)\n",fullpath,(int)GetLastError());
        if(uc->guicall)
            fflush(stderr);
        return 1;
    }
    return 0;
}
#endif

int UniCatalog::scandir_diff(const char *basedir)
{
#ifdef _WIN32
    if(uc->usestd)
        return scandir_diff_in(basedir,"");
    else
        return scandir_diff_in_win(basedir,"");
#else
    return scandir_diff_in(basedir,"");
#endif

}

int UniCatalog::scandir_diff_in(const char *basedir,const char *dirname)
{
    char hexhash[300];
    char fullpath[512];
    char mypath[512];
    char *umypath;
    char strbuf[32];
    DIR *dir;
    struct dirent *ent;
    struct stat s;

    if(dirname[0] == '\0' && uc->verbose > 0)
    {
        printf("Examine directory to match the catalog...\n");
        if(uc->guicall)
            fflush(stdout);
    }

    createFullPath(fullpath,basedir,dirname,false);
    if ((dir = opendir(fullpath)) != NULL)
    {
        if(uc->verbose > 1 && dirname[0] != '\0')
        {
            printf("%s\n",dirname);
            if(uc->guicall)
                fflush(stdout);
        }
        while ((ent = readdir (dir)) != NULL)
        {
            snprintf(mypath,512,"%s/%s",dirname,ent->d_name);
            umypath = unifypath(mypath);
            if(!strcmp(basedir,"/") || !strcmp(basedir,"\\"))
                snprintf(fullpath,512,"/%s",umypath);
            else
                snprintf(fullpath,512,"%s/%s",basedir,mypath);
            if(!stat(fullpath,&s))
            {
                if( !(s.st_mode & S_IFDIR) && !(s.st_mode & S_IFREG) )
                    continue;
                if( s.st_mode & S_IFDIR )
                {
                    if(strcmp(ent->d_name,".") && strcmp(ent->d_name,".."))
                    {
                        struct cItem *i;

                        if(uc->exclude)
                        {
                            if(needExclude(EXCL_DIR,ent->d_name))
                                continue;
                            if(needExclude(EXCL_PATH,umypath))
                                continue;
                        }

                        i = catalog_search(&cat_dir,umypath);
                        if(i == NULL) //not found in catalog
                        {
                            cItem *item = new cItem();
                            item->status = STATUS_NULL;
                            item->size = 0;
                            item->htype = HASH_EMPTY;
                            strcpy(item->pathname,umypath);
                            time_to_str(&s.st_mtime,strbuf);
                            strcpy(item->time,strbuf);
                            catalog_push(&cat_dir_new,item);
                        }
                        else
                        {
                            i->status = STATUS_MATCH;
                            time_to_str(&s.st_mtime,strbuf);

                            if(i->status == STATUS_MATCH)
                                catalog_move(&cat_dir,i,&cat_dir_ok);
                            else
                                catalog_move(&cat_dir,i,&cat_dir_mod);
                        }

                        if(scandir_diff_in(basedir,umypath))
                            return 1;
                    }
                }
                if(s.st_mode & S_IFREG )
                {
                    struct cItem *i;

                    if(uc->exclude)
                        if(needExclude(EXCL_FILE,ent->d_name))
                            continue;

                    i = catalog_search(&cat_file,umypath);
                    if(i == NULL) //not found in catalog
                    {
                        cItem *item = new cItem();
                        item->status = STATUS_NULL;
                        item->size = (unsigned int)s.st_size;
                        item->htype = HASH_EMPTY;
                        strcpy(item->pathname,umypath);
                        time_to_str(&s.st_mtime,strbuf);
                        strcpy(item->time,strbuf);
                        catalog_push(&cat_file_new,item);
                    }
                    else
                    {
                        bool hash_check_done=false;
                        i->status = STATUS_MATCH;
                        time_to_str(&s.st_mtime,strbuf);

                        if(i->size != (unsigned int)s.st_size)
                            i->status = STATUS_SIZEDIFF;

                        if(i->status == STATUS_MATCH && !uc->skiphash &&
                                (i->htype == HASH_MD5 || i->htype == HASH_SHA256) )
                        {
                            hash_check_done=true;
                            gethash(fullpath,hexhash,i->htype,0);
                            if(strcmp(hexhash,i->hash))
                                i->status = STATUS_HASHDIFF;
                        }

                        if((uc->watchtime || uc->fixmtime) && i->status == STATUS_MATCH && strcmp(i->time,strbuf))
                        {
                            if(hash_check_done && uc->fixmtime )
                            {
                                i->status = STATUS_FIXTIME;
                            }
                            else
                            {
                                if(uc->watchtime)
                                    i->status = STATUS_TIMEDIFF;
                            }
                        }

                        if(i->status == STATUS_MATCH)
                            catalog_delete(&cat_file,i);
                        else if(i->status == STATUS_FIXTIME)
                            catalog_move(&cat_file,i,&cat_file_fixtime);
                        else
                            catalog_move(&cat_file,i,&cat_file_mod);
                    }
                }
            }
            else
            {
                fprintf(stderr,"Error, Cannot stat: %s\n",fullpath);
                if(uc->guicall)
                    fflush(stderr);
                return 1;
            }
        }
        closedir(dir);
    }
    return 0;
}

#ifdef _WIN32
int UniCatalog::scandir_diff_in_win(const char *basedir,const char *dirname)
{
    char hexhash[300];
    char fullpath[512];
    char mypath[512];
    char *umypath;
    char strbuf[32];

    WIN32_FIND_DATAA FindFileData;
    HANDLE hFind;
    LARGE_INTEGER filesize;

    if(dirname[0] == '\0' && uc->verbose > 0)
    {
        printf("Examine directory to match the catalog...\n");
        if(uc->guicall)
            fflush(stdout);
    }

    createFullPath(fullpath,basedir,dirname,true);
    if((hFind = FindFirstFileExA(fullpath,FindExInfoStandard,&FindFileData,FindExSearchNameMatch,NULL,0)) != INVALID_HANDLE_VALUE )
    {
        if(uc->verbose > 1 && dirname[0] != '\0')
        {
            printf("%s\n",dirname);
            if(uc->guicall)
                fflush(stdout);
        }

        do
        {
            snprintf(mypath,512,"%s/%s",dirname,FindFileData.cFileName);
            umypath = unifypath(mypath);
            if(!strcmp(basedir,"/") || !strcmp(basedir,"\\"))
                snprintf(fullpath,512,"/%s",umypath);
            else
                snprintf(fullpath,512,"%s/%s",basedir,mypath);

            if( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DEVICE )
                    continue;

            if( FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                if(strcmp(FindFileData.cFileName,".") && strcmp(FindFileData.cFileName,".."))
                {
                    struct cItem *i;

                    if(uc->exclude)
                    {
                        if(needExclude(EXCL_DIR,FindFileData.cFileName))
                            continue;
                        if(needExclude(EXCL_PATH,umypath))
                            continue;
                    }

                    i = catalog_search(&cat_dir,umypath);
                    if(i == NULL) //not found in catalog
                    {
                        cItem *item = new cItem();
                        item->status = STATUS_NULL;
                        item->size = 0;
                        item->htype = HASH_EMPTY;
                        strcpy(item->pathname,umypath);
                        time_to_str_win(&FindFileData.ftLastWriteTime,strbuf);
                        strcpy(item->time,strbuf);
                        catalog_push(&cat_dir_new,item);
                    }
                    else
                    {
                        i->status = STATUS_MATCH;
                        time_to_str_win(&FindFileData.ftLastWriteTime,strbuf);

                        if(i->status == STATUS_MATCH)
                            catalog_move(&cat_dir,i,&cat_dir_ok);
                        else
                            catalog_move(&cat_dir,i,&cat_dir_mod);
                    }

                    if(scandir_diff_in_win(basedir,umypath))
                        return 1;
                }
            }
            else
            {
                struct cItem *i;

                if(uc->exclude)
                    if(needExclude(EXCL_FILE,FindFileData.cFileName))
                        continue;

                i = catalog_search(&cat_file,umypath);

                filesize.LowPart = FindFileData.nFileSizeLow;
                filesize.HighPart = FindFileData.nFileSizeHigh;

                if(i == NULL) //not found in catalog
                {
                    cItem *item = new cItem();
                    item->status = STATUS_NULL;
                    item->size = (unsigned int)filesize.QuadPart;
                    item->htype = HASH_EMPTY;
                    strcpy(item->pathname,umypath);
                    time_to_str_win(&FindFileData.ftLastWriteTime,strbuf);
                    strcpy(item->time,strbuf);
                    catalog_push(&cat_file_new,item);
                }
                else
                {
                    bool hash_check_done=false;
                    i->status = STATUS_MATCH;
                    time_to_str_win(&FindFileData.ftLastWriteTime,strbuf);

                    if(i->size != (unsigned int)filesize.QuadPart)
                        i->status = STATUS_SIZEDIFF;

                    if(i->status == STATUS_MATCH && !uc->skiphash &&
                            (i->htype == HASH_MD5 || i->htype == HASH_SHA256) )
                    {
                        hash_check_done=true;
                        gethash(fullpath,hexhash,i->htype,0);
                        if(strcmp(hexhash,i->hash))
                            i->status = STATUS_HASHDIFF;
                    }

                    if((uc->watchtime || uc->fixmtime) && i->status == STATUS_MATCH && strcmp(i->time,strbuf))
                    {
                        if(hash_check_done && uc->fixmtime )
                        {
                            i->status = STATUS_FIXTIME;
                        }
                        else
                        {
                            if(uc->watchtime)
                                i->status = STATUS_TIMEDIFF;
                        }
                    }

                    if(i->status == STATUS_MATCH)
                        catalog_delete(&cat_file,i);
                    else if(i->status == STATUS_FIXTIME)
                        catalog_move(&cat_file,i,&cat_file_fixtime);
                    else
                        catalog_move(&cat_file,i,&cat_file_mod);
                }
            }

        }
        while(FindNextFileA(hFind, &FindFileData) != 0);
        FindClose(hFind);
    }
    return 0;
}
#endif

void UniCatalog::rawPrint(void)
{
    struct cItem *r;

    r = cat_file;
    while(r != NULL)
    {
        printf("FILE-RAW: %s (%d bytes)\n",r->pathname,r->size);
        r = r->n;
    }
    r = cat_file_ok;
    while(r != NULL)
    {
        printf("FILE-OK : %s (%d bytes)\n",r->pathname,r->size);
        r = r->n;
    }
    r = cat_file_new;
    while(r != NULL)
    {
        printf("FILE-NEW: %s (%d bytes)\n",r->pathname,r->size);
        r = r->n;
    }
    r = cat_file_mod;
    while(r != NULL)
    {
        printf("FILE-MOD: %s (%d bytes) ",r->pathname,r->size);
        if(r->status == STATUS_NULL)     printf("STATUS:NULL");
        if(r->status == STATUS_TIMEDIFF) printf("STATUS:TIMEDIFF");
        if(r->status == STATUS_HASHDIFF) printf("STATUS:HASHDIFF");
        if(r->status == STATUS_SIZEDIFF) printf("STATUS:SIZEDIFF");
        if(r->status == STATUS_MATCH)    printf("STATUS:MATCH");
        printf("\n");
        r = r->n;
    }
    r = cat_file_fixtime;
    while(r != NULL)
    {
        printf("FILE-FIXTIME: %s (%d bytes)\n",r->pathname,r->size);
        r = r->n;
    }

    r = cat_dir;
    while(r != NULL)
    {
        printf("DIR-RAW: %s \n",r->pathname);
        r = r->n;
    }
    r = cat_dir_ok;
    while(r != NULL)
    {
        printf("DIR-OK : %s \n",r->pathname);
        r = r->n;
    }
    r = cat_dir_new;
    while(r != NULL)
    {
        printf("DIR-NEW: %s \n",r->pathname);
        r = r->n;
    }
    r = cat_dir_mod;
    while(r != NULL)
    {
        printf("DIR-MOD: %s ",r->pathname);
        if(r->status == STATUS_NULL)     printf("STATUS:NULL");
        if(r->status == STATUS_TIMEDIFF) printf("STATUS:TIMEDIFF");
        if(r->status == STATUS_HASHDIFF) printf("STATUS:HASHDIFF");
        if(r->status == STATUS_SIZEDIFF) printf("STATUS:SIZEDIFF");
        if(r->status == STATUS_MATCH)    printf("STATUS:MATCH");
        printf("\n");
        r = r->n;
    }
}

void UniCatalog::diffresultPrint(void)
{
    char sb[128];
    bool pe = false;
    struct cItem *r;

    r = cat_dir;
    while(r != NULL)
    {
        printf("DELETED FOLDER: %s\n",r->pathname);
        if(uc->guicall)
            fflush(stdout);
        pe = true;
        r = r->n;
    }

    r = cat_file;
    while(r != NULL)
    {
        my_dtoa((double)(r->size),sb,128,0,0,1);
        printf("DELETED FILE: %s (%s bytes)\n",r->pathname,sb);
        if(uc->guicall)
            fflush(stdout);
        pe = true;
        r = r->n;
    }

    r = cat_dir_new;
    while(r != NULL)
    {
        printf("NEW FOLDER: %s\n",r->pathname);
        if(uc->guicall)
            fflush(stdout);
        pe = true;
        r = r->n;
    }

    r = cat_file_new;
    while(r != NULL)
    {
        my_dtoa((double)(r->size),sb,128,0,0,1);
        printf("NEW FILE: %s (%s bytes)\n",r->pathname,sb);
        if(uc->guicall)
            fflush(stdout);
        pe = true;
        r = r->n;
    }

    r = cat_dir_mod;
    while(r != NULL)
    {
        printf("MODIFIED FOLDER: %s ",r->pathname);
        if(r->status == STATUS_TIMEDIFF) printf("(time changed)");
        if(r->status == STATUS_HASHDIFF) printf("(hash differs)");
        if(r->status == STATUS_SIZEDIFF) printf("(size differs)");
        printf("\n");
        if(uc->guicall)
            fflush(stdout);
        pe = true;
        r = r->n;
    }

    r = cat_file_mod;
    while(r != NULL)
    {
        printf("MODIFIED FILE: %s ",r->pathname);
        if(r->status == STATUS_TIMEDIFF) printf("(time changed)");
        if(r->status == STATUS_HASHDIFF) printf("(hash differs)");
        if(r->status == STATUS_SIZEDIFF) printf("(size differs)");
        printf("\n");
        if(uc->guicall)
            fflush(stdout);
        pe = true;
        r = r->n;
    }

    if(!pe)
    {
        printf("The folders seem to be identical\n");
        if(uc->guicall)
            fflush(stdout);
    }
}

char *wods(char *strptr)
{
    char *b;
    for(b = strptr ; (b[0] == '/' || b[0] == '\\') && b[0] != '\0' ; ++b );
    return b;
}

int UniCatalog::print_sync_procedures(const char *sourcefolder_bp,const char *targetfolder_bp,int direction)
{
    int cnt;
    int all;
    double csize;
    struct cItem *r;

    all = 0;

    printf("-------------------------\nRequired actions to sync:\n");
    if(uc->guicall)
        fflush(stdout);

    r = cat_file_fixtime;
    cnt = 0;
    while(r != NULL)
    {
        ++cnt;
        r = r->n;
    }
    all += cnt;
    if(cnt > 0)
        printf(" FILE-FIX-TIMES: \"%s\" -> %d file(s) -> \"%s\"\n",sourcefolder_bp,cnt,targetfolder_bp);

    r = (direction == DIRECTION_CAT_TO_DIFF ? cat_file_new : cat_file);
    if(r != NULL)
        while(r->n != NULL)
            r = r->n;
    cnt = 0;
    while(r != NULL)
    {
        ++cnt;
        r = r->p;
    }
    all += cnt;
    if(cnt > 0)
        printf(" DELETE FILES: %d file(s) -> \"%s\"\n",cnt,targetfolder_bp);

    r = (direction == DIRECTION_CAT_TO_DIFF ? cat_dir_new : cat_dir);
    if(r != NULL)
        while(r->n != NULL)
            r = r->n;
    cnt = 0;
    while(r != NULL)
    {
        ++cnt;
        r = r->p;
    }
    all += cnt;
    if(cnt > 0)
        printf(" DELETE FOLDERS: %d folder(s) -> \"%s\"\n",cnt,targetfolder_bp);

    r = (direction == DIRECTION_CAT_TO_DIFF ? cat_dir: cat_dir_new);
    cnt = 0;
    while(r != NULL)
    {
        ++cnt;
        r = r->n;
    }
    all += cnt;
    if(cnt > 0)
        printf(" COPY FOLDERS: \"%s\" -> %d folder(s) -> \"%s\"\n",sourcefolder_bp,cnt,targetfolder_bp);

    r = (direction == DIRECTION_CAT_TO_DIFF ? cat_file: cat_file_new);
    cnt = 0;
    csize = 0.0;
    while(r != NULL)
    {
        ++cnt;
        csize += r->size;
        r = r->n;
    }
    all += cnt;
    if(cnt > 0)
    {
        char buff[64];
        csize /= 1024 * 1024;
        my_dtoa(csize,(char *)buff,64,0,2,1);
        printf(" COPY MISSING FILES: \"%s\" -> %d file(s) / %s Mb -> \"%s\"\n",sourcefolder_bp,cnt,buff,targetfolder_bp);
    }

    r = cat_file_mod;
    cnt = 0;
    csize = 0.0;
    while(r != NULL)
    {
        ++cnt;
        csize += r->size;
        r = r->n;
    }
    all += cnt;
    if(cnt > 0)
    {
        char buff[64];
        csize /= 1024 * 1024;
        my_dtoa(csize,(char *)buff,64,0,2,1);
        printf(" COPY MODIFIED FILES: \"%s\" -> %d file(s) / %s Mb -> \"%s\"\n",sourcefolder_bp,cnt,buff,targetfolder_bp);
    }

    if(all == 0)
        printf(" - nothing - \n");

    return 0;
}

int UniCatalog::scandir_sync(const char *sourcefolder_bp,const char *targetfolder_bp,int direction)
{
    char srcbuf[512];
    char dstbuf[512];

    struct cItem *r;

    if(uc->verbose > 0)
    {
        printf("Sync directory...\n");
        if(uc->guicall)
            fflush(stdout);
    }

    if(PathMaker::mkpath(targetfolder_bp,false))
        return 1;

    FileCopier *copier = new FileCopier(uc);

    r = cat_file_fixtime;
    while(r != NULL)
    {
        snprintf(srcbuf,512,"%s/%s",sourcefolder_bp,wods(r->pathname));
        snprintf(dstbuf,512,"%s/%s",targetfolder_bp,wods(r->pathname));
        if(copier->fixtime(srcbuf,dstbuf))
        {
            delete copier;
            return 1;
        }
        r = r->n;
    }

    r = (direction == DIRECTION_CAT_TO_DIFF ? cat_file_new : cat_file);
    if(r != NULL)
        while(r->n != NULL)
            r = r->n;
    while(r != NULL)
    {
        snprintf(dstbuf,512,"%s/%s",targetfolder_bp,wods(r->pathname));
        if(copier->deletefile(dstbuf) != 0)
        {
            fprintf(stderr,"Error, cannot delete file: %s\n",dstbuf);
            delete copier;
            return 1;
        }
        r = r->p;
    }

    r = (direction == DIRECTION_CAT_TO_DIFF ? cat_dir_new : cat_dir);
    if(r != NULL)
        while(r->n != NULL)
            r = r->n;
    while(r != NULL)
    {
        snprintf(dstbuf,512,"%s/%s",targetfolder_bp,wods(r->pathname));
        if(copier->deletefolder(dstbuf) != 0)
        {
            fprintf(stderr,"Error, cannot delete folder: %s\n",dstbuf);
            delete copier;
            return 1;
        }
        r = r->p;
    }

    r = (direction == DIRECTION_CAT_TO_DIFF ? cat_dir: cat_dir_new);
    while(r != NULL)
    {
        snprintf(srcbuf,512,"%s/%s",sourcefolder_bp,wods(r->pathname));
        snprintf(dstbuf,512,"%s/%s",targetfolder_bp,wods(r->pathname));
        if(PathMaker::mkpath(dstbuf,false))
        {
            delete copier;
            return 1;
        }
        r = r->n;
    }

    r = (direction == DIRECTION_CAT_TO_DIFF ? cat_file: cat_file_new);
    while(r != NULL)
    {
        snprintf(srcbuf,512,"%s/%s",sourcefolder_bp,wods(r->pathname));
        snprintf(dstbuf,512,"%s/%s",targetfolder_bp,wods(r->pathname));
        if(copier->copy(srcbuf,dstbuf))
        {
            delete copier;
            return 1;
        }
        r = r->n;
    }

    r = cat_file_mod;
    while(r != NULL)
    {
        snprintf(srcbuf,512,"%s/%s",sourcefolder_bp,wods(r->pathname));
        snprintf(dstbuf,512,"%s/%s",targetfolder_bp,wods(r->pathname));
        if(copier->copy(srcbuf,dstbuf))
        {
            delete copier;
            return 1;
        }
        r = r->n;
    }

    copier->printStatistics();
    delete copier;
    return 0;
}

/*  Make an update which update the cataloged folder to the diffed before this func.
    sourcefolder_bp is the basepath of the diffed directory
    updatepack_bp is the basepath of update package
    DIFFED = CATALOGED + THIS UPDATE */
int UniCatalog::make_update_package(const char *sourcefolder_bp,const char *updatepack_bp)
{
    char srcbuf[512];
    char dstbuf[512];

    struct cItem *r;

    if(uc->verbose > 0)
    {
        printf("Generate the update package...\n");
        if(uc->guicall)
            fflush(stdout);
    }

    if(PathMaker::mkpath(updatepack_bp,false))
        return 1;

    FILE *df=NULL;
    snprintf(dstbuf,512,"%s/.deleted_items",updatepack_bp);
    if((df=fopen(dstbuf,"w"))==NULL)
    {
        fprintf(stderr,"Error, cannot write file: %s\n",dstbuf);
        return 1;
    }

    r = cat_file;
    if(r != NULL)
        while(r->n != NULL)
            r = r->n;
    while(r != NULL)
    {
        fprintf(df,"F:%s\n",wods(r->pathname));
        r = r->p;
    }

    r = cat_dir;
    if(r != NULL)
        while(r->n != NULL)
            r = r->n;
    while(r != NULL)
    {
        fprintf(df,"D:%s\n",wods(r->pathname));
        r = r->p;
    }

    fclose(df);

    FileCopier *copier = new FileCopier(uc);
    r = cat_dir_new;
    while(r != NULL)
    {
        snprintf(dstbuf,512,"%s/%s",updatepack_bp,wods(r->pathname));
        if(PathMaker::mkpath(dstbuf,false))
        {
            delete copier;
            return 1;
        }
        r = r->n;
    }

    r = cat_file_new;
    while(r != NULL)
    {
        snprintf(srcbuf,512,"%s/%s",sourcefolder_bp,wods(r->pathname));
        snprintf(dstbuf,512,"%s/%s",updatepack_bp,wods(r->pathname));
        if(copier->copy(srcbuf,dstbuf))
        {
            delete copier;
            return 1;
        }
        r = r->n;
    }

    r = cat_file_mod;
    while(r != NULL)
    {
        snprintf(srcbuf,512,"%s/%s",sourcefolder_bp,wods(r->pathname));
        snprintf(dstbuf,512,"%s/%s",updatepack_bp,wods(r->pathname));
        if(copier->copy(srcbuf,dstbuf))
        {
            delete copier;
            return 1;
        }
        r = r->n;
    }
    copier->printStatistics();
    delete copier;
    return 0;
}

int UniCatalog::apply_update_package(const char *updatepack_bp,const char *targetfolder_bp)
{
    char srcbuf[512];
    char dstbuf[512];
    char buffer[1024];
    struct cItem *i,*r;

    if(uc->verbose > 0)
    {
        printf("Apply the update package...\n");
        if(uc->guicall)
            fflush(stdout);
    }
    uc->save();
    uc->verbose = 0;
    uc->watchtime = 0;
    uc->hashmode = HASH_EMPTY;
    clear();
    scandir(updatepack_bp,NULL,true);
    strcpy(buffer,".deleted_items");
    i = catalog_search(&cat_file,buffer);
    if(i == NULL)
    {
        fprintf(stderr,"Error, missing .deleted_items file!");
        clear();
        return 1;
    }
    catalog_delete(&cat_file,i);
    uc->restore();

    if(PathMaker::mkpath(targetfolder_bp,false))
        return 1;

    FileCopier *copier = new FileCopier(uc);

    FILE *ef=NULL;
    snprintf(srcbuf,512,"%s/.deleted_items",updatepack_bp);
    ef = fopen(srcbuf,"r");
    if(ef == NULL)
    {
        fprintf(stderr,"Error, cannot open .deleted_items file!");
        clear();
        delete copier;
        return 1;
    }
    while(!feof(ef))
    {
        if(fgets(buffer,1024,ef)!=NULL)
        {
            chop(buffer);
            if(!strncmp(buffer,"F:",2))
            {
                snprintf(dstbuf,512,"%s/%s",targetfolder_bp,buffer+2);
                if(copier->deletefile(dstbuf) != 0)
                {
                    fprintf(stderr,"Error, cannot delete file: %s\n",dstbuf);
                    delete copier;
                    fclose(ef);
                    return 1;
                }
            }
            if(!strncmp(buffer,"D:",2))
            {
                snprintf(dstbuf,512,"%s/%s",targetfolder_bp,buffer+2);
                if(copier->deletefolder(dstbuf) != 0)
                {
                    fprintf(stderr,"Error, cannot delete folder: %s\n",dstbuf);
                    delete copier;
                    fclose(ef);
                    return 1;
                }
            }
        }
    }
    fclose(ef);

    r = cat_dir;
    while(r != NULL)
    {
        snprintf(dstbuf,512,"%s/%s",targetfolder_bp,wods(r->pathname));
        if(PathMaker::mkpath(dstbuf,false))
        {
            delete copier;
            return 1;
        }
        r = r->n;
    }

    r = cat_file;
    while(r != NULL)
    {
        snprintf(srcbuf,512,"%s/%s",updatepack_bp,wods(r->pathname));
        snprintf(dstbuf,512,"%s/%s",targetfolder_bp,wods(r->pathname));
        if(copier->copy(srcbuf,dstbuf))
        {
            delete copier;
            return 1;
        }
        r = r->n;
    }
    copier->printStatistics();
    delete copier;
    return 0;
}

/* ******************************************************************************** */
void UniCatalog::free_catalog(struct cItem** cpointer)
{
    struct cItem *old,*r=*cpointer;
    while(r != NULL)
    {
        old = r;
        r = r->n;
        delete old;
    }
    *cpointer = NULL;
}

void UniCatalog::catalog_push(struct cItem** cpointer,struct cItem *item)
{
    item->n = NULL;
    if(*cpointer == NULL)
    {
        *cpointer = item;
        (*cpointer)->p = NULL;
    }
    else
    {
        struct cItem *r=*cpointer;
        while(r->n != NULL)
            r = r->n;
        r->n = item;
        item->p = r;
    }
}

struct cItem * UniCatalog::catalog_search(struct cItem** cat,char *pathname)
{
    struct cItem *r=*cat;
    while(r != NULL)
    {
        if(!strcmp(pathname,r->pathname))
            return r;
        r = r->n;
    }
    return NULL;
}

void UniCatalog::catalog_delete(struct cItem** fromcatalog,struct cItem* item)
{
    if(item == *fromcatalog)
    {
        *fromcatalog = item->n;
        if(item->n != NULL)
            item->n->p = *fromcatalog;
        if(*fromcatalog != NULL)
            (*fromcatalog)->p = NULL;
        delete item;
    }
    else
    {
        item->p->n = item->n;
        if(item->n != NULL)
            item->n->p = item->p;
        delete item;
    }
}

void UniCatalog::catalog_move(struct cItem** fromcatalog,struct cItem* item,struct cItem** targetcatalog)
{
    if(item == *fromcatalog)
    {
        *fromcatalog = item->n;
        if(item->n != NULL)
            item->n->p = *fromcatalog;
        if(*fromcatalog != NULL)
            (*fromcatalog)->p = NULL;

    }
    else
    {
        item->p->n = item->n;
        if(item->n != NULL)
            item->n->p = item->p;
    }
    catalog_push(targetcatalog,item);
}

void UniCatalog::clear(void)
{
    free_catalog(&cat_file);
    free_catalog(&cat_file_ok);
    free_catalog(&cat_file_mod);
    free_catalog(&cat_file_new);
    free_catalog(&cat_file_fixtime);
    free_catalog(&cat_dir);
    free_catalog(&cat_dir_ok);
    free_catalog(&cat_dir_mod);
    free_catalog(&cat_dir_new);
}

/* end code */
