#include "defrag.h"

superblock *sb;
void* buffer, *newInodes;
FILE *newdisk;
int usedBlocks, filesize;

int main(int argc, char *argv[]){
  char *filename;
  if (argc <= 1 || argc > 2){
    printf("Try './defrag -h' for more information.\n");
    exit(1);
  }
  if (argc == 2) {
    if (strcmp(argv[1], "-h") == 0){
      printf("Usage:\t  ./defrag [DISK FILE]\nOutput:\t  Defragmented disk image with ”-defrag” concatenated to input file name\nExample:  ”./defrag myfile” produces the output file ”myfile-defrag”\n");
      exit(1);
    } else {
      filename = argv[1];
    }
  }
  readInput(filename);
  defrag();
  fillFreeBlocks();
  free(buffer);
  free(newInodes);
  fclose(newdisk);
  return 0;
}

void readInput(char* address){
  //writing content to separate file for error checking
  //text = fopen("content2", "w+");
  
  usedBlocks = 0; //set block index

  //read disk image & concat name for new disk image
  FILE *fp = fopen(address, "r");
  if (fp == NULL) {
    printf("File not found.\n");
    exit(1);
  }
  int namelen = strlen(address) + strlen("-defrag") + 1;
  char* newname = (char*)malloc(namelen);
  strcpy(newname, address);
  strcat(newname, "-defrag");
  newdisk = fopen(newname, "w+");
  free(newname);
  if (newdisk == NULL) errexit();

  if (fseek(fp, 0, SEEK_END) == -1) errexit();
  filesize = ftell(fp);
  if (filesize == -1) errexit();
  if (filesize > 1073741824){
    printf("Not enough memory to defrag disk.\n");
    exit(1);
  }
  if (fseek(fp, 0, SEEK_SET) == -1) errexit();
  buffer = malloc(filesize);
  if (buffer == NULL){
    printf("Not enough memory to defrag disk.\n");
    exit(1);
  }
  if (fread(buffer, 1, filesize, fp) == -1) errexit();

  sb = (superblock*) (buffer + 512);
#undef BLOCKSIZE
#define BLOCKSIZE sb->size
 
  if (sb->inode_offset >= sb->data_offset) errexit();
  //if (fwrite(buffer, 1, 1024, newdisk) != 1024) errexit();
  if (fseek(newdisk, 1024 + BLOCKSIZE * (sb->data_offset), SEEK_SET) == -1) errexit();
  fclose(fp);
  return;
}

int defrag(){
  int numInode = (sb->data_offset - sb->inode_offset) * BLOCKSIZE / (INODESIZE + 0.0);
#undef N_INODES
#define N_INODES numInode
  //create copy
  newInodes = malloc(INODESIZE * N_INODES);
  //if (newInodes == NULL) errexit();
  memcpy(newInodes, buffer + 1024 + BLOCKSIZE * sb->inode_offset, INODESIZE * N_INODES);
  
  for (int i = 0; i < N_INODES; i++){
    inode *temp = (inode*) (buffer + 1024 + BLOCKSIZE * sb->inode_offset + i * INODESIZE);
    //printf("\nInode #%d:\n", i);
    if (temp->nlink == 1){
      inode *newtemp = (inode*) (newInodes + i * INODESIZE);
      trackBlock(temp, newtemp, i);
    }
  }
  return 0;
}

void writeBlock(int index){ 
  void *block = buffer + 1024 + BLOCKSIZE * sb->data_offset + BLOCKSIZE * index;

  //writing content to separate file
  //fwrite(block, 1, BLOCKSIZE, text);
  
  if (fwrite(block, 1, BLOCKSIZE, newdisk) != BLOCKSIZE) errexit();

  //printf("wrote data block, old address %d, new address %d\n", index, usedBlocks); 
}

void writeIndexBlock(int start, int end){
  int indices[BLOCKSIZE/4];
  int j = 0;
  for (int i = start; i <= end; i++){
    indices[j] = i;
    j++;
  }

  if (fwrite(indices, 1, BLOCKSIZE, newdisk) != BLOCKSIZE) errexit();
  if (fseek(newdisk, 0, SEEK_END) == -1) errexit();
    
  //printf("Wrote index block, with address %d, indices from %d to %d\n", usedBlocks, usedBlocks + 1, usedBlocks + BLOCKSIZE/4);
}

void writeInode(inode* node, int index){
  long add = 1024 + BLOCKSIZE * sb->inode_offset + index * sizeof(inode);
  if (fseek(newdisk, add, SEEK_SET) == -1) errexit();
  if (fwrite(node, 1, INODESIZE, newdisk) != INODESIZE) errexit();
  if (fseek(newdisk, 0, SEEK_END) == -1) errexit();
}

void trackBlock(inode *node, inode *temp, int index){
  //tracking all blocks
  int dataLeft = node->size;

  //direct blocks
  for (int i = 0; i < N_DBLOCKS; i++){
    if (dataLeft <= 0) break;
    //printf("block: %d\n", node->dblocks[i]);
    writeBlock(node->dblocks[i]);
    temp->dblocks[i] = usedBlocks;
    usedBlocks++;
    //printf("WRITING direct block: %d\n", temp->dblocks[i]);
    dataLeft = dataLeft - BLOCKSIZE;
  }

  //indirect blocks
  for (int i = 0; i < N_IBLOCKS; i++){
    if (dataLeft <= 0) break;
    int ival = node->iblocks[i];
    writeIndexBlock(usedBlocks + 1, usedBlocks + BLOCKSIZE/4);
    temp->iblocks[i] = usedBlocks;
    usedBlocks++;
    for (int j = 0; j < BLOCKSIZE; j+=4){
      if (dataLeft <= 0) break;
      void *dadd = buffer + 1024 + BLOCKSIZE * sb->data_offset + BLOCKSIZE * ival + j;
      int dval = *((int*) dadd);
      //printf("block: %d, pointed from iblock %d\n", *(int*) dadd, node->iblocks[i]);
      writeBlock(dval);
      //printf("WRITING block: %d, pointed from iblock %d\n", usedBlocks, temp->iblocks[i]);
      usedBlocks++;
      dataLeft = dataLeft - BLOCKSIZE;
    }
  }

  //i2blocks
  for (int i = 0; i < 1; i++){
    if (dataLeft <= 0) break;
    int i2val = node->i2block;
    writeIndexBlock(usedBlocks + 1, usedBlocks + BLOCKSIZE / 4);
    temp->i2block = usedBlocks;
    usedBlocks++;
    int count = dataLeft;
    int numBlocks = 0;
    for (int j = 0; j < BLOCKSIZE; j+=4){
      if (count <= 0) break;
      writeIndexBlock(usedBlocks + (j/4+1)*BLOCKSIZE/4, usedBlocks + (j/4+2)*BLOCKSIZE/4 -1);
      numBlocks++;
      count = count - BLOCKSIZE;
    }
    usedBlocks += numBlocks; 
    for (int j = 0; j < BLOCKSIZE; j+=4){
      if (dataLeft <= 0) break;
      void* iadd = buffer + 1024 + BLOCKSIZE*sb->data_offset + BLOCKSIZE*i2val + j;
      if (iadd >= buffer+filesize || iadd < buffer) errexit();
      int ival = *((int*) iadd);
      for (int k = 0; k < BLOCKSIZE; k+=4){
	if (dataLeft <= 0) break;
	void *dadd = buffer + 1024 + BLOCKSIZE * sb->data_offset + BLOCKSIZE * ival + j;
	if (dadd >= buffer+filesize || dadd < buffer) errexit();
	int dval = *((int*) dadd);
	//printf("block: %d, pointed from iblock %d, i2block %d\n", dval, ival, node->i2block);
	writeBlock(dval);
	usedBlocks++;
	dataLeft = dataLeft - BLOCKSIZE;
      }
    }
  }

  //i3blocks
  for (int i = 0; i < 1; i++){
    if (dataLeft <= 0) break;
    int i3val = node->i3block;
    writeIndexBlock(usedBlocks + 1, usedBlocks + BLOCKSIZE / 4);
    temp->i2block = usedBlocks;
    usedBlocks++;
    int count = dataLeft;
    int curIndex = usedBlocks;
    for (int j = 0; j < BLOCKSIZE; j+=4){
      if (count <= 0) break;
      writeIndexBlock(curIndex + (j/4+1)*BLOCKSIZE/4, curIndex + (j/4+2)*BLOCKSIZE/4 -1);
      usedBlocks++;
      count = count - BLOCKSIZE;
    }
    int startdblock = (curIndex + (BLOCKSIZE/4 + 1)*BLOCKSIZE/4);
  
    for (int k = 0; k < BLOCKSIZE*BLOCKSIZE; k+=(BLOCKSIZE/4)){
      if (count <= 0) break;
      writeIndexBlock(startdblock, startdblock + BLOCKSIZE/4 - 1);
      startdblock += BLOCKSIZE/4;
      usedBlocks++;
      count = count - BLOCKSIZE;
    }
    
    for (int j = 0; j < BLOCKSIZE; j+=4){
      if (dataLeft <= 0) break;
      void* i2add = buffer + 1024 + BLOCKSIZE*sb->data_offset + BLOCKSIZE*i3val + j;
      if (i2add >= buffer+filesize || i2add < buffer) errexit();
      int i2val = *((int*) i2add);
      for (int k = 0; k < BLOCKSIZE; k+=4){
	if (dataLeft <= 0) break;
	void *iadd = buffer + 1024 + BLOCKSIZE * sb->data_offset + BLOCKSIZE * i2val + j;
	if (iadd >= buffer+filesize || iadd < buffer) errexit();
	int ival = *((int*) iadd);
	for (int l = 0; l < BLOCKSIZE; l+=4){
	  if (dataLeft <= 0) break;
	  void *dadd = buffer + 1024 + BLOCKSIZE*sb->data_offset + BLOCKSIZE * ival + j;
	  if (dadd >= buffer+filesize || dadd < buffer) errexit();
	  int dval = *((int*) dadd);
	  //printf("block: %d, pointed from iblock %d, i2block %d, i3block %d\n", dval, ival, i2val, node->i3block);
	  writeBlock(dval);
	  usedBlocks++;
	  dataLeft = dataLeft - BLOCKSIZE;
	}
      }
    }
  }

  writeInode(temp, index);
  return;
}

int fillFreeBlocks(){
  sb->free_block = usedBlocks;
  rewind(newdisk);
  if (fwrite(buffer, 1, 1024, newdisk) != 1024) errexit();
  if (fseek(newdisk, 0, SEEK_END) == -1) errexit();
  
  int hasSwap = 1;
  void* swapadd = buffer + 1024 + BLOCKSIZE * sb->swap_offset;
  void* maxDataAdd = swapadd - 1;
  if (swapadd >= buffer+filesize || swapadd < buffer+1024){
    hasSwap = 0;
    //printf("Invalid swap offset\n");
    maxDataAdd = buffer + filesize - 1;
  }

  void* temp = buffer + 1024 + BLOCKSIZE * sb->data_offset + BLOCKSIZE * usedBlocks;
  int numFreeBlocks = (maxDataAdd - (buffer+1024+BLOCKSIZE*(sb->data_offset + usedBlocks - 1) + 1)) / 512;
  for (int i = 0; i < numFreeBlocks-1; i++){
    *(int*)temp = usedBlocks;
    fwrite(temp, 1, BLOCKSIZE, newdisk);
    usedBlocks++;
    temp = temp + BLOCKSIZE;
  }

  *(int*)(temp) = -1;
  fwrite(temp, 1, BLOCKSIZE, newdisk);
		       
  /*while (temp < maxDataAdd - BLOCKSIZE){
    *(int*)temp = usedBlocks + 1;
    fwrite(temp, 1, BLOCKSIZE, newdisk);
    usedBlocks++;
    temp = temp + BLOCKSIZE;
  }

  *(int*) temp = -1;
  fwrite(temp, 1, BLOCKSIZE, newdisk);
  */

  //copy swap region
  if (hasSwap == 1){
    fwrite(swapadd, 1, buffer + filesize - swapadd - 1, newdisk);
  }
  return 0;
}

void errexit(){
  free(buffer);
  free(newInodes);
  fclose(newdisk);
  printf("Corrupted disk image.\n");
  exit(1);
}
