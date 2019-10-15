#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sys/time.h>

#define MAXDIM 225

using namespace std;

int array[20][100];

typedef struct data {
	int row;
	int col;
	int currentRow;
	int currentCol;
	int target;
	int count = 0;
}data;

//Prints array contents for debugging purposes
void print_array(struct data* data1) {
	for(int i=0; i<data1->row; i++) {
		for(int j=0; j<data1->col; j++) {
			cout << array[i][j] << " ";
		}
		cout << "\n";
	}
}

//Inserts file contents into the global array, populates key values
//	of the data structure
struct data* File_to_Array(char* filename) {
	ifstream file(filename, ios::in);
	
	if(!file.is_open()) {
		cout << "\nUnable to open file " << filename << "\n";
		return NULL;
	}
	int h;
	int w;
	file >> h;
	file >> w;
	
	struct data* data1 = new struct data;
	data1->row = h;
	data1->col = w;
	data1->count = 0;
	
	for(int i=0; i<h; i++) {
		for(int j=0; j<w; j++) {
			file >> array[i][j];
		}
	}
	return data1;	
}

//Copies passed data structure into elements of a passed array
void Copy_Struct_Into_Array(struct data* str, struct data* array[MAXDIM]) {
	for(int i=0; i<MAXDIM; i++) {
		array[i] = new struct data;
		array[i]->row		= str->row;
		array[i]->col		= str->col;
		array[i]->currentRow= str->currentRow;
		array[i]->currentCol= str->currentCol;
		array[i]->target	= str->target;
		array[i]->count		= 0;
	}
}

//Searches entire matrix in one thread
void *Search_Entire_Matrix(void *ptr) {
	//struct data* data1 = Copy_Struct((struct data *)ptr);
	struct data* data1 = (struct data *)ptr;
	data1->count = 0;
			
	for(int i=0; i<data1->row; i++) {
		for(int j=0; j<data1->col; j++) {
			if(array[i][j] == data1->target) {
				data1->count++;
			}
		}
	}
	pthread_exit(0);
}

//Searches only one row of a matrix in one thread
void *Search_Row(void *ptr) {
	struct data* data1 = (struct data *)ptr;	
	
	for(int j=0; j<data1->col; j++) {
		if(array[data1->currentRow][j] == data1->target) {
			data1->count++;
		}
	}
	
	pthread_exit(0);
}

//Searches one column of a matrix in one thread
void *Search_Col(void *ptr) {
	struct data* data1 = (struct data *)ptr;	
	
	for(int j=0; j<data1->row; j++) {
		if(array[j][data1->currentCol] == data1->target) {
			data1->count++;
		}
	}
	
	pthread_exit(0);
}

//Searches one element of a matrix in one thread
void *Search_Elem(void *ptr) {
	struct data* data1 = (struct data *)ptr;	
	
	if(array[data1->currentRow][data1->currentCol] == data1->target) {
		data1->count++;
	}	
	pthread_exit(0);
}

//Collects search and timing results for different search cases
void *Go_File(char* filename, int target) {
	struct data* file1 = File_to_Array(filename);
	if(file1 == NULL)
		return NULL;
	file1->target = target;
	
	pthread_t thread[MAXDIM];
	struct data* arrayofstructs[MAXDIM];
	Copy_Struct_Into_Array(file1,arrayofstructs);
	timeval b4,af;
	int total;
	
	//---------- WHOLE MATRIX
	//Start time
	gettimeofday(&b4, 0);
	
	for(int i=0; i<5; i++) {
		pthread_create(&thread[i], NULL, Search_Entire_Matrix, (void *)arrayofstructs[i]);
	}	
	
	//End time
	for(int i=0; i<5; i++) {
		pthread_join(thread[i], NULL);
	}		
	gettimeofday(&af, 0);
	
	int avgtimediff = ((af.tv_sec * 1000000 + af.tv_usec) - (b4.tv_sec * 1000000 + b4.tv_usec)) / 5;
	cout << "\n\n------------ WHOLE MATRIX\nMatches: " << arrayofstructs[0]->count << "\nAverage time to complete: " << avgtimediff << " microseconds.\n\n";
	

	//---------- ROW
	//Start time		
	gettimeofday(&b4, 0);
	
	for(int i=0; i<5; i++) {
		total = 0;
		
		for(int j=0; j<file1->row; j++) {
			arrayofstructs[j]->currentRow = j;
			arrayofstructs[j]->count = 0;
			pthread_create(&thread[j], NULL, Search_Row, (void *)arrayofstructs[j]);
			
		}
		
		//Join threads
		for(int k=0; k<file1->row; k++) {
			pthread_join(thread[k], NULL);
			total += arrayofstructs[k]->count;
		}
	}		
		
	//End time	
	gettimeofday(&af, 0);
	
	avgtimediff = ((af.tv_sec * 1000000 + af.tv_usec) - (b4.tv_sec * 1000000 + b4.tv_usec)) / 5;
	cout << "\n\n------------ ROW\nMatches: " << total << "\nAverage time to complete: " << avgtimediff << " microseconds.\n\n";
	
	
	//---------- COLUMN
	//Start time		
	gettimeofday(&b4, 0);
	
	for(int i=0; i<5; i++) {
		total = 0;
		
		for(int j=0; j<file1->col; j++) {
			arrayofstructs[j]->currentCol = j;
			arrayofstructs[j]->count = 0;
			pthread_create(&thread[j], NULL, Search_Col, (void *)arrayofstructs[j]);
			
		}
		
		//Join threads
		for(int k=0; k<file1->col; k++) {
			pthread_join(thread[k], NULL);
			total += arrayofstructs[k]->count;
		}
	}		
		
	//End time	
	gettimeofday(&af, 0);
	
	avgtimediff = ((af.tv_sec * 1000000 + af.tv_usec) - (b4.tv_sec * 1000000 + b4.tv_usec)) / 5;
	cout << "\n\n------------ COLUMN\nMatches: " << total << "\nAverage time to complete: " << avgtimediff << " microseconds.\n\n";
	
	
	//---------- ELEMENT
	//Start time		
	gettimeofday(&b4, 0);
	
	for(int i=0; i<5; i++) {
		int index = 0;
		total = 0;
		
		for(int j=0; j<file1->row; j++) {
			for(int m=0; m<file1->col; m++) {
				arrayofstructs[index]->currentRow = j;
				arrayofstructs[index]->currentCol = m;
				arrayofstructs[index]->count = 0;
				pthread_create(&thread[index], NULL, Search_Elem, (void *)arrayofstructs[index]);
				index++;
			}
		}
		
		index = 0;
		//Join threads
		for(int k=0; k<file1->row; k++) {
			for(int n=0; n<file1->col; n++) {
				pthread_join(thread[index], NULL);
				total += arrayofstructs[index]->count;
				index++;
			}
		}
	}		
		
	//End time	
	gettimeofday(&af, 0);
	
	avgtimediff = ((af.tv_sec * 1000000 + af.tv_usec) - (b4.tv_sec * 1000000 + b4.tv_usec)) / 5;
	cout << "\n\n------------ ELEMENT\nMatches: " << total << "\nAverage time to complete: " << avgtimediff << " microseconds.\n\n";
		
		
	for(int l=0; l<MAXDIM; l++) {
		delete arrayofstructs[l];
	}
}


int main(void) {
	char* f1 = (char*)"2x100.txt";
	char* f2 = (char*)"15x15.txt";
	char* f3 = (char*)"20x10.txt";
	
	cout << "\nEnter number to search for: ";
	int target;
	cin >> target;
	
	cout << "\nStarting search...\n";
	cout << "\n\n---------------------------------- " << f1 << " ----------------------------------\n\n";
	Go_File(f1,target);
	cout << "\n\n---------------------------------- " << f2 << " ----------------------------------\n\n";
	Go_File(f2,target);
	cout << "\n\n---------------------------------- " << f3 << " ----------------------------------\n\n";
	Go_File(f3,target);
	
	return(0);
}
