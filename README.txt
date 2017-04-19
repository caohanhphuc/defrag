Stephanie Cao
CS355, hw6

1. How the program is structured:
   To defrag the disk, I first read the whole disk image to memory. Since the whole file is stored in memory, there is a 1G limit of disk size.
   After that, I copy the first 1024 bytes (boot block and super block) to the new disk.I position the stream of the new disk to where the data region starts, then track all data blocks and put blocks of the same file next to each other.
   After finishing tracking an inode, I place the stream position to where that inode is supposed to be, and write it there then move back the stream position to the end. First I planned to save a copy of the inode region and then write the whole region to the file, but that did not work out, therefore I went with the above solution. Since it has to reposition the stream many more times and write smaller blocks, it is slower, but specifically to the sample file with only 20 inodes it performs fine.
   Last, after all data blocks are written, I change the free_block index inside the super block. Then I check if the swap offset is valid to decide whether to copy that region over.

2. Where the program works:
   After defrag, I check its accuracy with these tests:
   - New disk and old disk have the same size
   - disk-defrag and disk-defrag-defrag are totally similar
   - My disk-defrag and Xinyue's disk-herdefrag-mydefrag share the same inode region

   There's no memory leak or memory error.

3. Where the program doesn't work:
   Probably tracking i2block and i3block. I have checked my tracking steps by hand very carefully, however, I failed to create a unit test for these types of blocks. I attempted creating fake i2blocks and i3blocks to track them, but I could not get it right. This might lead to crashing when a disk has inodes with i2block and i3block, so I tried error checking, but my error checking is not thorough enough to prevent all cases when the program can crash because of tracking i2blocks and i3blocks.
