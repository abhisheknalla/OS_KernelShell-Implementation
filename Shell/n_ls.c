#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include<string.h>

int main(int argc,char *argv[])
{
    DIR* openDir;
    struct dirent *cont;
    struct stat statbuf;
    struct group *grp;
    struct passwd *usr;
    int cond = 0;
    if(argc == 1)
        openDir = opendir("./");
    else if(argc == 2)
    {
        if(strcmp(argv[1],"-l") == 0)
        {
            cond = 1;
            openDir = opendir("./");
        }
        else if(strcmp(argv[1],"-a") == 0)
        {
            cond = 2;
            openDir = opendir("./");
        }
        else if(strcmp(argv[1],"-la")==0 || strcmp(argv[1],"-al")==0)
        {
            cond = 3;
            openDir = opendir("./");
        }
        else
            openDir = opendir(argv[1]);
        if(openDir == NULL)
        {
            printf("Directory not found\n");
            return 1;
        }
    }
    else if(argc == 3)
    {
        if(strcmp(argv[1],"-l") == 0)
        {
            cond = 1;
            if(strcmp(argv[2],"-a") == 0)
            {
                cond = 3;
                openDir = opendir("./");
            }
            else
            {
                openDir = opendir(argv[2]);
            }
        }
        else if(strcmp(argv[1],"-a") == 0)
        {
            cond = 2;
            if(strcmp(argv[2],"-l") == 0)
            {
                cond = 3;
                openDir = opendir("./");
            }
            else
            {
                openDir = opendir(argv[2]);
            }
        }
        else if(strcmp(argv[1],"-la")==0 || strcmp(argv[1],"-al")==0)
        {
            cond = 3;
            openDir = opendir(argv[2]);
        }
        //openDir = opendir(argv[2]);
        if(openDir == NULL)
        {
            printf("Directory not found\n");
        }
    }
    else if(argc == 4)
    {
        openDir = opendir(argv[3]);
        cond = 3;
        if(openDir == NULL)
        {
            printf("Directory not found\n");
        }
    }
    while((cont = readdir(openDir)) != NULL)
    {
        //printf("%s\n",cont->d_name);
        if(cond == 3 || cond == 1)
        {
            if(cond == 1 && cont->d_name[0] == 46)
                ;
            else
            {
                stat(cont->d_name,&statbuf);
                if(((statbuf.st_mode) & S_IFMT) == S_IFREG)
                    printf("-");
                else if(((statbuf.st_mode) & S_IFMT) == S_IFDIR)
                    printf("d");
                if((statbuf.st_mode) & (S_IRUSR))
                    printf("r");
                else
                    printf("-");

                if((statbuf.st_mode) & (S_IWUSR))
                    printf("w");
                else
                    printf("-");
                if((statbuf.st_mode) & (S_IXUSR))
                    printf("x");
                else
                    printf("-");
                if((statbuf.st_mode) & (S_IRGRP))
                    printf("r");
                else
                    printf("-");
                if((statbuf.st_mode) & (S_IWGRP))
                    printf("w");
                else
                    printf("-");
                if((statbuf.st_mode) & (S_IXGRP))
                    printf("x");
                else
                    printf("-");
                if((statbuf.st_mode) & (S_IRGRP))
                    printf("r");
                else
                    printf("-");
                if((statbuf.st_mode) & (S_IWGRP))
                    printf("w");
                else
                    printf("-");
                if((statbuf.st_mode) & (S_IXGRP))
                    printf("x ");
                else
                    printf("- ");
                //printf("%d\n",statbuf.st_gid);
                grp = getgrgid(statbuf.st_gid);
                usr = getpwuid(statbuf.st_uid);
                printf("%ld %s %s %ld %s %s\n",statbuf.st_nlink,grp->gr_name,usr->pw_name,statbuf.st_size,ctime(&statbuf.st_mtime),cont->d_name);
            }
        }
        else if(cond == 2)
        {
            printf("%s\n",cont->d_name);
        }
        else
        {
            if(cont->d_name[0] == 46)
                ;
            else
                printf("%s\n",cont->d_name);
        }
        //printf("%s",cont->d_name);
    }
    return 0;
}
