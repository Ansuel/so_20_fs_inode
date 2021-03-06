#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "simplefs.h"
#define TRANSFER_SIZE 1024
// Global variable to store the disk
DirectoryHandle *currDir;
int mounted = 0;
int loaded = 0;
int ret;
DiskDriver disk;

SimpleFS fs;

char *built_in_str[] = {"mkDisk", "loadDisk",  "formatDisk", "mountDisk",
                        "help",   "exit",      "mkDir",      "cd",
                        "ls",     "touch",     "cp",         "extractFile",
                        "cat",    "writeFile", "appendFile", "rm"};

char *build_in_str_descr[] = {
    "Crea il disco *** Parametri: Nome - Dimensione",
    "Carica il disco nella shell *** Parametri: Nome",
    "Crea la root nel disco caricato *** Parametri: //",
    "Monta il disco nella shell *** Parametri: //",
    "Lista del comandi *** Parametri: //",
    "Termina la shell e libera la memoria occupata *** Parametri: //",
    "Crea una cartella *** Parametri: Nome",
    "Cambia la directory corrente *** Parametri: Nome",
    "Visualizza i file e le directory nella cartella corrente *** Parametri: "
    "//",
    "Crea un file vuoto *** Parametri: Nome",
    "Copia un file ESTERNO al disco NEL disco *** Parametri: NomeFileEsterno "
    "NomeFileInterno",
    "Copia un file INTERNO al disco e ne crea uno ESTERNO *** Parametri: "
    "NomeFileInterno NomeFileEsterno",
    "Leggi un file *** Parametri: Nome",
    "Scrivi una stringa su un file *** Parametri: Nome Testo",
    "Appendi una stringa alla fine di un file *** Parametri: Nome Testo",
    "Rimuove un file o una directory(ricorsivamente) *** Parametri: Nome"};

int num_built_in = sizeof(built_in_str) / sizeof(char *);

int shellExit(void) {

  if (mounted) {
    DiskDriver_flush(&disk);
    SimpleFS_unload(&fs, currDir);
  }

  return 1;
}

int help(void) {
  int i = 0;

  printf("\n");

  printf("Questi comandi sono usabili:");
  printf("\n");

  for (i = 0; i < num_built_in; i++) {
    if (i == 1 || i == 2 || i == 3 || i == 11 || i == 13 || i == 14) {
      printf("\033[0;32m%s \033[0m\t ", built_in_str[i]);
      printf("%s\n", build_in_str_descr[i]);
    } else {
      printf("\033[0;32m%s \033[0m\t\t %s\n", built_in_str[i],
             build_in_str_descr[i]);
    }
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
    printf("Parametri mancanti\n");
    return 0;
  }

  if (args[2] == NULL) {
    printf("Parametri mancanti\n");
    return 0;
  }

  // TODO: gestione errore
  ret = DiskDriver_init(&disk, args[1], atoi(args[2]));
  if (ret < 0) {
    printf("Errore nella DiskDriver_init\n");
    return -1;
  }
  fs.disk = &disk;

  printf("Disco creato con nome %s e grandezza %d Kbytes\n", args[1],
         (atoi(args[2]) * (BLOCK_SIZE / 1024)));
  printf("Bitmap Blocks: %d\n", disk.header->bitmap_blocks);
  printf("Bitmap Entries: %d\n", disk.header->bitmap_entries);
  printf("Reserved Blocks: %d\n", disk.reserved_blocks);

  loaded = 1;

  return 0;
}

int loadDisk(char **args) {
  if (args[1] == NULL) {
    printf("Parametri mancanti\n");
    return 0;
  }

  ret = DiskDriver_init(&disk, args[1], 0);
  if (ret < 0) {
    printf("Errore nella DiskDriver_init\n");
    return -1;
  }
  fs.disk = &disk;

  printf("Disco caricato con nome %s\n", args[1]);
  printf("Bitmap Blocks: %d\n", disk.header->bitmap_blocks);
  printf("Bitmap Entires: %d\n", disk.header->bitmap_entries);
  printf("Reserved blocks: %d\n", disk.reserved_blocks);

  loaded = 1;

  return 0;
}

int formatDisk(void) {
  ret = SimpleFS_format(&fs);
  if (ret < 0) {
    printf("Errore nella format\n");
    return 0;
  }

  printf("Disco formattato, root creata\n");

  return 0;
}

int mountDisk(void) {
  currDir = SimpleFS_init(&fs, fs.disk);

  if (!currDir) {
    printf("Disco non montato correttamente. E' stato formattato?\n");
    return 0;
  }

  mounted = 1;
  printf("Disco montato\n");

  return 0;
}

int mkDir(char **args) {
  if (!args[1]) {
    printf("Inserisci il nome della nuova cartella\n");
    return 0;
  }
  ret = SimpleFS_mkDir(currDir, args[1]);
  if (ret < 0) {
    printf("Errore nella mkDir. La direcorty già esiste?\n");
    return 0;
  }
  printf("Cartella creata con nome %s nella cartella %s\n", args[1],
         currDir->fdb->fcb.name);

  return 0;
}

int cd(char **args) {
  if (!args[1]) {
    printf("Inserisci il nome della cartella\n");
  }
  ret = SimpleFS_changeDir(currDir, args[1]);
  if (ret < 0) {
    printf("Errore nella changeDir\n");
    return 0;
  }
  return 0;
}

int ls(void) {
  int i;
  int numElem = currDir->fdb->num_entries;
  char **names = calloc(numElem, sizeof(char *));
  ret = SimpleFS_readDir(names, currDir);
  if (ret < 0) {
    printf("Errore nella readDir\n");
    free(names);
    return 0;
  }
  for (i = 0; i < numElem; i++) {
    printf("%s ", names[i]);
    free(names[i]);
  }
  printf("\n");
  free(names);
  return 0;
}

int touch(char **args) {
  FileHandle *ret;

  if (!args[1]) {
    printf("Inserisci il nome del file da creare\n");
    return 0;
  }
  ret = SimpleFS_createFile(currDir, args[1]);
  if (!ret) {
    printf("Errore nella crezione del file. Il file già esiste?\n");
    return 0;
  }
  printf("Creato file con nome: %s \n", args[1]);
  SimpleFS_close(ret);
  return 0;
}

int cp(char **args) {
  int readed_bytes = 0;
  int written_bytes = 0;
  if (!args[1] || !args[2]) {
    printf("Inserire il nome del file o il nuovo nome del file\n");
    return 0;
  }

  FileHandle *dst = SimpleFS_openFile(currDir, args[2]);
  if (!dst) {
    dst = SimpleFS_createFile(currDir, args[2]);
    if (!dst) {
      printf("Errore nella createFile\n");
      return 0;
    }
  }
  int src = open(args[1], O_RDONLY);
  if (src < 0) {
    printf("Errore nella open del file esterno al disco\n");
    SimpleFS_close(dst);
    return 0;
  }
  char buf[TRANSFER_SIZE];
  int bytes_read = 1;
  while (bytes_read != 0) {
    bytes_read = read(src, buf, TRANSFER_SIZE);
    if (bytes_read == -1) {
      printf("Errore nella read");
      goto exit;
    }
    readed_bytes += bytes_read;
    ret = SimpleFS_write(dst, buf, bytes_read);
    if (ret < 0) {
      printf("Errore nella write\n");
      goto exit;
    }
    written_bytes += ret;
  }

exit:
  printf("Bytes presenti nel file originale: %d\nBytes scritti nel file "
         "copiato : %d\n",
         readed_bytes, written_bytes);
  close(src);
  SimpleFS_close(dst);
  return 0;
}

int extractFile(char **args) {
  if (!args[1] || !args[2]) {
    printf("Inserire il nome del file o il nuovo nome del file\n");
    return 0;
  }

  FileHandle *src = SimpleFS_openFile(currDir, args[1]);
  if (!src) {
    printf("File non esistente\n");
    return 0;
  }
  int dst = open(args[2], O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (dst < 0) {
    printf("Errore nella open del file esterno al disco\n");
    SimpleFS_close(src);
    return 0;
  }
  char buf[TRANSFER_SIZE];
  int written_bytes = 0;
  int read_bytes = 0;
  int readed_bytes = 0;
  int fileLen = src->ffb->fcb.size_in_bytes;
  while (written_bytes < fileLen) {
    int to_read = TRANSFER_SIZE;
    if (fileLen - written_bytes < to_read) {
      to_read = fileLen - written_bytes;
    }
    read_bytes = SimpleFS_read(src, buf, to_read);
    if (read_bytes < 0) {
      goto exit;
    }
    readed_bytes += read_bytes;
    ret = write(dst, buf, read_bytes);
    if (ret < 0) {
      printf("Errore nella write\n");
      goto exit;
    }
    written_bytes += ret;
  }

exit:
  printf("Bytes presenti nel file originale: %d\nBytes scritti nel file "
         "estratto : %d\n",
         readed_bytes, written_bytes);
  close(dst);
  SimpleFS_close(src);
  return 0;
}

int cat(char **args) {
  if (!args[1]) {
    printf("Inserisci il nome del file da leggere\n");
    return 0;
  }

  FileHandle *src = SimpleFS_openFile(currDir, args[1]);
  if (!src) {
    printf("File non esistente\n");
    return 0;
  }

  char buf[1024];
  int readed_bytes = 0;
  int read_bytes = 0;
  int fileLen = src->ffb->fcb.size_in_bytes;
  while (readed_bytes < fileLen) {
    int to_read = 1024;
    if (fileLen - readed_bytes < to_read) {
      to_read = fileLen - readed_bytes;
    }
    memset(buf, 0, 1024);
    read_bytes = SimpleFS_read(src, buf, to_read);
    if (read_bytes < 0) {
      printf("Errore nella read\n");
      SimpleFS_close(src);
      return 0;
    }
    readed_bytes += read_bytes;
    printf("%s", buf);
  }
  printf("\n");
  printf("Bytes letti: %d\n", readed_bytes);
  SimpleFS_close(src);
  return 0;
}

int writeFile(char **args, char *line) {
  if (!args[1] || !args[2]) {
    printf("Inserire il nome del file o il testo da scrivere\n");
    return 0;
  }

  FileHandle *dst = SimpleFS_openFile(currDir, args[1]);
  if (!dst) {
    dst = SimpleFS_createFile(currDir, args[1]);
    if (!dst) {
      printf("Errore nella createFile\n");
      return 0;
    }
  }
  line += strlen(args[0]) + 1;
  line += strlen(args[1]) + 1;
  int len = strlen(line);
  ret = SimpleFS_write(dst, line, len - 1);
  if (ret < 0) {
    printf("Errore nella write\n");
  }
  printf("Bytes scritti: %d\n", ret);
  SimpleFS_close(dst);
  return 0;
}

int appendFile(char **args, char *line) {
  if (!args[1] || !args[2]) {
    printf("Inserire il nome del file o il testo da scrivere\n");
    return 0;
  }

  FileHandle *dst = SimpleFS_openFile(currDir, args[1]);
  if (!dst) {
    dst = SimpleFS_createFile(currDir, args[1]);
    if (!dst) {
      printf("Errore nella createFile\n");
      return 0;
    }
  }
  ret = SimpleFS_seek(dst, dst->ffb->fcb.size_in_bytes);
  if (ret < 0) {
    printf("Errore nella seek\n");
    SimpleFS_close(dst);
    return 0;
  }
  line += strlen(args[0]) + 1;
  line += strlen(args[1]) + 1;
  int len = strlen(line);
  ret = SimpleFS_write(dst, line, len - 1);
  if (ret < 0) {
    printf("Errore nella write\n");
  }
  printf("Bytes appesi: %d\n", ret);
  SimpleFS_close(dst);
  return 0;
}

int rm(char **args) {
  if (!args[1]) {
    printf("Inserisci il nome del file da rimuovere\n");
    return 0;
  }

  int ret = SimpleFS_remove(&fs, args[1]);
  if (ret) {
    printf("File inesistente\n");
  }
  return 0;
}

int execute_built_in_command(char **args, char *line) {
  if (strcmp(args[0], built_in_str[0]) == 0) {
    return mkDisk(args);
  }

  if (strcmp(args[0], built_in_str[1]) == 0) {
    return loadDisk(args);
  }

  if (strcmp(args[0], built_in_str[4]) == 0) {
    return help();
  }

  if (!loaded) {
    printf("Disco non caricato. Crealo/Caricalo per usare questo "
           "comando!\n");
    return 0;
  }

  if (strcmp(args[0], built_in_str[2]) == 0) {
    return formatDisk();
  }

  if (strcmp(args[0], built_in_str[3]) == 0) {
    return mountDisk();
  }

  if (strcmp(args[0], built_in_str[5]) == 0) {
    return shellExit();
  }

  if (!mounted) {
    printf("Disco non montato. Crealo/Caricalo e Montalo per usare questo "
           "comando!\n");
    return 0;
  }

  if (strcmp(args[0], built_in_str[6]) == 0) {
    return mkDir(args);
  }

  if (strcmp(args[0], built_in_str[7]) == 0) {
    return cd(args);
  }

  if (strcmp(args[0], built_in_str[8]) == 0) {
    return ls();
  }

  if (strcmp(args[0], built_in_str[9]) == 0) {
    return touch(args);
  }

  if (strcmp(args[0], built_in_str[10]) == 0) {
    return cp(args);
  }

  if (strcmp(args[0], built_in_str[11]) == 0) {
    return extractFile(args);
  }

  if (strcmp(args[0], built_in_str[12]) == 0) {
    return cat(args);
  }

  if (strcmp(args[0], built_in_str[13]) == 0) {
    return writeFile(args, line);
  }

  if (strcmp(args[0], built_in_str[14]) == 0) {
    return appendFile(args, line);
  }

  if (strcmp(args[0], built_in_str[15]) == 0) {
    return rm(args);
  }

  return 0;
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
  char *buffer = calloc(bufsize, sizeof(char));
  int c = '\0';
  int i = 0;

  while (c != '\n') {
    c = getchar();
    buffer[i] = c;
    i++;
  }

  return buffer;
}

void print_prompt(void) {

  if (mounted) {
    printf("\033[1;36m%s \033[0m", currDir->fdb->fcb.name);
  }
  printf(">> ");
}

int main(int argc, char **arg) {
  char *line;
  char **command;
  int flag = 0;

  printf("SimpleFS Test Shell ( Marangi 1816507 / Sudoso 1808353 )\n\n");
  printf("\n**********Dimensioni dei blocchi**********\n\n");
  printf("FileControlBlock size %ld\n", sizeof(FileControlBlock));
  printf("FirstFileBlock size %ld\n", sizeof(FirstFileBlock));
  printf("InodeBlock size %ld\n", sizeof(InodeBlock));
  printf("FileBlock size %ld\n", sizeof(FileBlock));
  printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));
  printf("\n******************************************\n");

  char *saved_line = calloc(1024, sizeof(char));
  while (!flag) {
    print_prompt();
    line = read_command_line();
    memcpy(saved_line, line, 1024);
    command = parse_command(line);

    if (command[0] != NULL) {
      if (check_built_in_command(command)) {
        flag = execute_built_in_command(command, saved_line);
      } else {
        printf("INVALID COMMAND!\n");
      }
    }
    free(command);
    free(line);
  }
  free(saved_line);
  return 0;
}
