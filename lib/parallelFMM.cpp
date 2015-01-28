#include <mpi.h>
#include <climits> // for HOST_NAME_MAX
#include <iostream>
#include <unistd.h> // for gethostname
#include "treedef.hpp"

using namespace std;
int main(int argc, char** argv)
{
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &numProc);

	MPI_Barrier(MPI_COMM_WORLD);
	char hostname[HOST_NAME_MAX];
	gethostname(hostname, sizeof hostname);
	MPI_Barrier(MPI_COMM_WORLD);

	if(rank == 0){
		LocalOctTree localTree;
		localTree.runControl();
	} else {
		LocalOctTree localTree;
		localTree.run();
	}
	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();

	if(rank == 0){
		cout << "Done." <<endl;
	}
	return 0;
}