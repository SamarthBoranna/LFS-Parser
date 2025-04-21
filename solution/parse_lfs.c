#include "parse_lfs.h"


int ls_function(){

}

int cat_function(){

}

int main(int argc, char ** argv)
{
    // P6 goes here

    char *instruction = NULL;
    char *img_path = NULL;

    if(argc > 4){
        exit(1);
    }

    instruction = strdup(argv[1]); //need to free

    if(argc == 4 && (strcmp(instruction, "cat") == 0)){
        img_path = strdup(argv[3]);
        char *file_to_cat = strdup(argv[2]);
        printf("img_path: %s, file_to_cat: %")
    }
    else if(argc == 3 && (strcmp(instruction, "ls") == 0)){
        img_path = strdup(argv[2]); //need to free
    }
    else{
        exit(1);
    }

    return 0;
}
