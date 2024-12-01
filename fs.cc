#include "fs.h"
#include <math.h>

int INE5412_FS::fs_format()
{
    if (mounted) {
        cerr << "ERROR: Disk is already mounted" << endl;
        return 0;
    }

	union fs_block block;
	disk->read(0, block.data);

	int nblocks = disk->size();
	int ninodeblocks = ceil(nblocks*0.1);
	int ninodes = ninodeblocks*INODES_PER_BLOCK;

	block.super.magic = FS_MAGIC;
	block.super.nblocks = nblocks;
	block.super.ninodeblocks = ninodeblocks;
	block.super.ninodes = ninodes;

	disk->write(0, block.data);

	for (int i = 1; i <= ninodeblocks; i++) {
		for (int j = 0; j < INODES_PER_BLOCK; j++) {
			block.inode[j].isvalid = 0;
			block.inode[j].size = 0;
			for (int k = 0; k < POINTERS_PER_INODE; k++) {
				block.inode[j].direct[k] = 0;
			}
			block.inode[j].indirect = 0;
		}
		disk->write(i, block.data);
	}

	for (int i = ninodeblocks + 1; i < nblocks; i++) {
		for (int j = 0; j < Disk::DISK_BLOCK_SIZE; j++) {
			block.data[j] = 0;
		}
		disk->write(i, block.data);
	}
	return 1;
}

void INE5412_FS::fs_debug()
{
	union fs_block block;

	disk->read(0, block.data);

	cout << "superblock:\n";
	cout << "    " << (block.super.magic == FS_MAGIC ? "magic number is valid\n" : "magic number is invalid!\n");
 	cout << "    " << block.super.nblocks << " blocks\n";
	cout << "    " << block.super.ninodeblocks << " inode blocks\n";
	cout << "    " << block.super.ninodes << " inodes\n";

    for(int i = 1; i <= block.super.ninodeblocks; i++) {
        disk->read(i, block.data);
        for(int j = 0; j < INODES_PER_BLOCK; j++) {

            if(block.inode[j].isvalid) {
				cout << "inode " << (i-1)*INODES_PER_BLOCK+j+1 << ":\n"; //index of inode
                cout << "    size: " << block.inode[j].size << " bytes\n";
                cout << "    direct blocks: ";

                for(int k = 0; k < POINTERS_PER_INODE; k++) {
                    if(block.inode[j].direct[k]) {
						cout << block.inode[j].direct[k] << " ";
					}
                }
                cout << "\n";

                if(block.inode[j].indirect){
					cout << "    indirect block: " << block.inode[j].indirect << "\n";
					cout << "    indirect data blocks: ";
                    union fs_block ind_block;
                    disk->read(block.inode[j].indirect,ind_block.data);
                    for(int k = 0; k < POINTERS_PER_BLOCK; k++) {
                        if(ind_block.pointers[k])
						{
							cout << ind_block.pointers[k]<< " " ;
						} 
                    }
                    cout << "\n";
                }
            }
        }    
    }
	}
void print_bitmap(const std::vector<int>& fblocks_bitmap) {
    cout << "Bitmap: ";
    for (std::size_t i = 0; i < fblocks_bitmap.size(); ++i) {
        cout << fblocks_bitmap[i] << " ";
    }
    cout << endl;
}

int INE5412_FS::fs_mount()
{
    union fs_block block;
    disk->read(0, block.data);
    fs_superblock superblock = block.super;
    if (superblock.magic != FS_MAGIC) {
        cerr << "ERROR: Invalid magic number!" << endl;
        return 0;
    }

    fblocks_bitmap.clear();
    fblocks_bitmap.resize(superblock.nblocks, 0);

    fblocks_bitmap[0] = 1; 
    cout << "fbsize: " << fblocks_bitmap.size() << endl;
    cout << "nblocks: " << superblock.nblocks << endl;
    cout << "ninodeblocks: " << superblock.ninodeblocks << endl;
    cout << "ninodes: " << superblock.ninodes << endl;
    cout << disk->size() << endl;

    for (int a = 1; a <= superblock.ninodeblocks; a++) {

        disk->read(a, block.data);

        for (int b = 0; b < INODES_PER_BLOCK; b++) {
            if (block.inode[b].isvalid) {
                fblocks_bitmap[a] = 1;

                for (int c = 0; c < POINTERS_PER_INODE; c++) {
                    if (block.inode[b].direct[c]) { 
                        fblocks_bitmap[int(block.inode[b].direct[c])] = 1;
                    }
                }
                if (block.inode[b].indirect) {
                    fblocks_bitmap[int(block.inode[b].indirect)] = 1;

                    union fs_block ind_block;

                    disk->read(block.inode[b].indirect, ind_block.data);

                    for (int d = 0; d < POINTERS_PER_BLOCK; d++) {
                        fblocks_bitmap[int(ind_block.pointers[d])] = 1;
                    }
                }
            }
        }    
    }     
    print_bitmap(fblocks_bitmap);
    mounted = true;
    return 1;
}
void INE5412_FS::inode_load( int inumber, class fs_inode *inode )
{
    if (inumber < 1 || inumber > superblock.ninodes) {
        cerr << "ERROR: Invalid inumber" << endl;
        inode->isvalid = 0;
        return;
    }
    int block_number = 1 + (inumber - 1) / INODES_PER_BLOCK;
    int inode_index = (inumber - 1) % INODES_PER_BLOCK;
    
    union fs_block block;
    disk->read(block_number, block.data);
    *inode = block.inode[inode_index];
}

void INE5412_FS::inode_save( int inumber, class fs_inode *inode ) 
{
    if (inumber < 1 || inumber > superblock.ninodes) {
        cerr << "ERROR: Invalid inumber" << endl;
        inode->isvalid = 0;
        return;
    }
    int block_number = 1 + (inumber - 1) / INODES_PER_BLOCK;
    int inode_index = (inumber - 1) % INODES_PER_BLOCK;
    union fs_block block;
    disk->read(block_number, block.data);
    block.inode[inode_index] = *inode;

    disk->write(block_number, block.data);
}

int INE5412_FS::fs_create()
{
    if (!mounted) {
        cerr << "ERROR: Disk is not mounted" << endl;
        return 0;
    }

    union fs_block block;
    disk->read(0, block.data);
    fs_inode inode;
    int inumber;

    for (inumber = 1; inumber <= block.super.ninodes; inumber++) {
        inode_load(inumber, &inode);
        if (!inode.isvalid) {
            inode.isvalid = 1;
            inode.size = 0;
            for (int i = 0; i < POINTERS_PER_INODE; i++) {
                inode.direct[i] = 0;
            }
            inode.indirect = 0;

            inode_save(inumber, &inode);
            fs_mount();
            return inumber;
        }

    }
    cerr << "ERROR: inode table is full" << endl;
	return 0;
}

int INE5412_FS::fs_delete(int inumber)
{
    if (!mounted) {
        cerr << "ERROR: Disk is not mounted" << endl;
        return 0;
    }

    fs_inode inode;
    inode_load(inumber, &inode);

    if (!inode.isvalid) {
        cerr << "ERROR: Invalid inumber" << endl;
        return 0;
    }

    inode.isvalid = 0;
    inode.size = 0;

    for (int i = 0; i < POINTERS_PER_INODE; i++) {
        if (inode.direct[i]) {
            fblocks_bitmap[inode.direct[i]] = 0;
        }
    }
    if (inode.indirect) {
        fblocks_bitmap[inode.indirect] = 0;
        
        union fs_block ind_block;
        disk->read(inode.indirect, ind_block.data);
        for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
            if (ind_block.pointers[i]) {
                fblocks_bitmap[ind_block.pointers[i]] = 0;
            }
        }
    }
    
    inode_save(inumber, &inode);

    fs_mount();

    print_bitmap(fblocks_bitmap);

	return 1;
}

int INE5412_FS::fs_getsize(int inumber)
{
    if (!mounted) {
        cerr << "ERROR: Disk is not mounted" << endl;
        return 0;
    }
    fs_inode inode;
    inode_load(inumber, &inode);
    if (inode.isvalid) {
        return inode.size;
    }
    cerr << "ERROR: Invalid inumber" << endl;
	return -1;
}

int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
    if (!mounted) {
        cerr << "ERROR: Disk is not mounted" << endl;
        return 0;
    }

    fs_inode inode;

    inode_load(inumber, &inode);

    if(!inode.isvalid) {
        cerr << "ERROR: Invalid inumber" << endl;
        return 0;
    }
    int inode_size = fs_getsize(inumber);

    if (offset > inode_size) {
        cerr << "ERROR: offset is equal or greater than inode size" << endl;
        return 0;
    }
    if (inode_size == 0) {
        cerr << "ERROR: inode size is 0" << endl;
        return 0;
    }

    int length_to_read = min(length, inode_size - offset);

    int block_i = offset / Disk::DISK_BLOCK_SIZE;
    int block_offset = offset % Disk::DISK_BLOCK_SIZE;

    int bytes_read = 0;

    while (bytes_read < length_to_read) {
        int block_num = get_dblocknum(inode, block_i);
        if (block_num == 0) {
            cerr << "ERROR: block number is 0" << endl;
            return 0;
        }

        union fs_block block;
        disk->read(block_num, block.data);

        int first_block_bytes = Disk::DISK_BLOCK_SIZE - block_offset;   // bytes left to read in the first block
        int bytes_to_read = min(length_to_read - bytes_read, first_block_bytes);

        memcpy(data + bytes_read, block.data + block_offset, bytes_to_read);

        bytes_read += bytes_to_read;
        block_i++;
        block_offset = 0;   // this offset is only used in the first block
    }
    return bytes_read;
}


int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
	return 0;
}

int INE5412_FS::get_dblocknum(fs_inode &inode, int block_i) {
    int block_num;

    if (block_i < POINTERS_PER_INODE) {
        block_num = inode.direct[block_i];
        return block_num;
    } 

    if (inode.indirect == 0) {
        cerr << "ERROR: Indirect block is not allocated" << endl;
        return 0;
    }

    union fs_block ind_block;

    disk->read(inode.indirect, ind_block.data);

    int indblock_i = block_i - POINTERS_PER_INODE;
    if (indblock_i >= POINTERS_PER_BLOCK) {
        cerr << "ERROR: Block index is out of range" << endl;
        return 0;
    }

    block_num = ind_block.pointers[indblock_i];
    return block_num;
} 