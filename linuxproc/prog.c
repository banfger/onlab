#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>     
#include <limits.h>     //PATH_MAX
#include <fcntl.h>      //O_RDWR
#include <string.h>
#include <errno.h>

/*
      c - Cgroup      CLONE_NEWCGROUP   Cgroup root directory
      i - IPC         CLONE_NEWIPC      System V IPC, POSIX message queues
      n - Network     CLONE_NEWNET      Network devices, stacks, ports, etc.
      m - Mount       CLONE_NEWNS       Mount points
      p - PID         CLONE_NEWPID      Process IDs
      U - User        CLONE_NEWUSER     User and group IDs
      u - UTS         CLONE_NEWUTS      Hostname and NIS domain name
*/

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                               } while (0)

struct timespec start, stop;        /*Idõmérés kezdete és vége*/


#define STACK_SIZE (1024 * 1024)    /* Verem méret a child process-nek */
#define MAP_BUF_SIZE 100            //uid_map és gid_map átírásának hosszához

struct child_args {
  char * input;      /* Az input string átadásához */
  int pipe_fd[2];    /* Pipe a parent és child közti kommunikációhoz */
};

static int child_fn(void *arg){
    //Switch case szerkezet ahhoz, hogy a child process tudja, milyen új namespace-ekben hozták létre, és csak akkor állítson bármit is egy namespace-re vonatkozóan, ha az is ott szerepel a létrehozottak között.
    //A 'case' -ek után azért szerepel a ';' karakter, hogy ne kapjuk a ,,a label can only be part of a statement and a declaration is not a statement" hibát.
    
     struct child_args *args = (struct child_args *) arg;           /* Cast-olni kell */
     char ch;
     
     close(args->pipe_fd[1]);                                       /* Az írási végét bezárjuk a csatornának, hogy */
                                                                    /* lássuk az EOF -ot, amikor a parent végez */
                                                                    
     if (read(args->pipe_fd[0], &ch, 1) != 0) {                     /* Olvassuk az üzenetet */
         fprintf(stderr,
             "Failure in child: read from pipe returned != 0\n");
             exit(EXIT_FAILURE);
     }
     
     close(args->pipe_fd[0]);                                       /* Bezárjuk az olvasási véget is */
     
     //Child processhez létrehozott új namespace-ekben beállításokat végzünk.
     for(int i = 0; i < 7; i ++){
        switch(args->input[i]) {    
            case 'c' : ;              
                break;
                
            case 'i' : ;             
                break;
                
            case 'n' : ;/* Internetes beállítások a network namespace-en belül */
                        system("ip addr add 10.200.1.2/24 dev veth1");                  /* A parent processbõl létrehozott veth1 interfészt felkonfiguráljuk */
                        system("ip link set dev veth1 up");
                        system("ip link set dev lo up");
                        system("ip route add default via 10.200.1.1");                  /* Adunk egy alap útvonalat, amivel elérjük az õs */                                                                                             /* network namespace-beli virtuális interfészt */
                        //system("ifconfig");
                        //system("ping -c 4 8.8.8.8");
                break;
                
            case 'm' : ;                         
                        char path[PATH_MAX];
                        
                        if (getcwd(path, PATH_MAX) == NULL) {                           /* Aktuális útvonal lekérése */
                            fprintf(stderr, "ERROR: getcwd: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        }
                        strcat(path, "/rootfs");                                        /* rootfs hozzáadásával megkapjuk a rootfs könyvtár útvonalát */
                        //printf("%s\n", path);   
                        
                        char procPath[PATH_MAX];                                        /* proc könyvtár mount-olásához útvonal */
                        strcpy(procPath, path);
                        strcat(procPath, "/proc");    
                        
                        char oldPath[PATH_MAX];                                         /* A régi root könyvtár elrakásához útvonal */
                        strcpy(oldPath, path);
                        strcat(oldPath, "/.pivot_root");
                        //printf("%s\n", oldPath);                                                            
                        
                        if (mount("proc", procPath, "proc", 0, NULL) == -1) {           /* Mount-oljuk a proc könyvtárat */
                            fprintf(stderr, "ERROR: mount proc: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        }
                           
                        if (mount(path, path, "", MS_BIND | MS_REC, NULL) == -1) {      /* rootfs-t hozzá mount-oljuk önmagához, hogy a */ 
                            fprintf(stderr, "ERROR: root mount: %s\n",                  /* pivot_root egy követelménye teljesüljön */
                            strerror(errno));                                           /* „new_root and put_old must not be on the same */
                            exit(EXIT_FAILURE);                                         /* filesystem as the current root." */
                        }   
                        
                                
                        if (mkdir(oldPath, 0777) == -1) {                              /* Létrehozzuk a .pivot_root mappát */
                              fprintf(stderr, "ERROR: mkdir: %s\n", strerror(errno));  /* Ha már egyszer megcsinálta, itt mindig ERROR lesz*/
                              exit(EXIT_FAILURE);                                                                 
                        }   
                        
                        //system("ls -la rootfs");
                        if (syscall(SYS_pivot_root, path, oldPath) == -1) {            /* Megváltoztatjuk a root könyvtárat a rootfs-re, a régi pedig átkerül a .pivot_root-ba */
                            fprintf(stderr, "ERROR: pivot_root: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        } 
                        
                        if (chdir("/") == -1) {                                        /* Megváltoztatjuk az aktuális könyvtárat a root könyvátrra */
                            fprintf(stderr, "ERROR: chdir: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        } 
                        
                        strcpy(oldPath, "/.pivot_root");
                        if (umount2(oldPath, MNT_DETACH) == -1) {                      /* Leválasztjuk a .pivot_root mount-ot az összes alatta lévõ mount-tal együtt */
                            fprintf(stderr, "ERROR: umount: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        } 
                        
                        if (remove(oldPath) == -1) {                                   /* Töröljük a .pivot_root mappát */
                            fprintf(stderr, "ERROR: remove: %s\n",  
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        } 
                        
                        /*
                        if(strchr(args->input, 'p')){
                          system("ps");
                        } 
                        system("ls -a");
                        system("less /proc/mounts");
                        */
                                            
                break;
                
            case 'p' : ;
                        /* Child process milyen pID-vel látja magát */
                        //fprintf(stderr, "Child process pID (child)  : %ld\n", (long)getpid());  
                        
                break;
                
            case 'U' : ;    
                        //system("id");                        /* Ellenõrizzük, hogy miként lát minket */                      
                break;
                
            case 'u' : ;   
                        /*             
                        char name[100];
                        gethostname(name, 100);                //Hostname lekérése 
                        printf("%s\n", name);
                        getdomainname(name, 100);              //Domainname lekérése
                        printf("%s\n", name);
                        */
                        
                        sethostname("new-hostname", 12);       /* Átállítjuk a hostname-t */
                        setdomainname("new-domainname", 14);   /* Átállítjuk a domainname-t */ 
                        
                        /*
                        gethostname(name, 100);
                        printf("%s\n", name);
                        getdomainname(name, 100);
                        printf("%s\n", name);    
                        */                
                break;
                
            default : ;
                break;
        }
    }
    
    //sleep(100)                  /* Idõ arra, hogy megnézhessük a létrejött namespace-eket lsns segítségével. */
}

//*_map fájlok felkonfigurálása a User namespace-hez
static void set_user_maps(pid_t pid){
  char path[PATH_MAX];
  char map_buf[MAP_BUF_SIZE];
  int fd;
  
  /* uid_map fájlba írjük a '0 [pID] 1' sort */                                    
  sprintf(path,"/proc/%d/uid_map", pid);
  snprintf(map_buf, MAP_BUF_SIZE, "0 %ld 1", (long) getuid());
  fd = open(path, O_RDWR);
                        
  if (fd == -1) {
    fprintf(stderr, "ERROR: open %s: %s\n", path,
      strerror(errno));
      exit(EXIT_FAILURE);
  }
                        
  if (write(fd, map_buf, strlen(map_buf)) != strlen(map_buf)) {
    fprintf(stderr, "ERROR: write %s: %s\n", path,
      strerror(errno));
      exit(EXIT_FAILURE);
  }                       
  close(fd);   
                                      
                                      
  /* Elõször a setgroups fáflt át kell írni 'deny'-ra, hogy lehessen a gid_mapot szerkeszteni */
  sprintf(path,"/proc/%d/setgroups", pid);
  sprintf(map_buf, "deny");
  fd = open(path, O_RDWR);
                        
  if (fd == -1) {
    fprintf(stderr, "ERROR: open %s: %s\n", path,
      strerror(errno));
      exit(EXIT_FAILURE);
  }
                        
  if (write(fd, map_buf, strlen(map_buf)) != strlen(map_buf)) {
    fprintf(stderr, "ERROR: write %s: %s\n", path,
      strerror(errno));
      exit(EXIT_FAILURE);
  }                        
  close(fd);   
      
  
  /* gid_map fájlba írjük a '0 [pID] 1' sort */                                        
  sprintf(path,"/proc/%d/gid_map", pid);      
  snprintf(map_buf, MAP_BUF_SIZE, "0 %ld 1", (long) getgid());
                          
  fd = open(path, O_RDWR); 
                        
  if (fd == -1) {
    fprintf(stderr, "ERROR: open %s: %s\n", path,
     strerror(errno));
     exit(EXIT_FAILURE);
  }
                                        
  if (write(fd, map_buf, strlen(map_buf)) != strlen(map_buf)) {
    fprintf(stderr, "ERROR: write %s: %s\n", path,
     strerror(errno));
     exit(EXIT_FAILURE);
  } 
  close(fd);
  
}

//Hálózati beállítások a child process-en belül.
static void set_netns(pid_t pid){
  char str[100] = ("");
  sprintf(str, "ip link add name veth0 type veth peer name veth1 netns %d\n", pid);    /* Virtuális interfész párt hozunk létre, amibõl az egyiket a child network namespace-be rakjuk */
  system(str);
  
  /* Elõször lekérjük az adott gép azon interfészének nevét, amin keresztül eléri az internetet */
  FILE *eth = popen("ip route get 8.8.8.8 | awk '{for(i=1;i<=NF;i++) if ($i==\"dev\") print $(i+1)}'", "r");
  
  char buf[256];
  char interfaceName[20];
  
  while (fgets(buf, sizeof(buf), eth) != 0) {
    strcpy(interfaceName, buf);
  }

  pclose(eth);
  
  char* newline;
  if ((newline = strchr(interfaceName, '\n')) != NULL){
    *newline = '\0';
  }
  
  
  system("ip addr add 10.200.1.1/24 dev veth0");                                       /* Felkonfiguráljuk a veth0 interfészt */
  system("ip link set veth0 up");
  system("echo 1 > /proc/sys/net/ipv4/ip_forward");                                    /* Engedélyezzük az IP forwarding-ot */
  system("iptables -P FORWARD DROP");                                                  /* Töröljük az eddigi szabályokat */
  system("iptables -F FORWARD");
  system("iptables -t nat -F");
  
  /* A kapott interfész névvel kiegészítve beillesztjük a következõ beállításokba, */
  /* amelyekben elõször a 10.200.1.2/24-es címrõl jövõ forgalmat "elrejtjük" a rendes IP cím alá, */
  /* majd engedélyezzük a virtuális interfészhez a forward-olást.*/
  sprintf(str, "iptables -t nat -A POSTROUTING -s 10.200.1.2/24 -o %s -j MASQUERADE", interfaceName);
  system(str);
  sprintf(str, "iptables -A FORWARD -i %s -o veth0 -j ACCEPT", interfaceName);
  system(str);
  sprintf(str, "iptables -A FORWARD -o %s -i veth0 -j ACCEPT", interfaceName);
  system(str);  
}

//A parent process-ben kitöröljük azokat a hálózati beállításokat, 
//amelyeket létrehoztunk azért, hogy elérjüka  child process-.t
static void del_netns(){
  system("echo 0 > /proc/sys/net/ipv4/ip_forward");
  system("iptables -P FORWARD DROP");
  system("iptables -F FORWARD");
  system("iptables -t nat -F");
}


int prog(int iteration, const char ns[])
{
    //Input megadása, először azt kell megadni, hogy hányszor fusson le a folyamat létrehozás/namespace beállítás, 
    //utána pedig azt, hogy mely namespace-eket szeretnénk létrehozi (fentebb szereplnek a betűjelek). Példa: pnuU
    //Hibás input nincs levédve.
    int n;
    char input[7] = {""};
    n = iteration;
    strcpy(input, ns);
    
    printf("Number of iteration: %d\n", n);
    printf("Types of namespaces: %s\n", input);
    
    //Flag-eket tartalmazó tömb létrehozása, illetve egy integer amiben ezeket összesítjük.
    int x[7] = {CLONE_NEWCGROUP, CLONE_NEWIPC, CLONE_NEWNET, CLONE_NEWNS, CLONE_NEWPID, CLONE_NEWUSER, CLONE_NEWUTS};
    int flags = 0;

    //Switch case szerkezet az input szerinti megfelelõ flag létrehozásához. A '|' operátor bitenkénti vagy-olás.
     for(int i = 0; i < 7; i ++){
        switch(input[i]) {
            case 'c' :
                flags = flags | x[0];
                break;
            case 'i' :
                flags = flags | x[1];
                break;
            case 'n' :
                flags = flags | x[2];
                break;
            case 'm' :
                flags = flags | x[3];
                break;
            case 'p' :
                flags = flags | x[4];
                break;
            case 'U' :
                flags = flags | x[5];
                break;
            case 'u' :  
                flags = flags | x[6];
                break;
            default :
                break;
        }
    }
            
    //Child process vermének inicializálása.
    char *stack;                    /* Verem buffer kezdete */
    char *stackTop;                 /* Verem buffer vége */
    stack = malloc(STACK_SIZE);     /* Verem memória foglalása */
    
    if (stack == NULL)              /* Hibaellenõrzés */
      errExit("malloc");
      
    stackTop = stack + STACK_SIZE;  /* Feltéve, hogy a verem lefelé terjed */
    pid_t pid;                      /* A clone() függvény visszatérési értéke */
    
    struct child_args args;         /* Létrehozzuk a child process-nek átadandó argumentumok struktúráját */
    args.input = input;             /* Átadjuk majd az inputot a child process switch case-éhez */
    
    FILE * fp;                      /* Idõeredmény fájlba írásához. */
     
    //For ciklus a mérés ismétléséhez.
    for(int j = 0; j < n; j++){
      printf("%d\n", j);
    
      if (pipe(args.pipe_fd) == -1)        /* Létrehozzuk a csatornát, amin keresztül üzen a parent a child-nak.*/
        errExit("pipe");  
         
      if ((fp = fopen("progresult.txt", "a")) == NULL) /* Megnyitjuk a fájlt hozzáadásra */
      {
        printf("Error opening file!\n");
        exit(1);
      }
      
      clock_gettime(CLOCK_REALTIME, &start); /* Kezdeti idõpont */
    
      fprintf(fp, "start: %ld.%09ld\n", start.tv_sec, start.tv_nsec);  /* start idõ hozzáadása */
      
      //Child process létrehozása.
      pid = clone(child_fn, stackTop, flags | SIGCHLD, &args);
      
      if (pid == -1)                      /* Megnézzük, hogy létrejött-e a child process. */
        errExit("clone");
      
      if(strchr(input, 'U') != NULL){     /* Ha hoztunk létre User namespace-t, akkor beállítjuk a *_map-okat */
        set_user_maps(pid);               /* Átadjuk a child process azonosítóját is a path-hoz */    
      }
      
      if(strchr(input, 'n') != NULL){     /* Ha hoztunk létre Network namespace-t, akkor konfiguráljuk fel */
        set_netns(pid);
      }
      
      close(args.pipe_fd[1]);             /* Bezárjuk a csatornát, hogy jelezzünk a child process-nek */
              
      if (waitpid(pid, NULL, 0) == -1)    /* Megvárjuk, amíg a child process visszatér */
               errExit("waitpid");
                    
      clock_gettime(CLOCK_REALTIME, &stop);  /* Child process "halála" utáni idõpont */
      
      fprintf(fp, "ready: %ld.%09ld\n", stop.tv_sec, stop.tv_nsec); /* ready idõ hozzáadása */
      fclose(fp);
      
      if(strchr(input, 'n') != NULL){
          del_netns();
          if(strchr(input, 'i') == NULL){      /* Ha nem várunk az iterációk között, akkor */  
            sleep(1);                          /*  RTNETLINK answers: File exists hiba lehet */
          }                                    /* (IPC namespace-szel nem kell) */                            
      }                                                                                    
    }
}
