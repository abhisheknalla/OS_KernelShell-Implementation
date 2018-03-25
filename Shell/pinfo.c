#include<stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include<string.h>

int main(int argc,char* argv[])
{
    FILE *fd;
    char buff[1000],str[250],pat[100],pa[100];
    char *is;
    int i,cnt;
    if(argc == 1)
    {
    //printf("%d\n",getpid()); 
    sprintf(str,"%d",getpid());
    strcpy(pat,"/proc/");
    strcat(pat,str);
    }
    else if (argc == 2)
    {
        strcpy(str,argv[1]);
        strcpy(pat,"/proc/");
        strcat(pat,str);
    }
    strcpy(pa,pat);
    strcat(pa,"/status");
    fd = fopen(pa,"r");
    fgets(str,250,fd);
    //is = strtok(str,"\n");
    //fgets(str,250,fd);
    //printf("%s\n",str);
    //printf("%s\n",is);
    i = 1;
    while(i < 17)
    {
        fgets(str,1000,fd);
        if(i == 1 || i == 4 || i == 16)
            printf("%s",str);
        i++;
    }
    strcat(pat,"/exe");
    cnt = readlink(pat,buff,100);
    buff[cnt] = '\0';
    printf("Exexutable Path: %s\n",buff);
    return 0;
}
