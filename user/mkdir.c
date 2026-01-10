#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    printf(2, "Usage: mkdir files...\n");
    exit();
  }

  //for(i = 1; i < argc; i++){
  for(i = 1; i < 2; i++){
    //printf(1, "strlen %d, %s\n", strlen(argv[i]), argv[i]);

    if(mkdir(argv[i]) < 0){

      printf(2, "mkdir: %s failed to create\n", argv[i]);
      break;
    }
  }

  exit();
}
