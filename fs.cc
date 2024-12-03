#include "fs.h"
#include <math.h>

int INE5412_FS::fs_format()
{
    //  verifica se esta montado
    if (mounted) {
        cerr << "ERROR: Disk is already mounted" << endl;
        return 0;
    }

	union fs_block block;
	disk->read(0, block.data);  // lê o superbloco

	int nblocks = disk->size();  // pega o tamanho dos blocos
	int ninodeblocks = ceil(nblocks*0.1);   // calcula o n de blocos de inode, pegando 10% do tamanho dos blocos e arredondando para cima
	int ninodes = ninodeblocks*INODES_PER_BLOCK;    // calcula o n de inodes

    // prenche o superbloco, com o magic number, n de blocos, n de blocos de inode e n de inodes
	block.super.magic = FS_MAGIC;
	block.super.nblocks = nblocks;
	block.super.ninodeblocks = ninodeblocks;
	block.super.ninodes = ninodes;

	disk->write(0, block.data); // escreve o superbloco

    // loop que formata os blocos de inode
	for (int i = 1; i <= ninodeblocks; i++) {
		for (int j = 0; j < INODES_PER_BLOCK; j++) {
			block.inode[j].isvalid = 0;    // ajusta o inode como inválido
			block.inode[j].size = 0;    // ajusta o tamanho do inode como 0
			for (int k = 0; k < POINTERS_PER_INODE; k++) {
				block.inode[j].direct[k] = 0;   // ajusta os ponteiros diretos como 0
			}
			block.inode[j].indirect = 0;    // ajusta o ponteiro indireto como 0
		}
		disk->write(i, block.data); // escreve os blocos de inode formatados
	}

    // loop que formata os blocos de dados
	for (int i = ninodeblocks + 1; i < nblocks; i++) {
		for (int j = 0; j < Disk::DISK_BLOCK_SIZE; j++) {
			block.data[j] = 0;  // zera o bloco de dados
		}
		disk->write(i, block.data); // escreve o bloco de dados formatado
	}
	return 1;
}

void INE5412_FS::fs_debug()
{
	union fs_block block;

	disk->read(0, block.data);

    // imprime os dados do superbloco
	cout << "superblock:\n";
	cout << "    " << (block.super.magic == FS_MAGIC ? "magic number is valid\n" : "magic number is invalid!\n");
 	cout << "    " << block.super.nblocks << " blocks\n";
	cout << "    " << block.super.ninodeblocks << " inode blocks\n";
	cout << "    " << block.super.ninodes << " inodes\n";

    // loop que imprime os dados dos inodes
    for(int i = 1; i <= block.super.ninodeblocks; i++) {
        disk->read(i, block.data);  // le o bloco de inode

        // loop que imprime os dados dos inodes
        for(int j = 0; j < INODES_PER_BLOCK; j++) {
            // se o inode for válido, imprime os dados do inode
            if(block.inode[j].isvalid) {
				cout << "inode " << (i-1)*INODES_PER_BLOCK+j+1 << ":\n"; // indice do inode
                cout << "    size: " << block.inode[j].size << " bytes\n";  // tamanho do inode
                cout << "    direct blocks: ";  // blocos diretos

                // loop que imprime os blocos diretos do inode
                for(int k = 0; k < POINTERS_PER_INODE; k++) {
                    // se o bloco direto for diferente de 0, imprime o bloco
                    if(block.inode[j].direct[k]) {
						cout << block.inode[j].direct[k] << " ";
					}
                }
                cout << "\n";

                // se o bloco indireto for diferente de 0, imprime o bloco indireto
                if(block.inode[j].indirect){
					cout << "    indirect block: " << block.inode[j].indirect << "\n";
					cout << "    indirect data blocks: ";
                    union fs_block ind_block;   // bloco indireto

                    disk->read(block.inode[j].indirect,ind_block.data); // le o bloco indireto

                    // loop que imprime os blocos de dados indiretos
                    for(int k = 0; k < POINTERS_PER_BLOCK; k++) {
                        // se bloco de dados indireto for != 0, imprime o bloco
                        if(ind_block.pointers[k])
						{
							cout << ind_block.pointers[k]<< " " ;   // imprime o indice do bloco
						} 
                    }
                    cout << "\n";
                }
            }
        }    
    }
}

// função auxiliar que retorna o bitmap de blocos livres
void print_bitmap(const std::vector<int>& fblocks_bitmap) {
    cout << "Bitmap: ";
    // loop que imprime o bitmap de blocos livres
    for (std::size_t i = 0; i < fblocks_bitmap.size(); ++i) {
        cout << fblocks_bitmap[i] << " ";
    }
    cout << endl;
}

int INE5412_FS::fs_mount()
{

    union fs_block block;
    disk->read(0, block.data);  // le o superblock
    
    fs_superblock superblock = block.super;
    // verifica se o magic number é válido
    if (superblock.magic != FS_MAGIC) {
        cerr << "ERROR: Invalid magic number!" << endl;
        return 0;
    }

    fblocks_bitmap.clear(); // limpa o bitmap de blocos livres
    fblocks_bitmap.resize(superblock.nblocks, 0);   // ajusta o tamanho do bitmap de blocos livres para o tamanho do n de blocos do superbloco

    fblocks_bitmap[0] = 1;  // bota o bloco 0 como ocupado

    // loop que preenche o bitmap de blocos livres
    for (int a = 1; a <= superblock.ninodeblocks; a++) {

        disk->read(a, block.data);  // le o bloco de inode

        // para cada inode no bloco de inode
        for (int b = 0; b < INODES_PER_BLOCK; b++) {
            //  se o inode for válido
            if (block.inode[b].isvalid) {
                fblocks_bitmap[a] = 1;  // bota o bloco de inode como ocupado

                // pra cada bloco direto do inode
                for (int c = 0; c < POINTERS_PER_INODE; c++) {
                    // se o bloco direto for diferente de 0, bota o bloco direto como ocupado
                    if (block.inode[b].direct[c]) { 
                        fblocks_bitmap[int(block.inode[b].direct[c])] = 1;
                    }
                }
                // se o bloco indireto!=0, bota o bloco indireto como ocupado
                if (block.inode[b].indirect) {
                    fblocks_bitmap[int(block.inode[b].indirect)] = 1;

                    union fs_block ind_block;
                    disk->read(block.inode[b].indirect, ind_block.data);    // le o bloco indireto

                    // pra cada bloco de dados indireto, bota o bloco de dados indireto como ocupado
                    for (int d = 0; d < POINTERS_PER_BLOCK; d++) {
                        fblocks_bitmap[int(ind_block.pointers[d])] = 1;
                    }
                }
            }
        }    
    }     
    print_bitmap(fblocks_bitmap);   // imprime o bitmap de blocos livres que foi montado no shell
    mounted = true; // seta o disco como montado
    return 1;
}

// função auxiliar que carrega o inode
void INE5412_FS::inode_load( int inumber, class fs_inode *inode )
{
    // se o inumber for inválido, retorna erro
    if (inumber < 1 || inumber > superblock.ninodes) {
        cerr << "ERROR: Invalid inumber" << endl;
        inode->isvalid = 0;
        return;
    }

    int block_number = 1 + (inumber - 1) / INODES_PER_BLOCK;    // calcula o número do bloco de inode
    int inode_index = (inumber - 1) % INODES_PER_BLOCK; // calcula o índice do inode no bloco de inode
    
    union fs_block block;
    disk->read(block_number, block.data);   // le o bloco de inode usando o block_number calculado
    *inode = block.inode[inode_index];  // carrega o inode
}

// função auxiliar que salva o inode
void INE5412_FS::inode_save( int inumber, class fs_inode *inode ) 
{   
    // se o inumber for inválido, retorna erro
    if (inumber < 1 || inumber > superblock.ninodes) {
        cerr << "ERROR: Invalid inumber" << endl;
        inode->isvalid = 0;
        return;
    }

    int block_number = 1 + (inumber - 1) / INODES_PER_BLOCK; // calcula o número do bloco de inode
    int inode_index = (inumber - 1) % INODES_PER_BLOCK; // calcula o índice do inode no bloco de inode

    union fs_block block;
    disk->read(block_number, block.data);   // le o bloco de inode usando o block_number calculado
    block.inode[inode_index] = *inode;  // salva o inode no bloco de inode

    disk->write(block_number, block.data);  // escreve o bloco de inode
}

int INE5412_FS::fs_create()
{
    //checa se está montado
    if (!mounted) {
        cerr << "ERROR: Disk is not mounted" << endl;
        return 0;
    }

    union fs_block block;
    disk->read(0, block.data);  // le o superbloco

    fs_inode inode;

    int inumber;

    // loop que procura um inode livre
    for (inumber = 1; inumber <= block.super.ninodes; inumber++) {
        inode_load(inumber, &inode);  // carrega o inode
        
        // se o inode for inválido, cria o inode
        if (!inode.isvalid) {
            inode.isvalid = 1;  // seta o inode como válido
            inode.size = 0; // seta o tamanho do inode como 0

            //  seta os ponteiros diretos do inode como 0
            for (int i = 0; i < POINTERS_PER_INODE; i++) {
                inode.direct[i] = 0;
            }
            inode.indirect = 0; // seta o ponteiro indireto como 0

            inode_save(inumber, &inode);    // salva o inod criado
            fs_mount(); // remonta o disco 
            return inumber; // retorna o inumber do inode criado
        }

    }

    //  se não achar um inode livre dentro do n de inodes, retorna erro
    cerr << "ERROR: inode table is full" << endl;
	return 0;
}

int INE5412_FS::fs_delete(int inumber)
{
    // verifica se está montado
    if (!mounted) {
        cerr << "ERROR: Disk is not mounted" << endl;
        return 0;
    }

    fs_inode inode;
    inode_load(inumber, &inode); // carrega o inode pelo inumber

    // se o inumber for inválido, retorna erro
    if (!inode.isvalid) {
        cerr << "ERROR: Invalid inumber" << endl;
        return 0;
    }

    inode.isvalid = 0;  // bora o inode como inválido
    inode.size = 0; // bota o tamanho do inode como 0

    // bota os ponteiros diretos do inode como 0
    for (int i = 0; i < POINTERS_PER_INODE; i++) {
        if (inode.direct[i]) {
            inode.direct[i] = 0;
            //fblocks_bitmap[inode.direct[i]] = 0;
        }
    }
    
    inode.indirect = 0; // bota o ponteiro indireto como 0

    inode_save(inumber, &inode);    // salva o inode

    fs_mount(); // remonta o disco
	return 1;
}

int INE5412_FS::fs_getsize(int inumber)
{
    // verifica se está montado
    if (!mounted) {
        cerr << "ERROR: Disk is not mounted" << endl;
        return 0;
    }

    fs_inode inode;
    inode_load(inumber, &inode);    //carrega o inode pelo inumber

    // se inode for válido, retorna o tamanho do inode
    if (inode.isvalid) {
        return inode.size;
    }

    //se o inumber for inválido, retorna erro
    cerr << "ERROR: Invalid inumber" << endl;
	return -1;
}

int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
    // verifica se está montado
    if (!mounted) {
        cerr << "ERROR: Disk is not mounted" << endl;
        return 0;
    }

    fs_inode inode;

    // carrega o inode correspondente ao inumber
    inode_load(inumber, &inode);

    // se o inumber for inválido, retorna erro
    if(!inode.isvalid) {
        cerr << "ERROR: Invalid inumber" << endl;
        return 0;
    }

    int inode_size = fs_getsize(inumber);

    // se o offset for maior ao tamanho do inode, retorna erro
    if (offset > inode_size) {
        cerr << "ERROR: offset is greater than inode size" << endl;
        return 0;
    }

    // se o tamanho do inode for 0, retorna erro
    if (inode_size == 0) {
        cerr << "ERROR: inode size is 0" << endl;
        return 0;
    }


    int length_to_read = min(length, inode_size - offset);  // calcula o tamanho de bytes a serem lidos

    int bytes_read = 0; // contador de bytes lidos

    // loop para ler os dados
    while (bytes_read < length_to_read) {

        int curr_offset = offset + bytes_read;  // offset atual
        int block_i = curr_offset / Disk::DISK_BLOCK_SIZE;  // índice do bloco
        int block_offset = curr_offset % Disk::DISK_BLOCK_SIZE; // offset do bloco

        int block_num = get_dblocknum(inode, block_i);  // armazena o indice do bloco a ser lido

        int bytes_to_read = min(length_to_read - bytes_read, Disk::DISK_BLOCK_SIZE - block_offset); // calcula o tamanho de bytes a serem lidos

        // se o bloco for diferente de 0, lê o bloco
        if (block_num != 0) {
            union fs_block block;
            disk->read(block_num, block.data);  // lê o bloco

            memcpy(data + bytes_read, block.data + block_offset, bytes_to_read);    // copia os dados para o buffer
        }
        else {
            memset(data + bytes_read, 0, bytes_to_read);  
        }

        bytes_read += bytes_to_read;    // incrementa o contador de bytes lidos 
        cout << "interation: " << bytes_read << endl;

        cout<< "block_i: " << block_i << endl;
    }
    return bytes_read;  // retorna a quantidade de bytes lidos
}

int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
    fs_inode inode;
	return 0;
}

// função auxiliar que retorna o indice do bloco de dados
int INE5412_FS::get_dblocknum(fs_inode &inode, int block_i) {
    int block_num;  // indice do bloco de dados

    // se o indice do bloco for menor que o n de ponteiros diretos por inode
    if (block_i < POINTERS_PER_INODE) {
        block_num = inode.direct[block_i];  // armazena o indice do bloco de dados conforme o indice do bloco
        return block_num;
    } 

    // se o bloco indireto for 0, retorna erro
    if (inode.indirect == 0) {
        cerr << "ERROR: Indirect block is not allocated" << endl;
        return 0;
    }

    union fs_block ind_block;

    disk->read(inode.indirect, ind_block.data); // le o bloco indireto

    int indblock_i = block_i - POINTERS_PER_INODE;  // calcula o indice do bloco de dados indireto

    // se o indblock_i for maior ou igual ao n de ponteiros por bloco, retorna erro
    if (indblock_i >= POINTERS_PER_BLOCK) {
        cerr << "ERROR: Block index is out of range" << endl;
        return 0;
    }

    block_num = ind_block.pointers[indblock_i]; // armazena o indice do bloco de dados
    return block_num;
} 