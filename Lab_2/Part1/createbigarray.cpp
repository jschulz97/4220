#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <fstream>
#include <iostream>

using namespace std;

#define row 1000
#define col 500

int mat[row+1][col];

//Prints array contents for debugging purposes
void print_mat_to_file() {
	ofstream file("1000x500.txt", ios::out);
	file << mat[0][0];
	file << " " << mat[0][1] << "\n";
	for(int i=1; i<mat[0][0]+1; i++) {
		for(int j=0; j<mat[0][1]; j++) {
			file << mat[i][j] << " ";
		}
		file << "\n";
	}
}

void generate_mat() {
	mat[0][0] = row;
	mat[0][1] = col;
	for(int i=1; i<row+1; i++) {
		for(int j=0; j<col; j++) {
			mat[i][j] = (int) rand() % 10;
		}
	}
}


int main(void) {
	generate_mat();
	print_mat_to_file();
	
	return 0;
}
