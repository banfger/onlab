#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

int prog(int iteration, const char ns[]);

int main(int agrc, const char* argv[])
{
    FILE * fp;
    
    fp = fopen("progresult.txt", "w");
    fclose(fp);
    
    fp = fopen("tdiffresult.txt", "w");
    fclose(fp);
    
    int n = atoi(argv[1]);
    char ns[] = {"cinuUmp"};
      
    prog(n, ns);

    system("awk -f tdiff.awk progresult.txt > tdiffresult.txt");
}
