#include "fs.h"
#include <math.h>

int INE5412_FS::fs_format()
{
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
		for (int j = 0; j < POINTERS_PER_BLOCK; j++) {
			block.pointers[j] = 0;
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
void print_bitmap(std::vector<int> fblocks_bitmap) {
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
	if (block.super.magic != FS_MAGIC) {
		return 0;
	}
	int nblocks = block.super.nblocks;
	fblocks_bitmap.assign(block.super.nblocks,0);	
	if (nblocks != fblocks_bitmap.size()) {
		cout << "nblocks != fblocks_bitmap.size()" << endl;
		return 0;
	}
	print_bitmap(fblocks_bitmap);
	fblocks_bitmap[0] = 1;
	print_bitmap(fblocks_bitmap);
	for (int i = 1; i <= block.super.ninodeblocks; i++) {
    if (i < 0 || i >= nblocks) {
        cerr << "ERROR: Inode block (" << i << ") is out of bounds!" << endl;
        return 0;
    }
    disk->read(i, block.data);

    for (int j = 0; j < INODES_PER_BLOCK; j++) {
        if (block.inode[j].isvalid) {
            if (i < 0 || i >= nblocks) {
                cerr << "ERROR: Inode block (" << i << ") is out of bounds!" << endl;
                return 0;
            }
            fblocks_bitmap[i] = 1;

            for (int a = 0; a < POINTERS_PER_INODE; a++) {
                if (block.inode[j].direct[a]) {
                    if (block.inode[j].direct[a] < 0 || block.inode[j].direct[a] >= nblocks) {
                        cerr << "ERROR: Direct block (" << block.inode[j].direct[a] << ") is out of bounds!" << endl;
                        return 0;
                    }
                    fblocks_bitmap[block.inode[j].direct[a]] = 1;
                }
            }

            if (block.inode[j].indirect) {
                if (block.inode[j].indirect < 0 || block.inode[j].indirect >= nblocks) {
                    cerr << "ERROR: Indirect block (" << block.inode[j].indirect << ") is out of bounds!" << endl;
                    return 0;
                }
                fblocks_bitmap[block.inode[j].indirect] = 1;

                union fs_block ind_block;
                disk->read(block.inode[j].indirect, ind_block.data);
                for (int b = 0; b < POINTERS_PER_BLOCK; b++) {
                    if (ind_block.pointers[b]) {
                        if (ind_block.pointers[b] < 0 || ind_block.pointers[b] >= nblocks) {
                            cerr << "ERROR: Indirect data block (" << ind_block.pointers[b] << ") is out of bounds!" << endl;
                            return 0;
                        }
                        fblocks_bitmap[ind_block.pointers[b]] = 1;
                    }
                }
            }
        }
    }
}

	
	return 1;
	}
	


int INE5412_FS::fs_create()
{
	print_bitmap(fblocks_bitmap);
	return 0;
}

int INE5412_FS::fs_delete(int inumber)
{
	return 0;
}

int INE5412_FS::fs_getsize(int inumber)
{
	return -1;
}

int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
	return 0;
}

int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
	return 0;
}
