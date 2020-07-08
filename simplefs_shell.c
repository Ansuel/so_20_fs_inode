#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "simplefs.h"

/* TODO
 * - cd cartella ( cd ..)
 * - mkdir nomecartella
 * - ls
 * - mount(init del fs)
 * - touch (creazione file)
 * - cp (copia di un file, nella directory corrente)
 * - cat (lettura di un file)
 * - write On file (scrittura di una stringa su un file)
 * - exit (uscire dalla shell)
 *
 */

// Global variable to store the disk
DirectoryHandle* currDir;
int mounted = 0;
DiskDriver disk;

SimpleFS fs;

char *built_in_str[] = {
    "mkDisk", "loadDisk", "formatDisk", "mountDisk",
    "help", "exit", "mkDir", "cd", "ls", "touch", "cp", "extractFile"};

int num_built_in = sizeof(built_in_str) / sizeof(char *);

// TODO unload disk and the simpleFS
int shellExit() { return 1; }

int help() {
  int i = 0;

  printf("\n");

  printf("Questi comandi sono usabili:");
  printf("\n");

  for (i = 0; i < num_built_in; i++) {
    printf("%s\n", built_in_str[i]);
  }

  printf("\n");

  return 0;
}

int check_built_in_command(char **args) {
  int i;

  if (args[0] == NULL) {
    return 0;
  }

  for (i = 0; i < num_built_in; i++) {
    if (strcmp(args[0], built_in_str[i]) == 0) {
      return 1;
    }
  }

  return 0;
}

int mkDisk(char **args) {
    if (args[1] == NULL) {
        return 0;
      }

    if (args[2] == NULL) {
        return 0;
    }

    // TODO: gestione errore
    DiskDriver_init(&disk, args[1], atoi(args[2]));
    fs.disk = &disk;

    printf("Disco creato con nome %s e grandezza %d Kbytes\n", args[1], (atoi(args[2]) * (BLOCK_SIZE / 1024) ));

    return 0;
}

int loadDisk(char **args) {
    if (args[1] == NULL) {
        return -1;
      }

    // TODO: gestione errore
    DiskDriver_init(&disk, args[1], 0);
    fs.disk = &disk;

    printf("Disco caricato con nome %s\n", args[1]);

    return 0;
}

int formatDisk() {
    SimpleFS_format(&fs);

    printf("Disco formattato, root creata\n");

    return 0;
}

int mountDisk() {
    currDir = SimpleFS_init(&fs, fs.disk);
    mounted = 1;

    printf("Disco montato\n");

    return 0;
}

int mkDir(char **args) {
    if(!args[1]){ printf("Inserisci il nome della nuova cartella\n");}
    SimpleFS_mkDir(currDir, args[1]);

    printf("Cartella creata con nome %s \n", args[1]);

    return 0;
}

int cd(char **args) {
    if(!args[1]){ printf("Inserisci il nome della cartella\n");}
    SimpleFS_changeDir(currDir, args[1]);
    return 0;
}

int ls() {
    int i;
    int numElem = currDir->fdb->num_entries;
    char** names = calloc(numElem,sizeof(char*));
    SimpleFS_readDir(names,currDir);
    for(i = 0; i < numElem; i++){
      printf("%s\t", names[i]);
    }
    printf("\n");
    return 0;
}

int touch(char** args) {
    FileHandle* ret;
    if(!args[1]) {printf("Inserisci il nome del file da creare\n");}
    ret = SimpleFS_createFile(currDir,args[1]);
    printf("Creato file con nome: %s \n", args[1]);
    free(ret);
    return 0;
}

int cp(char** args) {
    if(!args[1] || !args[2]){
      printf("Inserie il nome del file o il nuovo nome del file\n");
    }

    FileHandle* dst = SimpleFS_openFile(currDir,args[2]);
    if(!dst){ dst = SimpleFS_createFile(currDir,args[2]); }
    int src = open(args[1], O_RDONLY);
    char buf[1024];
    int bytes_read = 1;
    while(bytes_read != 0){
      bytes_read = read(src,buf,1024);
      if(bytes_read == -1){printf("Errore nella read"); return 0;}
      SimpleFS_write(dst,buf,bytes_read);
    }
    close(src);
    SimpleFS_close(dst);
    return 0;

}

int extractFile(char** args) {
    if(!args[1] || !args[2]){
      printf("Inserie il nome del file o il nuovo nome del file\n");
    }

    FileHandle* src = SimpleFS_openFile(currDir,args[1]);
    if(!src){ printf("File non esistente\n"); return 0; }
    int dst = open(args[2], O_CREAT | O_TRUNC | O_WRONLY, 0644);
    char buf[1024];
    int written_bytes = 0;
    int readed_bytes = 0;
    int fileLen = src->ffb->fcb.size_in_bytes;
    printf("filelen: %d\n", fileLen);
    while(written_bytes < fileLen){
      printf("Ci entro \n");
      int to_read = 1024;
      if(fileLen-written_bytes < to_read){ 
        to_read = fileLen-written_bytes;
        }
        printf("prima della read to_read: %d\n",to_read);
      readed_bytes = SimpleFS_read(src, buf, to_read);
      printf("dopo la read\n");
      written_bytes += write(dst, buf, readed_bytes);
    }
    close(dst);
    SimpleFS_close(src);
    return 0;

}

int execute_built_in_command(char **args) {
  int flag;

  if (strcmp(args[0], built_in_str[0]) == 0) {
    flag = mkDisk(args);
  }

    if (strcmp(args[0], built_in_str[1]) == 0) {
    flag = loadDisk(args);
  }

  if (strcmp(args[0], built_in_str[2]) == 0) {
    flag = formatDisk();
  }

  if (strcmp(args[0], built_in_str[3]) == 0) {
    flag = mountDisk();
  }

   if (strcmp(args[0], built_in_str[4]) == 0) {
    flag = help();
  }

   if (strcmp(args[0], built_in_str[5]) == 0) {
    flag = shellExit();
  }

   if (strcmp(args[0], built_in_str[6]) == 0) {
    flag = mkDir(args);
  }

   if (strcmp(args[0], built_in_str[7]) == 0) {
    flag = cd(args);
  }

   if (strcmp(args[0], built_in_str[8]) == 0) {
    flag = ls();
  }

   if (strcmp(args[0], built_in_str[9]) == 0) {
    flag = touch(args);
  }

   if (strcmp(args[0], built_in_str[10]) == 0) {
    flag = cp(args);
  }

  if (strcmp(args[0], built_in_str[11]) == 0) {
    flag = extractFile(args);
  }

  return flag;
}

char **parse_command(char *my_line) {
  int buffer_size = 64;
  int i = 0;
  char *arg;
  char **args = malloc(buffer_size * sizeof(char *));

  arg = strtok(my_line, " \t\r\n\a");
  while (arg != NULL) {
    args[i] = arg;
    i++;
    arg = strtok(NULL, " \t\r\n\a");
  }

  args[i] = NULL;

  return args;
}

char *read_command_line(void) {
  int bufsize = 1024;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;
  int i = 0;

  while (c != '\n') {
    c = getchar();
    buffer[i] = c;
    i++;
  }

  return buffer;
}

void print_prompt() { 

    if (mounted) {
        printf("%s ",currDir->fdb->fcb.name);
    }
    printf(">> "); 
}

int main(int argc, char **arg) {
  char *line;
  char **command;
  int flag = 0;

  printf("SimpleFS Test Shell ( Marangi 1816507 / Sudoso 1808356 )\n");

  while (!flag) {
    print_prompt();
    line = read_command_line();
    command = parse_command(line);

    if (command[0] != NULL) {
      if (check_built_in_command(command)) {
        flag = execute_built_in_command(command);
      } else {
        printf("INVALID COMMAND!\n");
      }
    }
  }

  return 0;
}
