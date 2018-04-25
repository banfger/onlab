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

struct timespec start, stop;        /*Id�m�r�s kezdete �s v�ge*/


#define STACK_SIZE (1024 * 1024)    /* Verem m�ret a child process-nek */
#define MAP_BUF_SIZE 100            //uid_map �s gid_map �t�r�s�nak hossz�hoz

struct child_args {
  char * input;      /* Az input string �tad�s�hoz */
  int pipe_fd[2];    /* Pipe a parent �s child k�zti kommunik�ci�hoz */
};

static int child_fn(void *arg){
    //Switch case szerkezet ahhoz, hogy a child process tudja, milyen �j namespace-ekben hozt�k l�tre, �s csak akkor �ll�tson b�rmit is egy namespace-re vonatkoz�an, ha az is ott szerepel a l�trehozottak k�z�tt.
    //A 'case' -ek ut�n az�rt szerepel a ';' karakter, hogy ne kapjuk a ,,a label can only be part of a statement and a declaration is not a statement" hib�t.
    
     struct child_args *args = (struct child_args *) arg;           /* Cast-olni kell */
     char ch;
     
     close(args->pipe_fd[1]);                                       /* Az �r�si v�g�t bez�rjuk a csatorn�nak, hogy */
                                                                    /* l�ssuk az EOF -ot, amikor a parent v�gez */
                                                                    
     if (read(args->pipe_fd[0], &ch, 1) != 0) {                     /* Olvassuk az �zenetet */
         fprintf(stderr,
             "Failure in child: read from pipe returned != 0\n");
             exit(EXIT_FAILURE);
     }
     
     close(args->pipe_fd[0]);                                       /* Bez�rjuk az olvas�si v�get is */
     
     //Child processhez l�trehozott �j namespace-ekben be�ll�t�sokat v�gz�nk.
     for(int i = 0; i < 7; i ++){
        switch(args->input[i]) {    
            case 'c' : ;              
                break;
                
            case 'i' : ;             
                break;
                
            case 'n' : ;/* Internetes be�ll�t�sok a network namespace-en bel�l */
                        system("ip addr add 10.200.1.2/24 dev veth1");                  /* A parent processb�l l�trehozott veth1 interf�szt felkonfigur�ljuk */
                        system("ip link set dev veth1 up");
                        system("ip link set dev lo up");
                        system("ip route add default via 10.200.1.1");                  /* Adunk egy alap �tvonalat, amivel el�rj�k az �s */                                                                                             /* network namespace-beli virtu�lis interf�szt */
                        //system("ifconfig");
                        //system("ping -c 4 8.8.8.8");
                break;
                
            case 'm' : ;                         
                        char path[PATH_MAX];
                        
                        if (getcwd(path, PATH_MAX) == NULL) {                           /* Aktu�lis �tvonal lek�r�se */
                            fprintf(stderr, "ERROR: getcwd: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        }
                        strcat(path, "/rootfs");                                        /* rootfs hozz�ad�s�val megkapjuk a rootfs k�nyvt�r �tvonal�t */
                        //printf("%s\n", path);   
                        
                        char procPath[PATH_MAX];                                        /* proc k�nyvt�r mount-ol�s�hoz �tvonal */
                        strcpy(procPath, path);
                        strcat(procPath, "/proc");    
                        
                        char oldPath[PATH_MAX];                                         /* A r�gi root k�nyvt�r elrak�s�hoz �tvonal */
                        strcpy(oldPath, path);
                        strcat(oldPath, "/.pivot_root");
                        //printf("%s\n", oldPath);                                                            
                        
                        if(strchr(args->input, 'p') != NULL){   
                          if (mount("proc", procPath, "proc", 0, NULL) == -1) {           /* Mount-oljuk a proc k�nyvt�rat */
                            fprintf(stderr, "ERROR: mount proc: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                          }  
                        }   
                        
                         
                        if (mount(path, path, "", MS_BIND | MS_REC, NULL) == -1) {      /* rootfs-t hozz� mount-oljuk �nmag�hoz, hogy a */                                                                                               /* pivot_root egy k�vetelm�nye teljes�lj�n */
                            fprintf(stderr, "ERROR: root mount: %s\n",                  /* �new_root and put_old must not be on the same */                                                                                              /* filesystem as the current root." */
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        }   
                        
                                
                        if (mkdir(oldPath, 0777) == -1) {                              /* L�trehozzuk a .pivot_root mapp�t */
                              if(!(strcmp(strerror(errno), "ERROR: mkdir: File exists"))){
                                fprintf(stderr, "ERROR: mkdir: %s\n", strerror(errno));  /* Ha m�r egyszer megcsin�lta, itt mindig ERROR lesz*/
                                exit(EXIT_FAILURE);            
                              }                                                             
                        }   
                        
                        //system("ls -la rootfs");
                        printf("%s\n", oldPath);
                        printf("%s\n", path);
                        if (syscall(SYS_pivot_root, path, oldPath) == -1) {            /* Megv�ltoztatjuk a root k�nyvt�rat a rootfs-re, a r�gi pedig �tker�l a .pivot_root-ba */
                            fprintf(stderr, "ERROR: pivot_root: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        } 
                        
                        if (chdir("/") == -1) {                                        /* Megv�ltoztatjuk az aktu�lis k�nyvt�rat a root k�nyv�trra */
                            fprintf(stderr, "ERROR: chdir: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        } 
                        
                        strcpy(oldPath, "/.pivot_root");
                        if (umount2(oldPath, MNT_DETACH) == -1) {                      /* Lev�lasztjuk a .pivot_root mount-ot az �sszes alatta l�v� mount-tal egy�tt */
                            fprintf(stderr, "ERROR: umount: %s\n",
                            strerror(errno));
                            exit(EXIT_FAILURE);                  
                        } 
                        
                        if (remove(oldPath) == -1) {                                   /* T�r�lj�k a .pivot_root mapp�t */
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
                        /* Child process milyen pID-vel l�tja mag�t */
                        //fprintf(stderr, "Child process pID (child)  : %ld\n", (long)getpid());  
                        
                break;
                
            case 'U' : ;    
                        //system("id");                        /* Ellen�rizz�k, hogy mik�nt l�t minket */                      
                break;
                
            case 'u' : ;   
                        /*             
                        char name[100];
                        gethostname(name, 100);                //Hostname lek�r�se 
                        printf("%s\n", name);
                        getdomainname(name, 100);              //Domainname lek�r�se
                        printf("%s\n", name);
                        */
                        
                        sethostname("new-hostname", 12);       /* �t�ll�tjuk a hostname-t */
                        setdomainname("new-domainname", 14);   /* �t�ll�tjuk a domainname-t */ 
                        
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
    
    //sleep(100)                  /* Id� arra, hogy megn�zhess�k a l�trej�tt namespace-eket lsns seg�ts�g�vel. */
}

//*_map f�jlok felkonfigur�l�sa a User namespace-hez
static void set_user_maps(pid_t pid){
  char path[PATH_MAX];
  char map_buf[MAP_BUF_SIZE];
  int fd;
  
  /* uid_map f�jlba �rj�k a '0 [pID] 1' sort */                                    
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
                                      
                                      
  /* El�sz�r a setgroups f�flt �t kell �rni 'deny'-ra, hogy lehessen a gid_mapot szerkeszteni */
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
      
  
  /* gid_map f�jlba �rj�k a '0 [pID] 1' sort */                                        
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

//H�l�zati be�ll�t�sok a child process-en bel�l.
static void set_netns(pid_t pid){
  char str[100] = ("");
  sprintf(str, "ip link add name veth0 type veth peer name veth1 netns %d\n", pid);    /* Virtu�lis interf�sz p�rt hozunk l�tre, amib�l az egyiket a child network namespace-be rakjuk */
  system(str);
  
  /* El�sz�r lek�rj�k az adott g�p azon interf�sz�nek nev�t, amin kereszt�l el�ri az internetet */
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
  
  system("ip addr add 10.200.1.1/24 dev veth0");                                       /* Felkonfigur�ljuk a veth0 interf�szt */
  system("ip link set veth0 up");
  system("echo 1 > /proc/sys/net/ipv4/ip_forward");                                    /* Enged�lyezz�k az IP forwarding-ot */
  system("iptables -P FORWARD DROP");                                                  /* T�r�lj�k az eddigi szab�lyokat */
  system("iptables -F FORWARD");
  system("iptables -t nat -F");
  
  /* A kapott interf�sz n�vvel kieg�sz�tve beillesztj�k a k�vetkez� be�ll�t�sokba, */
  /* amelyekben el�sz�r a 10.200.1.2/24-es c�mr�l j�v� forgalmat "elrejtj�k" a rendes IP c�m al�, */
  /* majd enged�lyezz�k a virtu�lis interf�szhez a forward-ol�st.*/
  sprintf(str, "iptables -t nat -A POSTROUTING -s 10.200.1.2/24 -o %s -j MASQUERADE", interfaceName);
  system(str);
  sprintf(str, "iptables -A FORWARD -i %s -o veth0 -j ACCEPT", interfaceName);
  system(str);
  sprintf(str, "iptables -A FORWARD -o %s -i veth0 -j ACCEPT", interfaceName);
  system(str);  
}

//A parent process-ben kit�r�lj�k azokat a h�l�zati be�ll�t�sokat, 
//amelyeket l�trehoztunk az�rt, hogy el�rj�ka  child process-.t
static void del_netns(){
  system("echo 0 > /proc/sys/net/ipv4/ip_forward");
  system("iptables -P FORWARD DROP");
  system("iptables -F FORWARD");
  system("iptables -t nat -F");
}


int prog(int iteration, const char ns[])
{
    //Input megad�sa, el�sz�r azt kell megadni, hogy h�nyszor fusson le a folyamat l�trehoz�s/namespace be�ll�t�s, ut�na pedig azt, hogy mely namespace-eket szeretn�nk l�trehozi (fentebb szereplnek a bet�jelek). P�lda: pnuU
    //Hib�s input nincs lev�dve.
    int n;
    char input[7] = {""};
    n = iteration;
    strcpy(input, ns);
    
    printf("Number of iteration: %d\n", n);
    printf("Types of namespaces: %s\n", input);
    
    //Flag-eket tartalmaz� t�mb l�trehoz�sa, illetve egy integer amiben ezeket �sszes�tj�k.
    int x[7] = {CLONE_NEWCGROUP, CLONE_NEWIPC, CLONE_NEWNET, CLONE_NEWNS, CLONE_NEWPID, CLONE_NEWUSER, CLONE_NEWUTS};
    int flags = 0;

    //Switch case szerkezet az input szerinti megfelel� flag l�trehoz�s�hoz. A '|' oper�tor bitenk�nti vagy-ol�s.
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
            
    //Child process verm�nek inicializ�l�sa.
    char *stack;                    /* Verem buffer kezdete */
    char *stackTop;                 /* Verem buffer v�ge */
    stack = malloc(STACK_SIZE);     /* Verem mem�ria foglal�sa */
    
    if (stack == NULL)              /* Hibaellen�rz�s */
      errExit("malloc");
      
    stackTop = stack + STACK_SIZE;  /* Felt�ve, hogy a verem lefel� terjed */
    pid_t pid;                      /* A clone() f�ggv�ny visszat�r�si �rt�ke */
    
    struct child_args args;         /* L�trehozzuk a child process-nek �tadand� argumentumok strukt�r�j�t */
    args.input = input;             /* �tadjuk majd az inputot a child process switch case-�hez */
    
    FILE * fp;                      /* Id�eredm�ny f�jlba �r�s�hoz. */
     
    //For ciklus a m�r�s ism�tl�s�hez.
    for(int j = 1; j < n+1; j++){
      printf("%d\n", j);
    
      if (pipe(args.pipe_fd) == -1)        /* L�trehozzuk a csatorn�t, amin kereszt�l �zen a parent a child-nak.*/
        errExit("pipe");  
         
      if ((fp = fopen("progresult.txt", "a")) == NULL) /* Megnyitjuk a f�jlt hozz�ad�sra */
      {
        printf("Error opening file!\n");
        exit(1);
      }
      
      clock_gettime(CLOCK_REALTIME, &start); /* Kezdeti id�pont */
    
      fprintf(fp, "start: %ld.%09ld\n", start.tv_sec, start.tv_nsec);  /* start id� hozz�ad�sa */
      
      //Child process l�trehoz�sa.
      pid = clone(child_fn, stackTop, flags | SIGCHLD, &args);
      
      if (pid == -1)                      /* Megn�zz�k, hogy l�trej�tt-e a child process. */
        errExit("clone");
      
      if(strchr(input, 'U') != NULL){     /* Ha hoztunk l�tre User namespace-t, akkor be�ll�tjuk a *_map-okat */
        set_user_maps(pid);               /* �tadjuk a child process azonos�t�j�t is a path-hoz */    
      }
      
      if(strchr(input, 'n') != NULL){     /* Ha hoztunk l�tre Network namespace-t, akkor konfigur�ljuk fel */
        set_netns(pid);
      }
      
      close(args.pipe_fd[1]);             /* Bez�rjuk a csatorn�t, hogy jelezz�nk a child process-nek */
              
      if (waitpid(pid, NULL, 0) == -1)    /* Megv�rjuk, am�g a child process visszat�r */
               errExit("waitpid");
                    
      clock_gettime(CLOCK_REALTIME, &stop);  /* Child process "hal�la" ut�ni id�pont */
      
      fprintf(fp, "ready: %ld.%09ld\n", stop.tv_sec, stop.tv_nsec); /* ready id� hozz�ad�sa */
      fclose(fp);
      
      if(strchr(input, 'n') != NULL){
          del_netns();
          //if(strchr(input, 'i') == NULL){      /* Ha nem v�runk az iter�ci�k k�z�tt, akkor */  
            sleep(1);                          /*  RTNETLINK answers: File exists hiba lehet */
          //}                                    /* (IPC namespace-szel nem kell n�hol, de van ahol igen) */                            
      }                                                                                    
    }
}