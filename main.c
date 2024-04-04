#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>

#ifdef DEBUG
#define PRINT(str, var); printf("%s: %d\n", str, var);
#define PRINTS(str); printf("%s\n", str);
#define PRINTX(str, var); printf("%s: 0x%x\n", str, var);

#else
#define PRINT(str, var); ;
#define PRINTS(str); ;
#define PRINTX(str, var); ;

#endif

typedef struct _boot_sector{
	uint8_t jump[3];
	uint8_t sys_id[8];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t zeros1[3];
	uint16_t not_used1;
	uint8_t md;
	uint16_t zeros2;
	uint16_t sec_per_track;
	uint16_t num_of_heads;
	uint32_t hidden_sectors;
	uint64_t not_used2;
	uint64_t total_sectors;
	uint64_t mft_adr_in_clusters;
	uint64_t mftmirr_adr;
	uint32_t cluster_per_file;
	uint8_t cluster_per_index;
	uint8_t not_used3[3];
	uint64_t volume_num;
	uint32_t checksum;
}__attribute__((packed)) boot_sector;

typedef struct _record_structure{
	uint32_t sig;
	uint16_t marker_offset;
	uint16_t size_of_markers;
	uint64_t LSN;
	uint16_t seq_number;
	uint16_t hard_link_count;
	uint16_t first_atr_offset;
	uint16_t flags;
	uint32_t real_size_mft;
	uint32_t alloc_size_mft;
	uint64_t base_rec_mft;
	uint16_t next_atr_id;
	uint16_t reserved;
	uint32_t num_of_mft;
}__attribute__((packed)) record_structure; 

typedef struct _resident_attr_header{
	uint32_t type_attr_id;
	uint32_t attr_lenth;
	uint8_t resident_flag;
	uint8_t name_lenth;
	uint16_t name_offset;
	uint16_t flags1; 
	uint16_t attr_id;
	uint32_t data_lenth;
	uint16_t data_offset;
	uint8_t flags2;
	uint8_t reserved1;
}__attribute__((packed)) resident_attr_header; 

typedef struct _non_resident_attr_header{
	uint32_t attr_type;
	uint32_t attr_lenth;
	uint8_t non_resident_flag;
	uint8_t name_lenth;
	uint16_t name_offset;
	uint16_t flags;
	uint16_t attr_id;
	uint64_t starting_VCN;
	uint64_t last_VCN;
	uint16_t data_runs_offset;
	uint16_t compression_unit_zise;
	uint32_t padding;
	uint64_t allocated_size_of_attr;
	uint64_t real_size_of_attr;
	uint64_t init_data_size;
}__attribute__((packed)) non_resident_attr_header;

typedef struct _index_record{
	uint32_t magic_number;
	uint16_t offset_to_sequense;
	uint16_t sequense_size;
	uint64_t sequense_number;
	uint64_t vcn_of_index_record;
	uint32_t first_index_offset;
	uint32_t total_size_of_index;
	uint32_t alloc_size_of_node;
	uint8_t node_flag;
	uint8_t padding[3];
	uint16_t sequense;
}__attribute__((packed)) index_record;

typedef struct _index_entry_rec{
	//uint8_t file_rec_num[6];
	//uint16_t seq_num;
	uint64_t file_ref;
	uint16_t index_lenth;
	uint16_t stream_lenth;
	uint8_t flag;
}__attribute__((packed)) index_entry_rec;


int name_comparison(int fd_img, int argument_lenth, uint16_t* argument, int name_address){
	uint8_t filename_lenth;
	
	lseek(fd_img, name_address + 0x40, SEEK_SET);
	read(fd_img, &filename_lenth, 1);

	if(filename_lenth != argument_lenth){
		return -1;
	}

	uint16_t* name = (uint16_t*) malloc(filename_lenth);
	lseek(fd_img, name_address + 0x42, SEEK_SET);
	for(int i = 0; i < filename_lenth; i++){
		read(fd_img, &name[i], 2);
	}

#ifdef DEBUG
	printf("filename: ");
	for(int i = 0; i < filename_lenth; i++){
		printf("%c", name[i]);
	}
	printf("\n");
#endif 

	if(memcmp(name, argument, argument_lenth) != 0){
		return -1; 
	} else {
		return 0; 
	}

	free(name);
}


int find_attribute(uint8_t id, int address, int fd_img){
	while(1){
		lseek(fd_img, address, SEEK_SET);

		uint32_t type; 
		read(fd_img, &type, 4);
		PRINTX("Type of attribute", type);
	
		uint32_t attr_size;
		read(fd_img, &attr_size, 4);
		PRINT("Size of attribute", attr_size);

		if(type == 0xFFFFFFFF){
			PRINT("End of attributes at", address);
			return -1; 
		}

		if(type == id){
			PRINTX("Attribute finded at", address);
			return address;
		}
	
		if(type == 0){
			PRINTS("Zero attribute!");
			break;
		}	

		address += attr_size; 
	}
}

int find_data_runs(int fd_img, int data_runs_address, int *lenth, int *offset, int *next_data_run){
	lseek(fd_img, data_runs_address + *next_data_run, SEEK_SET);
	uint8_t data_run_header;
	read(fd_img, &data_run_header, 1);

	if(data_run_header == 0){
		PRINTS("The header of data run is zero! End data runs!");
		return -1;
	}

	uint8_t offset_size = data_run_header >> 4;
	PRINTX("offset size", offset_size);

	uint8_t lenth_size = data_run_header && 0x0F;
	PRINTX("lenth size", lenth_size);

	read(fd_img, &(*lenth), lenth_size);
	read(fd_img, &(*offset), offset_size);

	*next_data_run += (offset_size + lenth_size);
}

int func3(int fd_img, uint16_t* argument, int argument_lenth, int next_index){
	index_entry_rec index_entry = {0};

	while(1){
		lseek(fd_img, next_index, SEEK_SET);
		if(read(fd_img, &index_entry, sizeof(index_entry_rec)) != sizeof(index_entry_rec)){
			perror("Reading index entry header");
			return __LINE__;
		}

		if(index_entry.index_lenth == 0){
			printf("File not found...\n");
			return -1;
		}

		if(name_comparison(fd_img, argument_lenth, argument, next_index + 16) != 0){
			next_index += index_entry.index_lenth;
			continue;
		} else {
			lseek(fd_img, next_index, SEEK_SET);
			uint64_t file_record_number;
			read(fd_img, &file_record_number, 6);
			PRINTX("file record number", file_record_number);
			return file_record_number;
		}
	}

}

int func(int fd_img, int cluster, int index_alloc_attribute, uint16_t* argument, int argument_lenth){
	non_resident_attr_header header = {0};
	if(read(fd_img, &header, sizeof(non_resident_attr_header)) != sizeof(non_resident_attr_header)){
		perror("Reading attribute header");
		return __LINE__;
	}
	PRINTX("Offset to data runs", header.data_runs_offset);

	int next_data_run = 0;  
	index_entry_rec index_entry = {0};
	resident_attr_header res_header = {0};
	index_record index_rec = {0};

	while(1){	
		int lenth, offset, next_index;
		if(find_data_runs(fd_img, index_alloc_attribute + header.data_runs_offset + next_data_run, &lenth, &offset, &next_data_run) == -1){
			return -1;
		}
		PRINT("Next data run", next_data_run);

		while(1){	
			next_index = cluster * offset;							
			lseek(fd_img, next_index, SEEK_SET);
			PRINTX("Indx", cluster * offset );
			
			if(read(fd_img, &index_rec, sizeof(index_rec)) != sizeof(index_rec)){
				perror("Reading INDX header");
				return __LINE__;
			}
			next_index += (index_rec.first_index_offset + 0x18);	//0x18 - offset of index node header 
			PRINTX("First index entry", next_index);

			uint64_t file_record_number = func3(fd_img, argument, argument_lenth, next_index); 
			if(file_record_number == -1){
				break;
			} else {
				return file_record_number;
			}
		}
	}
}

int data_download(int fd_img, int offset){
	resident_attr_header header = {0};
	record_structure record = {0};
	lseek(fd_img, offset, SEEK_SET);
	if(read(fd_img, &record, sizeof(record_structure)) != sizeof(record_structure)){
		perror("reading header");
		exit(1);
	}

	uint64_t data_attribute_address = find_attribute(0x80, offset + record.first_atr_offset, fd_img);
	if (data_attribute_address == -1){
		PRINTS("atr 0x80 not found");
		return -1; 
	}
	 
	lseek(fd_img, data_attribute_address, SEEK_SET);

	if(read(fd_img, &header, sizeof(resident_attr_header)) != sizeof(resident_attr_header)){
		perror("reading header");
		exit(1);
	}
	PRINTX("data_attribute_address", data_attribute_address);
	
	uint8_t* buffer = (uint8_t*)malloc(header.data_lenth);
	read(fd_img, buffer, header.data_lenth);

	lseek(fd_img, offset + record.marker_offset, SEEK_SET);
	uint8_t* marker = (uint8_t*)malloc(record.size_of_markers * 2); 
	read(fd_img, marker, record.size_of_markers * 2);


	PRINTX("offset + marker offset", data_attribute_address + header.data_offset);

	uint64_t index = (offset + 510) - (data_attribute_address + header.data_offset);
	PRINT("indexx", index);

	buffer[index] = marker[2];
	buffer[index + 1] = marker[3];

	int fd_out;
	if((fd_out = open("file.bin", O_RDWR | O_CREAT, 0600)) == -1){
		perror("fd_out opening");
		exit(1);
	}
	if(write(fd_out, buffer, header.data_lenth) == -1){
		perror("Writing to buffer");
		exit(1);
	}

	close(fd_out);
	free(buffer);
	free(marker);
}

int main(int argc, char *argv[]){
	if (argc != 3){
		printf("Usage: 'disk image' 'filename'\n");
		exit(1);
	}

	int fd_img;
	if((fd_img = open(argv[1], O_RDONLY)) == -1){
		perror("fd_img opening");
		return __LINE__;
	}

	boot_sector bs_img = {0};
	non_resident_attr_header header = {0};
	record_structure root_record = {0};

	int argument_lenth = strlen(argv[2]);
	uint16_t* argument = (uint16_t*) malloc(argument_lenth);
	for(int i = 0; i < argument_lenth; i++){
			argument[i] = argv[2][i];
	}

	if(read(fd_img, &bs_img, sizeof(bs_img)) != sizeof(boot_sector)){
		perror("Reading boot");
		return __LINE__;
	}
	PRINT("Byes per sector", bs_img.bytes_per_sector);
	PRINT("Sectors per cluster", bs_img.sectors_per_cluster);
	PRINT("MFT address in clusters", (int)bs_img.mft_adr_in_clusters);

	int cluster = bs_img.sectors_per_cluster * bs_img.bytes_per_sector;;
	int mft_adr = bs_img.mft_adr_in_clusters * cluster;
	PRINT("MFT address", mft_adr);
	
	int root_address = mft_adr + 1024 * 5;
	lseek(fd_img, root_address, SEEK_SET);

	if(read(fd_img, &root_record, sizeof(record_structure)) != sizeof(record_structure)){
		perror("Reading root record");
		return __LINE__;
	}	

	int index_alloc_attribute = find_attribute(0xA0, root_address + root_record.first_atr_offset, fd_img);
	lseek(fd_img, index_alloc_attribute, SEEK_SET);

	PRINTS("------------------------");	
	int sequense = func(fd_img, cluster, index_alloc_attribute, argument, argument_lenth);
	if(sequense == -1){
		exit(1);
	}
	PRINTX("sequense", mft_adr + sequense * 1024);

	data_download(fd_img, mft_adr + sequense * 1024);

	return 0;
}
