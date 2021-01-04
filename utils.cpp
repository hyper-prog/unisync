/* **********************************************************
    UniSync - Universal direcotry sync-diff utility
     http://hyperprog.com

    (C) 2014-2019 Peter Deak (hyper80@gmail.com)

    License: GPLv2  http://www.gnu.org/licenses/gpl-2.0.html
************************************************************* */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <utime.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <sys/sendfile.h>
#endif

#include "utils.h"

#include "sha2.c"
#include "md5.c"

int mymkdir(const char *dirname)
{
#ifdef __WIN32
    return mkdir(dirname);
#else
    return mkdir(dirname,0775);
#endif
}

/* Converts an integer to hexadecimal character */
char dtoh(int v)
{
    if(v < 10)
        return v+'0';
    return (v-10)+'a';
}

/* Trims the / and \ signs from end of string */
void trimenddir(char *str)
{
    int l;
    while((l=strlen(str)) > 1 && (str[l-1] == '/' || str[l-1] == '\\' ))
    {
        str[l-1] = '\0';
    }
}

/* Replace the \ to / and returns string without leading / */
char *unifypath(char *path)
{
    char *c = path;
    while(*c != '\0')
    {
        if(*c == '\\')
            *c = '/';
        ++c;
    }
    c = path;
    while(c[0] != '\0' && c[0] == '/')
        ++c;
    return c;
}

char *chop(char *str)
{
    char *c = str;
    c += strlen(str) - 1;
    while(c != str && *c == '\n')
    {
        *c = '\0';
        --c;
    }
    return str;
}

/* ************************************************************************************** */
struct PathMakerCacheItem* PathMaker::cache = NULL;

void PathMaker::clearCache(void)
{
    struct PathMakerCacheItem *old,*r=cache;
    while(r != NULL)
    {
        old = r;
        r = r->n;
        delete old;
    }
    cache = NULL;
}

bool PathMaker::findPath(char *path)
{
    struct PathMakerCacheItem *p=NULL,*r=cache;
    while(r != NULL)
    {
        if(!strcmp(r->path,path))
            return true;
        p = r;
        r = r->n;
    }
    r = new PathMakerCacheItem();
    strcpy(r->path,path);
    r->n = NULL;
    if(p==NULL)
        cache = r;
    else
        p->n = r;
    return false;
}

int PathMaker::mkpath(const char *path,bool contains_filename)
{
    int i;
    struct stat st;

    char *p = strdup(path);
    if(contains_filename)
    {
        i = strlen(p);
        if(contains_filename)
        while(i >= 0)
        {
            if(p[i] == '/' || p[i] == '\\')
                break;
            --i;
        }
        if(i>0)
            p[i] = '\0';
    }

    while((i = strlen(p)) > 0 && (p[i-1] == '/' || p[i-1] == '\\'))
        p[i-1] = '\0';

    if(findPath(p))
    {
        free(p);
        return 0;
    }

    char *cb,*c;
    cb = c = p;
    bool end = false;
    while(!end)
    {
        if(*c == '/' || *c == '\\' || *c == '\0')
        {
            if(*c == '\0')
                end = true;
            *c = '\0';

            if(cb != p || strlen(cb) != 2 || cb[1] != ':')
            {
                if(cb[0] != '\0')
                {
                    if (stat(cb, &st) != 0)
                    {
                        if(mymkdir(cb) != 0)
                        {
                            fprintf(stderr,"Error, cannot create directory: %s\n",cb);
                            free(p);
                            return 1;
                        }
                    }
                }
            }

            *c = '/';
        }
        ++c;
    }

    free(p);
    return 0;
}

/* ************************************************************************************** */
double FileCopier::ckbytes;

FileCopier::FileCopier(UniSyncConfig *ucp)
{
    uc = ucp;
    resetCounters();
}

int FileCopier::copy(const char *source,const char *dest)
{
#ifdef _WIN32
    if(uc->usestd)
        return copy_std(source,dest);
    return copy_spec(source,dest);
#else
    if(uc->usestd)
        return copy_std(source,dest);
    return copy_spec(source,dest);
#endif
}

int FileCopier::copy_std(const char *source,const char *dest)
{
    int n;
    FILE *src=NULL,*dst=NULL;
    unsigned char *buff;
    unsigned int copied=0;
    double size;

    if(uc->verbose > 1)
    {
        printf("Copy %s ...\n",source);
        if(uc->guicall)
            fflush(stdout);
    }

    if(PathMaker::mkpath(dest,true))
        return 1;

    if((src=fopen(source,"rb")) == NULL)
        return 1;

    if((dst=fopen(dest,"wb")) == NULL)
    {
        fclose(src);
        return 1;
    }

    buff = new unsigned char[8192];
    do
    {
        n = fread(buff,1,8192,src);
        if(n > 0)
            fwrite(buff,1,n,dst);
        copied += n;
    }
    while (n > 0);
    fclose(src);
    fclose(dst);

    struct stat s_st;
    struct utimbuf d_mt;
    if(!stat(source,&s_st))
    {
        d_mt.actime = s_st.st_atime;
        d_mt.modtime = s_st.st_mtime;
        if(utime(dest,&d_mt) != 0)
        {
            fprintf(stderr,"Error, Copy: cannot set times of target file: %s (%d)\n",dest,errno);
            if(uc->guicall)
                fflush(stderr);
            return 1;
        }
        if(chmod(dest,s_st.st_mode) != 0)
        {
            fprintf(stderr,"Error, Copy: cannot set mode of target file: %s (%d)\n",dest,errno);
            if(uc->guicall)
                fflush(stderr);
            return 1;
        }
    }
    else
    {
        fprintf(stderr,"Error, Copy: cannot get times of source file: %s (%d)\n",source,errno);
        if(uc->guicall)
            fflush(stderr);
        return 1;
    }

    size = ((double)copied) / 1024;
    ckbytes += size;
    delete[] buff;
    return 0;
}

#ifdef _WIN32
int FileCopier::copy_spec(const char *source,const char *dest)
{
    double size;

    if(uc->verbose > 1)
    {
        printf("Copy %s ..\n",source);
        if(uc->guicall)
            fflush(stdout);
    }

    if(PathMaker::mkpath(dest,true))
        return 1;
    BOOL pbCancel = false;
    if(CopyFileExA(source,dest,NULL,NULL,&pbCancel,0) == 0)
    {
        fprintf(stderr,"Error, Cannot copy the file: %s (%d)\n",source,(int)GetLastError());
        if(uc->guicall)
            fflush(stderr);
        return 1;
    }

    struct stat s_st;
    if(!stat(source,&s_st))
    {
        size = ((double)s_st.st_size) / 1024;
    }
    else
    {
        fprintf(stderr,"Error, Copy: cannot stat source file: %s (%d)\n",source,errno);
        if(uc->guicall)
            fflush(stderr);
        return 1;
    }

    ckbytes += size;
    return 0;
}
#else
int FileCopier::copy_spec(const char *source,const char *dest)
{
    int srcfd,dstfd;
    unsigned int copied=0;
    off_t offset = 0;
    double size;
    struct stat s_st;
    struct utimbuf d_mt;

    if(uc->verbose > 1)
    {
        printf("Copy %s ..\n",source);
        if(uc->guicall)
            fflush(stdout);
    }

    if(PathMaker::mkpath(dest,true))
        return 1;

    if(stat(source,&s_st))
    {
        fprintf(stderr,"Error, Copy: cannot get times of source file: %s (%d)\n",source,errno);
        if(uc->guicall)
            fflush(stderr);
        return 1;
    }

    if((srcfd=open(source,O_RDONLY)) == -1)
        return 1;

    if((dstfd=open(dest,O_WRONLY | O_CREAT,S_IRUSR | S_IWUSR)) == -1)
    {
        close(srcfd);
        return 1;
    }

    copied = sendfile(dstfd,srcfd,&offset,s_st.st_size);

    close(srcfd);
    close(dstfd);

    d_mt.actime = s_st.st_atime;
    d_mt.modtime = s_st.st_mtime;
    if(utime(dest,&d_mt) != 0)
    {
        fprintf(stderr,"Error, Copy: cannot set times of target file: %s (%d)\n",dest,errno);
        if(uc->guicall)
            fflush(stderr);
        return 1;
    }
    if(chmod(dest,s_st.st_mode) != 0)
    {
        fprintf(stderr,"Error, Copy: cannot set mode of target file: %s (%d)\n",dest,errno);
        if(uc->guicall)
            fflush(stderr);
        return 1;
    }

    size = ((double)copied) / 1024;
    ckbytes += size;
    return 0;
}
#endif

int FileCopier::fixtime(const char *source,const char *dest)
{
    struct stat s_st;
    struct utimbuf d_mt;
    if(uc->verbose > 1)
    {
        printf("Fixing times of %s ...\n",dest);
        if(uc->guicall)
            fflush(stdout);
    }
    if(!stat(source,&s_st))
    {
        d_mt.actime = s_st.st_atime;
        d_mt.modtime = s_st.st_mtime;
        if(utime(dest,&d_mt) != 0)
        {
            fprintf(stderr,"Error, Fixtime: cannot set times of target file: %s\n",dest);
            if(uc->guicall)
                fflush(stderr);
            return 1;
        }
        if(chmod(dest,s_st.st_mode) != 0)
        {
            fprintf(stderr,"Error, Fixtime: cannot set mode of target file: %s\n",dest);
            if(uc->guicall)
                fflush(stderr);
            return 1;
        }
    }
    else
    {
        fprintf(stderr,"Error, Fixtime: cannot get times of source file: %s\n",source);
        if(uc->guicall)
            fflush(stderr);
        return 1;
    }
    return 0;
}

void FileCopier::resetCounters(void)
{
    ckbytes = 0.0;
    ts = time(NULL);
    te = 0;
}

void FileCopier::printStatistics()
{
    if(uc->verbose > 0)
    {
        char buff[32];
        double speed;
        double sumtime;

        te = time(NULL);
        sumtime = difftime(te,ts);
        speed = 0;
        if(sumtime > 0)
            speed = (ckbytes/1024) / sumtime;
        my_dtoa(ckbytes,(char *)buff,32,0,2,1);
        if(speed > 0)
            printf("%s kbyte copied in %.2f sec (%.2f Mbyte/sec)\n",buff,sumtime,speed);
        else
            printf("%s kbyte copied in %.2f sec\n",buff,sumtime);
        if(uc->guicall)
            fflush(stdout);
    }
}

int FileCopier::deletefile(const char *path)
{
    if(uc->verbose > 1)
    {
        printf("Delete %s\n",path);
        if(uc->guicall)
            fflush(stdout);
    }
    return unlink(path);
}

int FileCopier::deletefolder(const char *path)
{
    if(uc->verbose > 1)
    {
        printf("Remove directory %s ...\n",path);
        if(uc->guicall)
            fflush(stdout);
    }
    return rmdir(path);
}

int gethash(const char *fullpath,char *hexhash,int hashmode,int needprefix)
{
    SHA256_CTX shactx;
    MD5_CTX md5ctx;
    FILE *f;

    if(hashmode == HASH_EMPTY)
    {
        hexhash[0] = '\0';
        return 0;
    }

    unsigned char hash[32];
    unsigned char buff[8192];

    f = fopen(fullpath,"rb");
    if(f == NULL)
        return 1;

    if(hashmode == HASH_SHA256)
        sha256_init(&shactx);
    if(hashmode == HASH_MD5)
        MD5_Init(&md5ctx);

    size_t n;
    do
    {
        n = fread(buff, 1, 8192, f);
        if(n > 0)
        {
            if(hashmode == HASH_SHA256)
                sha256_update(&shactx,buff,n);
            if(hashmode == HASH_MD5)
                MD5_Update(&md5ctx,buff,n);
        }
    }
    while (n > 0);

    if(hashmode == HASH_SHA256)
    {
        sha256_final(&shactx,hash);

        int idx=0;
        if(needprefix)
        {
            idx=5;
            memcpy(hexhash,"SHA2:",5);
        }
        for (int i=0; i < 32; i++)
        {
            hexhash[idx++] = dtoh((hash[i] & 240) >> 4);
            hexhash[idx++] = dtoh(hash[i] & 15);
        }
        hexhash[idx] = '\0';
    }
    if(hashmode == HASH_MD5)
    {
        MD5_Final(hash,&md5ctx);

        int idx=0;
        if(needprefix)
        {
            idx=4;
            memcpy(hexhash,"MD5:",5);
        }
        for (int i=0; i < 16; i++)
        {
            hexhash[idx++] = dtoh((hash[i] & 240) >> 4);
            hexhash[idx++] = dtoh(hash[i] & 15);
        }
        hexhash[idx] = '\0';
    }

    fclose(f);
    return 0;
}

#ifndef _WIN32
static struct termios oldt, newt;

/* Initialize new terminal i/o settings */
void initTermios(int echo)
{
  tcgetattr(0, &oldt); /* grab old terminal i/o settings */
  newt = oldt; /* make new settings same as old settings */
  newt.c_lflag &= ~ICANON; /* disable buffered i/o */
  newt.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
  tcsetattr(0, TCSANOW, &newt); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void)
{
  tcsetattr(0, TCSANOW, &oldt);
}

/* Read 1 character - echo defines echo mode */
char getch_(int echo)
{
  char ch;
  initTermios(echo);
  ch = getchar();
  resetTermios();
  return ch;
}
#endif

char read_and_echo_character(void)
{
#ifdef _WIN32
    return getche();
#else
    return getch_(0);
#endif
}

int my_dtoa(double v,char *buffer,int bufflen,int min,int max,int group)
{
    int digitnum;
    int i,forlength;
    int length=0; //the currnt filled length of the buffer

    char digit;
    char *str = buffer;

    unsigned long int i_ip,i_fp,idigit_value;
    double ip,fp;

    bufflen -= 2; //decrease bufflen value, to avoid decreasing in every if

    if(isnan(v))
    {
        if(bufflen < 4)
            return 1;
        strcpy(str,"NaN");
        return 0;
    }
    if(isinf(v))
    {
        if(bufflen < 4)
            return 1;
        strcpy(str,"Inf");
        return 0;
    }

    //split the number to integer and fractional part.
    fp = fabs(modf(v,&ip));
    ip = fabs(ip);
    if(fp != 0.0)
    {
        fp *= pow(10.0,max);
        fp = floor(fp + 0.5);
    }
    i_ip=ip;
    i_fp=fp;

    //If the original (rounded) number is negative put the sign to front
    v *= pow(10.0,max);
    v = floor(v + 0.5);
    if (v < 0)
    {
        *(str++) = '-';
        ++length;
        v = -v;
    }

    //Generate integer part (from i_ip)
    idigit_value = 1;
    digitnum = 1;
    while(idigit_value*10 <= i_ip)
    {
        idigit_value *= 10;
        ++digitnum;
    }
    forlength=0;
    while(idigit_value >= 1)
    {
        //put grouping space if set
        if(group && forlength != 0 && digitnum % 3 == 0)
        {
            *(str++) = ' ';
            ++length;
            if(length >= bufflen)
            {
                *(str) = '\0';
                return 1;
            }
        }

        digit = static_cast<char>((i_ip - i_ip%idigit_value) / idigit_value);
        i_ip = i_ip%idigit_value;

        *(str++) = '0' + digit%10;
        ++length;
        --digitnum;
        ++forlength;
        idigit_value /= 10;

        if(length >= bufflen)
        {
            *(str) = '\0';
            return 1;
        }
    }

    //Generate fractional part (from i_fp)
    digitnum=0;
    if( i_fp > 0 )
    {
        *(str++) = '.';
        ++length;

        idigit_value = 1;
        for(i=0;i<max-1;++i)
            idigit_value *= 10;

        while (idigit_value >= 1)
        {
            if(group && digitnum && digitnum%3 == 0)
            {
                *(str++) = ' ';
                ++length;
                if(length >= bufflen)
                {
                    *(str) = '\0';
                    return 1;
                }
            }

            digit = static_cast<char>((i_fp - i_fp%idigit_value) / idigit_value);
            i_fp = i_fp%idigit_value;

            *(str++) = '0' + digit%10;
            ++length;
            ++digitnum;
            idigit_value /= 10;

            if(length >= bufflen)
            {
                *(str) = '\0';
                return 1;
            }

            if(digitnum >= min && i_fp == 0)
                break;
        }
    }
    else
    {   //the original number was an integer, so we fill the minimal fractional part with zeros
        if(min > 0)
        {
            *(str++) = '.';
            ++length;
            for(digitnum=0;digitnum<min;)
            {
                if(group && digitnum && digitnum%3 == 0)
                {
                    *(str++) = ' ';
                    ++length;
                    if(length >= bufflen)
                    {
                        *(str) = '\0';
                        return 1;
                    }
                }
                *(str++) = '0';
                ++length;
                ++digitnum;
                if(length >= bufflen)
                {
                    *(str) = '\0';
                    return 1;
                }
            }
        }
    }
    *str = '\0';
    return 0;
}

/* end code */
