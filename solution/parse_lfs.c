#include "parse_lfs.h"

Imap *imap = NULL;
CR *checkpoint_region = NULL;
Inode *nodes = NULL;

void initImap(){
    imap = malloc(sizeof(Imap));
    imap->num_entries = 0;
    imap->InodeEntry_arr = malloc(sizeof(InodeEntry));
}

void initCheckpoint(){
    checkpoint_region = malloc(sizeof(CR));
    checkpoint_region->image_offset = 0;
    checkpoint_region->num_entries = 0;
    checkpoint_region->ImapEntry_arr = NULL;
}

void parse_checkpoint_region(char *base){
    uint image_offset = *(uint *)(base);
    char *entry_count_path = base + 4;
    uint entry_count = *(uint *)(entry_count_path);
    checkpoint_region->image_offset = image_offset;
    checkpoint_region->num_entries = entry_count;
    checkpoint_region->ImapEntry_arr = malloc(entry_count * sizeof(ImapEntry));
    char *next_entry_path = entry_count_path + 4;
    for(int i = 0; i < (int)entry_count; i++){
        uint inumber_start = *(uint *)(next_entry_path);
        uint inumber_end = *(uint *)(next_entry_path + 4);
        uint imap_disk_offset = *(uint *)(next_entry_path + 8);
        checkpoint_region->ImapEntry_arr[i].inumber_start = inumber_start;
        checkpoint_region->ImapEntry_arr[i].inumber_end = inumber_end;
        checkpoint_region->ImapEntry_arr[i].imap_disk_offset = imap_disk_offset;
        next_entry_path = next_entry_path + 12;
    }
}

int get_dir_offsets(Inode *node, char* start_of_offsets){
    uint num_direct = node->num_direct_blocks;
    if (num_direct == 0) {
        return 0;
      }
    else{
        node->dir_block_offs = malloc(num_direct * sizeof(uint));
        for(int i = 0; i < (int)num_direct; i++){
            node->dir_block_offs[i] = *(uint *)start_of_offsets;
            start_of_offsets = start_of_offsets + 4;
        }
    }

    return 0;
}

int node_mapping(char *base){
    for(int i = 0; i<(int)imap->num_entries; i++){
        InodeEntry node = imap->InodeEntry_arr[i];
        uint inode_num = node.inumber;
        uint offset = node.inode_disk_offset;
        char *node_base = base + offset;

        uint file_cursor = *(uint *)(node_base);
        uint size = *(uint *)(node_base + 4);
        mode_t permissions = *(mode_t *)(node_base + 8);
        time_t mtime = *(time_t *)(node_base + 12);
        uint direct_count = *(uint *)(node_base + 20);

        nodes[i].inumber = inode_num;
        nodes[i].file_cursor = file_cursor;
        nodes[i].size = size;
        nodes[i].permissions = permissions;
        nodes[i].mtime = mtime;
        nodes[i].num_direct_blocks = direct_count;
        nodes[i].dir_block_offs = NULL;
        nodes[i].parent_inum = UINT_MAX;
        get_dir_offsets(&nodes[i], node_base + 24);
    }

    return 0;
}

void parse_imap(char *base){
    int num_entries = (int)checkpoint_region->num_entries;
    uint imap_arr_index = 0;
    for(int i = 0; i<num_entries; i++){
        char* imap_path = base + checkpoint_region->ImapEntry_arr[i].imap_disk_offset;
        uint imap_entries = *(uint *)imap_path;
        imap->InodeEntry_arr = realloc(imap->InodeEntry_arr, (imap_arr_index + imap_entries) * sizeof(InodeEntry));
        if(imap->InodeEntry_arr == NULL){
            perror("realloc");
            exit(1);
        }
        char *imap_inode_data_path = imap_path + 4;

        for (uint j = 0; j < imap_entries; j++) {
            uint inum = *(uint *)imap_inode_data_path;
            uint iNode_offset = *(uint *)(imap_inode_data_path + 4);
            imap->InodeEntry_arr[imap_arr_index + j].inumber = inum;
            imap->InodeEntry_arr[imap_arr_index + j].inode_disk_offset = iNode_offset;
            imap_inode_data_path += 8;
        }
        imap_arr_index += imap_entries;
        imap->num_entries += imap_entries;
    }
    return;
}

void getfilenames(char *base){
    for(int i = 0; i<(int)imap->num_entries; i++){
        Inode *node = &nodes[i];
        if(!S_ISDIR(node->permissions)) continue;

        uint parent_num = node->inumber;
        for(int j = 0; j<(int)node->num_direct_blocks; j++){
            char *dblock_base = base + node->dir_block_offs[j];
            size_t curr = 0;
            while(curr < 4096){
                char *ret = memchr(dblock_base + curr, ',', 4096 - curr);
                if(!ret) break;
                size_t length = ret - (dblock_base + curr);
                char *name = malloc(length + 1);
                memcpy(name, dblock_base + curr, length);
                name[length] = '\0';
                uint inode_match;
                memcpy(&inode_match, ret + 1, sizeof(uint));
                
                for(int k = 0; k<(int)imap->num_entries; k++){
                    if(nodes[k].inumber == inode_match){
                        free(nodes[k].filename);
                        nodes[k].filename = strdup(name);
                        nodes[k].parent_inum = parent_num;
                        break;
                    }
                }
                free(name);
                curr += length + 1 + sizeof(uint);
            }
        }
    }
}


int getDepth(uint parentNum){
    if(parentNum == UINT_MAX){
        return 0;
    }
    else{
        for(int i = 0; i < (int)imap->num_entries; i++){
            if(nodes[i].inumber == parentNum){
                return 1 + getDepth(nodes[i].parent_inum);
            }
        }
    }
    return -1;
}


int setDepth(){
    for(int i = 1; i<(int)imap->num_entries; i++){
        int depth = getDepth(nodes[i].parent_inum);
        if(depth == -1){
            return -1;
        }
        else if(depth == 0){
            nodes[i].depth = UINT_MAX;
        }
        else{
            nodes[i].depth = depth;
        }
    }
    return 0;
}

void ls_func(){
    for(int i = 0; i < (int)imap->num_entries; i++){
        Inode node = nodes[i];
        if(node.inumber == 0){continue;}
        ls_print_file(node.filename, node.size, node.permissions, node.mtime, node.depth);
    }
}

void printCR(){
    printf("CR offset: %d\n", checkpoint_region->image_offset);
    printf("CR entries: %d\n", checkpoint_region->num_entries);
    for(uint i = 0; i<checkpoint_region->num_entries; i++){
        printf("Imap Entry %d, inumber_start: %d\n", i, checkpoint_region->ImapEntry_arr[i].inumber_start);
        printf("Imap Entry %d, inumber_end: %d\n", i, checkpoint_region->ImapEntry_arr[i].inumber_end);
        printf("Imap Entry %d, imap_disk_offset: %d\n", i, checkpoint_region->ImapEntry_arr[i].imap_disk_offset);
    }
}

void printImap(){
    printf("Imap entries: %d\n", imap->num_entries);
    for(int i = 0; i<(int)imap->num_entries; i++){
        printf("Entry %d\n", i);
        printf("inumber: %d\n", imap->InodeEntry_arr[i].inumber);
        printf("inode offset: %d\n", imap->InodeEntry_arr[i].inode_disk_offset);
    }
}

void print_inodes(int offsets){
    for(int i = 0; i<(int)imap->num_entries; i++){
        uint node_dirs = nodes[i].num_direct_blocks;
        printf("Node %d\n", i);
        printf("Filename: %s\n", nodes[i].filename);
        printf("Inode Number: %d\n", nodes[i].inumber);
        printf("Parent Num: %d\n", nodes[i].parent_inum);
        if(offsets != 0){
            for(int j = 0; j < (int)node_dirs; j++){
                printf("offset %d: %d\n", j, nodes[i].dir_block_offs[j]);
            }
        }
        printf("\n");
    }
}

int main(int argc, char ** argv)
{
    // P6 goes here

    initCheckpoint();
    initImap();

    char *instruction = NULL;
    char *img_path = NULL;
    int fd;
    char *img_base;
    struct stat st;
    int inst_flag = -1;

    if(argc > 4){
        exit(1);
    }

    instruction = strdup(argv[1]);
    
    if(argc == 3 && (strcmp(instruction, "ls") == 0)){
        img_path = strdup(argv[2]);
        //printf("inst: %s, img_path: %s\n", instruction, img_path);
        inst_flag = LS;
    }
    else if(argc == 4 && (strcmp(instruction, "cat") == 0)){
        img_path = strdup(argv[3]);
        char *file_to_cat = strdup(argv[2]);
        //printf("inst: %s, img_path: %s, file_to_cat: %s\n", instruction, img_path, file_to_cat);
        free(file_to_cat);
        inst_flag = CAT;
    }
    else{
        exit(1);
    }

    fd = open(img_path, O_RDONLY);
    if (fd == -1){
        perror("open");
        exit(1);
    }
    
    if (fstat(fd, &st) == -1){
        perror("fstat");
    }

    size_t size = st.st_size;
    img_base = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    parse_checkpoint_region(img_base);
    //printCR();

    parse_imap(img_base);
    //printImap();

    nodes = malloc(imap->num_entries * sizeof(Inode));

    node_mapping(img_base);
    getfilenames(img_base);

    //print_inodes(0);

    setDepth();

    if(inst_flag == LS){
        ls_func();
    }
    else if(inst_flag == CAT){

    }else{
        free(instruction);
        free(img_path);
        exit(1);
    }

    free(instruction);
    free(img_path);
    return 0;
}
