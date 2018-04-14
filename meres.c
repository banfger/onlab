#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

int main(int agrc, const char* argv[])
{
    FILE * fp;
    
    fp = fopen("progresult.txt", "w");
    fclose(fp);
    
    fp = fopen("tdiffresult.txt", "w");
    fclose(fp);
    
    int n = atoi(argv[1]);
    char ns[] = {"cinuUmp"};
    
    char str[120] = "";
    sprintf(str, "./prog %d %s", n, ns);
    system(str);

    system("awk -f tdiff.awk progresult.txt > tdiffresult.txt");
}