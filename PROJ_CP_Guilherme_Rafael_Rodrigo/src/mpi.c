#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>

#pragma pack(1)

// UFSCar Sorocaba
// Parallel Computing
// Final Project
// Rodrigo Barbieri, Rafael Machado and Guilherme Baldo
// MPI version

/* 
 * Grupo:
 * 000000 - Guilherme Baldo
 * 000000 - Rafael Machado
 * 000000 - Rodrigo Barbieri
*/

typedef struct ProgramInfo { //Structure responsible for maintaining program information and state
	unsigned int height; //image height
	unsigned int width; //image width
	int p; //number of threads selected by the user
	unsigned int header_size; //header size
	char decode; //whether the user chose to print the sorted array
	int padding;
	int repeat;
	char* outputFilename;
} ProgramInfo;

ProgramInfo *p_info;

typedef struct BMP_HEADER {
	short signature;
	long size;
	short reserved1;
	short reserved2;
	long offset_start;
	long header_size;
	long width;
	long height;
	short planes;
	short bits;
	long compression;
	long size_data;
	long hppm;
	long vppm;
	long colors;
	long important_colors;
} BMP_HEADER;

void initialize_header(BMP_HEADER *header){
	header->signature = 0;
	header->size = 0;
	header->reserved1 = 0;
	header->reserved2 = 0;
	header->offset_start = 0;
	header->header_size = 0;
	header->width = 0;
	header->height = 0;
	header->planes = 0;
	header->bits = 0;
	header->compression = 0;
	header->size_data = 0;
	header->hppm = 0;
	header->vppm = 0;
	header->colors = 0;
	header->important_colors = 0;
}

void print_header(BMP_HEADER *header){
	printf("signature: %hd,\nsize: %ld,\nreserved1: %hd,\nreserved2: %hd,\noffset_start: %ld,\nheader_size: %ld,\nwidth: %ld,\nheight: %ld,\nplanes: %hd,\nbits: %hd,\ncompression: %ld,\nsize_data: %ld,\nhppm: %ld,\nvppm: %ld,\ncolors: %ld,\nimportant_colors: %ld\n",header->signature,header->size,header->reserved1,header->reserved2,header->offset_start,header->header_size,header->width,header->height,header->planes,header->bits,header->compression,header->size_data,header->hppm,header->vppm,header->colors,header->important_colors);
}

void writeToFile(char* message, unsigned int* size,char* filename){

	FILE* output = NULL;

	while(output == NULL){
		output = fopen(filename, "ab");
	}
	
	if(NULL != output){
		fwrite(message, sizeof(char), *size, output);
		fflush(output);
		fclose(output);
	} else {
		printf("Could not write to file for some reason\n");
	}
}

FILE* validation(int* argc, char* argv[]){ //validates several conditions before effectively starting the program

	FILE* f = NULL;
	
	if (*argc != 5)
	{ //validates number of arguments passed to executable, currently number of threads and file name
		printf("Usage: %s <file name> <decode? Y/N> <Times to repeat> <output filename>\n",argv[0]);
		fflush(stdout);
	} 
	else 
	{
		p_info->repeat = strtol(argv[3],NULL,10);

		p_info->decode = *argv[2];

		f = fopen(argv[1],"rb");

		if (f == NULL)
		{ //check if the file inputted exists
			printf("File not found!");
			fflush(stdout);
		}
	}

	return f;
}

void decode (char* message, unsigned int encodedWidth, unsigned int width, char* output){
	unsigned int i = 0,j = 0;
	unsigned int count = 0;
	char* color = NULL;
	int outputIndex = 0;
		
	while (i < encodedWidth){
		count = (unsigned int) message[i] & 0x000000FF;

		color = &message[i+1];	

		while (j < count){
			output[outputIndex] = color[0];
			output[++outputIndex] = color[1];
			output[++outputIndex] = color[2];	
			j++;
			outputIndex++;
		}
		j = 0;
		i += 4;		
	}
}

void encode (char* message, unsigned int width, char* output, unsigned int* encodedSize){
	unsigned int i = 0;
	unsigned int count = 1;
	unsigned int outputIndex = 0;
	
	while (i < width){

		if (message[i] == message[i+3] && 
			message[i+1] == message[i+4] && 
			message[i+2] == message[i+5] && 
			count < 255 && (i + 3 < width)) {
			count++;
		}
		else {
			output[outputIndex] = (char) count & 0x000000FF;
			output[++outputIndex] = message[i];
			output[++outputIndex] = message[i+1];
			output[++outputIndex] = message[i+2];
			outputIndex++;
			count = 1;
		}
		i += 3;
	}

	*encodedSize = outputIndex;
}

void calculateLocalArray(unsigned int* local_n,unsigned int* my_first_i, int* rank){ //calculates local number of elements and starting index for a specific rank based on total number of elements
	unsigned int div = p_info->height / p_info->p;
	int r = p_info->height % p_info->p; //divides evenly between all threads, firstmost threads get more elements if remainder is more than zero
	if (*rank < r){
		*local_n = div + 1;
		if (my_first_i != NULL) //allows my_first_i parameter to be NULL instead of an address
			*my_first_i = *rank * *local_n;
	} else {
		*local_n = div;
		if (my_first_i != NULL) //allows my_first_i parameter to be NULL instead of an address
			*my_first_i = *rank * *local_n + r;
	}
}

void manageProcessesReadingFile(char* bytes,char* filename, unsigned int* local_n, unsigned int size, unsigned int* my_first_i, int* rank)
{
	int count = 0, i;
	FILE* input = NULL;
	while (count != *rank)
		MPI_Bcast(&count,1,MPI_INT,count,MPI_COMM_WORLD);

	input = fopen(filename,"rb");

	if (input != NULL){

		fseek(input,p_info->header_size + (*my_first_i * size),SEEK_SET);
		for (i = 0; i < *local_n ; i++){	
			fread(bytes,sizeof(char), size, input);
			bytes += size;
		}
		fclose(input);
	} else
		printf("Rank: %d, Could not open file for reading\n",*rank);

	count++;
	MPI_Bcast(&count,1,MPI_INT,(int)*rank,MPI_COMM_WORLD);
	while(count < p_info->p)
		MPI_Bcast(&count,1,MPI_INT,count,MPI_COMM_WORLD);
}

void manageProcessesWritingToFile(char* bytes,char* filename, unsigned int* local_n,unsigned int block_size, unsigned int* size_list, int* rank){

	int count = 0, i;
	FILE* output = NULL;

	while (count != *rank)
		MPI_Bcast(&count,1,MPI_INT,count,MPI_COMM_WORLD);

	output = fopen(filename, "ab");

	if (output != NULL){
		for (i = 0; i < *local_n; i++){
			fwrite(bytes, sizeof(char), size_list[i], output);
			fflush(output);
			bytes += block_size;
		}
		fclose(output);
	} else 
		printf("Rank: %d, Could not write to file\n",*rank);

	count++;
	MPI_Bcast(&count,1,MPI_INT,(int)*rank,MPI_COMM_WORLD);
	while (count < p_info->p)
		MPI_Bcast(&count,1,MPI_INT,count,MPI_COMM_WORLD);
}

int main(int argc, char *argv[]){

	FILE *f = NULL;
	int rank,p;
	unsigned int local_n,my_first_i,t;
	double start = 0;
	double end = 0;
	double min = 999;
	double total = 0;
	unsigned int i;
	unsigned int* encodedSize = NULL;
	unsigned int dimensions[4];
	char* imageInBytesHEAD = NULL;
	char* encodedImageHEAD = NULL;
	char* encodedImage = NULL;
	char* imageInBytes = NULL;
	unsigned int originalSize;
	unsigned int originalPlusPadding;
	BMP_HEADER header;

	MPI_Init(NULL,NULL);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &p);

	p_info = (ProgramInfo*) malloc(sizeof(ProgramInfo)); //allocates ProgramInfo structure

	p_info->p = p;

	p_info->outputFilename = argv[4];

	if (rank == 0){

		initialize_header(&header);
		f = validation(&argc,argv);

		if (f != NULL){
			fread(&header,sizeof(BMP_HEADER),1,f);
			fclose(f);
		}
		dimensions[0] = (unsigned int) header.width;
		dimensions[1] = (unsigned int) header.height;
		dimensions[2] = (unsigned int) header.offset_start;
		dimensions[3] = (unsigned int) p_info->repeat;
	}

	if (rank == 0 && (header.reserved1 != 0 || header.reserved2 != 0))
		printf("Your image is either malformed or your compiler is not reading pragma pack!\n");

	MPI_Bcast(dimensions,4,MPI_UNSIGNED,0,MPI_COMM_WORLD);	

	if (dimensions[0] != 0 && dimensions[1] != 0 && dimensions[2] != 0 && dimensions[3] != 0){

		p_info->width = dimensions[0];
		p_info->height = dimensions[1];
		p_info->header_size = dimensions[2];
		p_info->repeat = dimensions[3];
		p_info->padding = (p_info->width * 3) % 4 == 0 ? 0 : (4 - ((p_info->width * 3) % 4));
		originalSize = p_info->width * 3;
		originalPlusPadding = originalSize + p_info->padding;

		calculateLocalArray(&local_n,&my_first_i,&rank);

		if (rank == 0){
			remove(p_info->outputFilename);
			writeToFile((char*) &header,&p_info->header_size,p_info->outputFilename);
		}

		encodedSize = (unsigned int*) malloc (sizeof(unsigned int) * local_n);
		for (i = 0; i < local_n; i++)
			encodedSize[i] = 0;

		imageInBytes = (char *) malloc(originalPlusPadding * local_n);
		encodedImage = (char *) malloc(originalSize * 2 * local_n);

		memset(imageInBytes,'\0',originalPlusPadding * local_n);
		memset(encodedImage,'\0',originalSize * 2 * local_n);

		imageInBytesHEAD = imageInBytes;
		encodedImageHEAD = encodedImage;

		manageProcessesReadingFile(imageInBytes,argv[1],&local_n,originalPlusPadding, &my_first_i, &rank);

		imageInBytes = imageInBytesHEAD;

		MPI_Barrier(MPI_COMM_WORLD);

		for (t = 0 ; t < p_info->repeat; t++){

			encodedImage = encodedImageHEAD;
			imageInBytes = imageInBytesHEAD;

			start = MPI_Wtime();

			for (i = 0; i < local_n; i++){
				encode(imageInBytes,originalSize,encodedImage,&encodedSize[i]);
				imageInBytes += originalPlusPadding;
				encodedImage += originalSize * 2;
			}

			end = MPI_Wtime();

			if (end - start < min)
				min = end - start;
		}		

		MPI_Reduce(&min,&total,1,MPI_DOUBLE,MPI_MAX,0,MPI_COMM_WORLD);

		if (rank == 0)
			printf("MPI: %lf\n",total);

		encodedImage = encodedImageHEAD;

		manageProcessesWritingToFile(encodedImage,p_info->outputFilename,&local_n,originalSize * 2,encodedSize,&rank);

		encodedImage = encodedImageHEAD;
		imageInBytes = imageInBytesHEAD;

		if (rank == 0)
			system("ps -e -L -o cmd,pid,tid,psr,pcpu,pmem | grep mpi | grep -v grep | grep -v mpiexec");
		
		if (encodedImage != NULL)
			free(encodedImage);
		if (imageInBytes != NULL)
			free(imageInBytes);
		if (encodedSize != NULL)
			free(encodedSize);
	}

	if (p_info != NULL)
		free(p_info);

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();

	return 0;
}
