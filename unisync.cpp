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
#include <sys/time.h>
#include <utime.h>

#include "unisync.h"
#include "utils.h"
#include "catalog.h"

#ifdef _WIN32
#include <windows.h>
#include <locale.h>
#endif

int printhelp(void)
{
    printf("%s - Universal offline/online direcotry sync/diff utility\n\n",PROGRAMNAME);
    printf("  Usage: %s <command> [switches] [other parameters]...\n",PROGRAMCMD);
    printf("    \n");
    printf("Commands:\n");
    printf("  create - Create a catalog file from a directory\n");
    printf("    %s create cat:CATALOGFILE SOURCE_DIRECOTRY [switches]\n",PROGRAMCMD);
    printf("    %s create cat:./mycatalog.usc /STORE/MyPics -md5 -v \n",PROGRAMCMD);
    printf("    %s create cat:./mycatalog.usc \"/Dir with spaces/a\" -md5\n",PROGRAMCMD);
    printf("    \n");
    printf("  diff - Compares two directories and print the differences\n");
    printf("    %s diff SOURCE_DIRECOTRY DESTINATION_DIRECTORY [switches]\n",PROGRAMCMD);
    printf("    %s diff /STORE/MyPics /STORE/BackupMyPics -md5 -vv \n",PROGRAMCMD);
    printf("    \n");
    printf("  catdiff - Compares a directory to catalog\n");
    printf("    %s catdiff cat:CATALOGFILE SOURCE_DIRECOTRY [switches]\n",PROGRAMCMD);
    printf("    %s catdiff cat:./mycatalog.usc /STORE/BackupMyPics -md5 -v \n",PROGRAMCMD);
    printf("    %s catdiff cat:\"./my pics\" /STORE/BackupMyPics -md5 -vv \n",PROGRAMCMD);
    printf("    \n");
    printf("  sync - Syncronize a directory to a directory\n");
    printf("    %s sync SOURCE_DIRECOTRY DESTINATION_DIRECTORY [switches]\n",PROGRAMCMD);
    printf("    %s sync /STORE/MyPics /STORE/BackupMyPics -md5 -vv \n",PROGRAMCMD);
    printf("    %s sync /STORE/MyPics /STORE/BackupMyPics -exclf=Thumbs.db -v\n",PROGRAMCMD);
    printf("    %s sync /STORE/MyPics /STORE/BackupMyPics -exclf=Thumbs.db -i -vv\n",PROGRAMCMD);
    printf("    \n");
    printf("  makeupdate - Create an update package to sync offline directories\n");
    printf("    %s makeupdate cat:CATALOGFILE SOURCE_DIRECOTRY update:UPDATEDIR\n",PROGRAMCMD);
    printf("    %s makeupdate cat:./mycatalog.usc /STORE/MyPics update:/media/pen/upd\n",PROGRAMCMD);
    printf("    \n");
    printf("  makesyncupdate - Create an update package to sync online directories\n");
    printf("    %s makeupdate SOURCE_DIRECOTRY DESTINATION_DIRECTORY update:UPDATEDIR\n",PROGRAMCMD);
    printf("    %s makeupdate /STORE/MyPics /STORE/BackupMyPics update:/media/pen/upd\n",PROGRAMCMD);
    printf("    \n");
    printf("  applyupdate - Apply update package to sync directories\n");
    printf("    %s makeupdate SOURCE_DIRECOTRY update:UPDATEDIR\n",PROGRAMCMD);
    printf("    %s makeupdate /STORE/BackupMyPics update:/media/pen/upd\n",PROGRAMCMD);
    printf("    %s makeupdate /STORE/BackupMyPics update:\"update package\" -vv\n",PROGRAMCMD);
    printf("    \n");
    printf("Switches:\n");
    printf(" -v          - Be verbose\n");
    printf(" -vv         - Be more verbose\n");
    printf(" -md5        - Generate md5 hash to check the file's contents\n");
    printf(" -sha2       - Generate sha256 hash to check the file's contents\n");
    printf(" -nohash     - Don't generate any hash (default)\n");
    printf(" -mtime      - Check/Compare modification times of files\n");
    printf(" -fixtime    - Only in SYNC mode: Fixing file times instead of copy\n");
    printf("               if the files appears to be same according to hashes.\n");
    printf("               (This switch only works with hashes and command=sync)\n");
    printf(" -skiphash   - Don't check hashes even if the catalog contains its.\n");
    printf(" -exclf=EXF  - Exclude file named EXF from every work\n");
    printf(" -excld=EXD  - Exclude directory named EXD from every work\n");
    printf(" -exclp=EXP  - Exclude path matched EXP from every work\n");
    printf(" -std        - Use standard POSIX routines instead of platform dependent codes.\n");
    printf(" -i          - Interactive/paranoid sync mode. Print info and ask before sync.\n");
    printf(" -h          - Print help\n");
    printf(" -version    - Version and author informations\n");
    return 0;
}

int printversionabout(void)
{
    printf("%s - Universal Direcotry Sync-Diff utility\n",PROGRAMNAME);
    printf("Author: Peter Deak (hyper80@gmail.com) http://hyperprog.com/unisync\n");
    printf("Version: %s\n",VERSION);
    printf("License: GPLv2\n");
    return 0;
}

void specify(char *val,const char *name);
void specify_and_canopen(char *val,const char *name);
void dontspecify(char *val,const char *name);

int main(int argi,char **argc)
{
    int r;
    if(argi <= 1)
    {
        printhelp();
        return 0;
    }

    char command[512];
    char sourcedir[512];
    char destdir[512];
    char catalogfile[512];
    char updatedir[512];

    strcpy(command,"");
    strcpy(sourcedir,"");
    strcpy(destdir,"");
    strcpy(catalogfile,"");
    strcpy(updatedir,"");

    int spc = 0;
    char *simpleparams[3];
    simpleparams[0] = command;
    simpleparams[1] = sourcedir;
    simpleparams[2] = destdir;

    UniSyncConfig config;
    int p;
    for(p = 1 ; p < argi ; ++p)
    {
        if(!strcmp(argc[p],"-h") || !strcmp(argc[p],"-help"))
            return printhelp();

        if(!strcmp(argc[p],"-version"))
            return printversionabout();

        if(!strcmp(argc[p],"-v"))
        {
            config.verbose = 1;
            continue;
        }
        if(!strcmp(argc[p],"-vv"))
        {
            config.verbose = 2;
            continue;
        }
        if(!strcmp(argc[p],"-vvv"))
        {
            config.verbose = 3;
            continue;
        }
        if(!strcmp(argc[p],"-md5"))
        {
            config.hashmode = HASH_MD5;
            continue;
        }
        if(!strcmp(argc[p],"-sha2"))
        {
            config.hashmode = HASH_SHA256;
            continue;
        }
        if(!strcmp(argc[p],"-nohash"))
        {
            config.hashmode = HASH_EMPTY;
            continue;
        }
        if(!strcmp(argc[p],"-mtime"))
        {
            config.watchtime = 1;
            continue;
        }
        if(!strcmp(argc[p],"-skiphash"))
        {
            config.skiphash = 1;
            continue;
        }
        if(!strcmp(argc[p],"-fixtime"))
        {
            config.fixmtime = 1;
            continue;
        }
        if(!strcmp(argc[p],"-std"))
        {
            config.usestd = 1;
            continue;
        }
        if(!strcmp(argc[p],"-i"))
        {
            config.interactivesync = 1;
            continue;
        }

        if(!strcmp(argc[p],"-guicall"))
        {
            config.guicall = 1;
            continue;
        }
        if(!strncmp(argc[p],"-exclf",6) || !strncmp(argc[p],"-excld",6) || !strncmp(argc[p],"-exclp",6))
        {
            if(argc[p][6] != '=')
            {
                fprintf(stderr,"Error, Exclude must specified with = sign ( -exclf=file.ext )\n");
                return 1;
            }

            config.exclude = 1;
            ExcludeNames *e=new ExcludeNames();
            if(argc[p][5] == 'f')
                e->typ = EXCL_FILE;
            if(argc[p][5] == 'd')
                e->typ = EXCL_DIR;
            if(argc[p][5] == 'p')
                e->typ = EXCL_PATH;

            strncpy(e->name,argc[p]+7,254);
            if(e->typ == EXCL_PATH)
                unifypath(e->name);

            e->n = NULL;
            if(config.exl == NULL)
            {
                config.exl = e;
            }
            else
            {
                ExcludeNames *r=config.exl;
                while(r->n != NULL)
                    r = r->n;
                r->n = e;
            }
            continue;
        }
        if(!strncmp(argc[p],"-wl",3))
        {
            if(argc[p][3] != '=')
            {
                fprintf(stderr,"Error, Windows locale must specified with = sign ( -wl=locale )\n");
                return 1;
            }

            char slocale[32];
            slocale[0]='.';
            strncpy(slocale+1,argc[p]+4,30);
            #ifdef _WIN32
            if(strcmp(slocale+1,"info")!=0)
            {
                printf("Set locale to: %s\n",slocale+1);
                setlocale(LC_ALL,slocale);
            }
            printf("Locale is: %s\n",setlocale(LC_ALL,NULL));
            #else
            printf("Info: -wl, Windows locale switch is unaffected now.\n");
            #endif
            continue;
        }
        if(!strncmp(argc[p],"cat:",4))
        {
            strncpy(catalogfile,argc[p]+4,510);
            int l=strlen(catalogfile);
            if(l<4 || strcmp(catalogfile+l-4,".usc"))
                strcpy(catalogfile+l,".usc");
            continue;
        }
        if(!strncmp(argc[p],"update:",7))
        {
            strncpy(updatedir,argc[p]+7,510);
            continue;
        }
        if(!strncmp(argc[p],"-",1))
        {
            fprintf(stderr,"Error, unknown switch: \"%s\"\n",argc[p]);
            return 1;
        }
        if(spc < 3)
        {
            strncpy(simpleparams[spc],argc[p],510);
            ++spc;
        }
        else
        {
            fprintf(stderr,"Error, too much parameter passed.\n");
            return 1;
        }
    }

    if(!strcmp(command,"help"))
        return printhelp();
    if(!strcmp(command,"version") || !strcmp(command,"about"))
        return printversionabout();

    trimenddir(sourcedir);
    trimenddir(destdir);
    trimenddir(updatedir);

    if(config.verbose > 1)
    {
        printf("----- PARAMETERS -----\nCommand: %s\nSource: %s\nDestination: %s\nUpdate: %s\nCatalog: %s\n",
                command,sourcedir,destdir,updatedir,catalogfile);
        if(config.exclude)
        {
            ExcludeNames *r=config.exl;
            while(r!= NULL)
            {
                printf("Exclude %s: %s\n",
                        r->typ == EXCL_FILE ? "file" :(r->typ == EXCL_DIR ? "directory" : "path"),
                        r->name);
                r=r->n;
            }
        }
        printf("--- END PARAMETERS ---\n");
        if(config.guicall)
            fflush(stdout);
    }

    // **********************************************************************
    //Switch off fixtime switch in some unwanted situation...
    if(config.fixmtime && strcmp(command,"sync")) // ...when not SYNC command requested
        config.fixmtime = 0;
    if(config.fixmtime && config.skiphash) // ...when skiphash is enabled
        config.fixmtime = 0;
    // **********************************************************************
    if(!strcmp(command,"create"))
    {
        specify(catalogfile,"catalog file");
        specify_and_canopen(sourcedir,"source directory");
        dontspecify(destdir,"directory");
        dontspecify(updatedir,"parameter");

        FILE *catf=NULL;
        catf = fopen(catalogfile,"w");
        if(catf == NULL)
        {
            fprintf(stderr,"Error, Cannot open catalog file for writing: %s\n",catalogfile);
            return 1;
        }

        UniCatalog *catalog = new UniCatalog(&config);
        r = catalog->scandir(sourcedir,catf,false);
        fclose(catf);
        delete catalog;
        return r;
    }
    // **********************************************************************
    if(!strcmp(command,"diff"))
    {
        specify_and_canopen(sourcedir,"source directory");
        specify_and_canopen(destdir,"destination directory");
        dontspecify(catalogfile,"parameter");
        dontspecify(updatedir,"parameter");

        UniCatalog *catalog = new UniCatalog(&config);
        r = catalog->scandir(sourcedir,NULL,true);
        if(r != 0) { delete catalog; return 1; }

        r = catalog->scandir_diff(destdir);
        if(r != 0) { delete catalog; return 1; }

        catalog->diffresultPrint();
        delete catalog;
        return 0;
    }
    // **********************************************************************
    if(!strcmp(command,"catdiff"))
    {
        specify_and_canopen(sourcedir,"source directory");
        specify(catalogfile,"catalog file");
        dontspecify(destdir,"directory");
        dontspecify(updatedir,"parameter");

        UniCatalog *catalog = new UniCatalog(&config);
        r = catalog->read(catalogfile);
        if(r != 0) { delete catalog; return 1; }

        r = catalog->scandir_diff(sourcedir);
        if(r != 0) { delete catalog; return 1; }

        catalog->diffresultPrint();
        delete catalog;
        return 0;
    }
    // **********************************************************************
    if(!strcmp(command,"sync"))
    {
        specify_and_canopen(sourcedir,"source directory");
        specify(destdir,"destination directory");
        dontspecify(catalogfile,"parameter");
        dontspecify(updatedir,"parameter");

        UniCatalog *catalog = new UniCatalog(&config);
        r = catalog->scandir(sourcedir,NULL,true);
        if(r != 0) { delete catalog; return 1; }

        r = catalog->scandir_diff(destdir);
        if(r != 0) { delete catalog; return 1; }

        if(config.interactivesync)
        {
            r = catalog->print_sync_procedures(sourcedir,destdir,DIRECTION_CAT_TO_DIFF);
            char ch;
            printf("Do you really want to start the sync? [y/n]\n");
            ch = read_and_echo_character();
            if(ch != 'y')
            {
                printf("\nSync aborted.\n");
                delete catalog;
                return 0;
            }
        }

        r = catalog->scandir_sync(sourcedir,destdir,DIRECTION_CAT_TO_DIFF);

        delete catalog;
        return r;
    }
    // **********************************************************************
    if(!strcmp(command,"makeupdate"))
    {
        specify_and_canopen(sourcedir,"source directory");
        specify(catalogfile,"catalog file");
        specify(updatedir,"update directory");
        dontspecify(destdir,"directory");

        UniCatalog *catalog = new UniCatalog(&config);
        r = catalog->read(catalogfile);
        if(r != 0) { delete catalog; return 1; }

        r = catalog->scandir_diff(sourcedir);
        if(r != 0) { delete catalog; return 1; }

        r = catalog->make_update_package(sourcedir,updatedir);
        delete catalog;
        return r;
    }
    // **********************************************************************
    if(!strcmp(command,"makesyncupdate"))
    {
        specify_and_canopen(sourcedir,"source directory");
        specify_and_canopen(destdir,"destination directory");
        specify(updatedir,"update directory");
        dontspecify(catalogfile,"parameter");

        UniCatalog *catalog = new UniCatalog(&config);
        r = catalog->scandir(destdir,NULL,true);
        if(r != 0) { delete catalog; return 1; }

        r = catalog->scandir_diff(sourcedir);
        if(r != 0) { delete catalog; return 1; }

        r = catalog->make_update_package(sourcedir,updatedir);
        delete catalog;
        return r;
    }
    // **********************************************************************
    if(!strcmp(command,"applyupdate"))
    {
        specify_and_canopen(sourcedir,"source directory");
        specify_and_canopen(updatedir,"update directory");
        dontspecify(destdir,"directory");
        dontspecify(catalogfile,"parameter");

        UniCatalog *catalog = new UniCatalog(&config);
        r = catalog->apply_update_package(updatedir,sourcedir);
        delete catalog;
        return r;
    }

    fprintf(stderr,"Error, unknown command: %s\n",command);
    return 1;
}

void specify(char *val,const char *name)
{
    if(strlen(val) == 0)
    {
        fprintf(stderr,"Error, You have to specify the %s!\n",name);
        exit(1);
    }
}

void specify_and_canopen(char *val,const char *name)
{
    char val_to_check[512];
    strcpy(val_to_check,val);

#ifdef _WIN32
    //On windows: If the string is a drive letter, for example "D:" append the slash sign to the end
    // (it was removed by trimenddir before)
    if(strlen(val_to_check) == 2 && isalpha(val_to_check[0]) && val_to_check[1] == ':')
    {
        val_to_check[2] = '\\';
        val_to_check[3] = '\0';

    }
#endif

    struct stat s;
    if(strlen(val_to_check) == 0 || stat(val_to_check,&s) != 0)
    {
        fprintf(stderr,"Error, The %s does not specified, not exists or cannot be opened!\n",name);
        exit(1);
    }
}

void dontspecify(char *val,const char *name)
{
    if(strlen(val) > 0)
    {
        fprintf(stderr,"Error, Too much %s specified!\n",name);
        exit(1);
    }
}

UniSyncConfig::UniSyncConfig(void)
{
    guicall = 0;
    verbose = 0;
    watchtime = 0;
    hashmode = HASH_EMPTY;
    skiphash = 0;
    exclude = 0;
    fixmtime = 0;
    usestd = 0;
    interactivesync = 0;
    exl = NULL;
}

void UniSyncConfig::save(void)
{
    s_verbose = verbose;
    s_hashmode = hashmode;
    s_watchtime = watchtime;
    s_skiphash = skiphash;
    s_exclude = exclude;
    s_fixmtime = fixmtime;
}

void UniSyncConfig::restore(void)
{
    verbose = s_verbose;
    hashmode = s_hashmode;
    watchtime = s_watchtime;
    skiphash = s_skiphash;
    exclude = s_exclude;
    fixmtime = s_fixmtime;
}

/* end code */
