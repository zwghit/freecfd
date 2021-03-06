/************************************************************************
	
	Copyright 2007-2010 Emre Sozer

	Contact: emresozer@freecfd.com

	This file is a part of Free CFD

	Free CFD is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    Free CFD is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    For a copy of the GNU General Public License,
    see <http://www.gnu.org/licenses/>.

*************************************************************************/
#include "grid.h"
#include "inputs.h"
#include "bc.h"
#include "bc_interface.h"

extern InputFile input;
extern vector<Grid> grid;
extern vector<vector<BCregion> > bc;
extern vector<vector<BC_Interface> > interface;
extern vector<int> equations;
extern vector<bool> turbulent;

void set_bcs(int gid) {
	
	// Loop through each boundary condition region and apply sequentially
	int count=input.section("grid",gid).subsection("BC",0).count;
	grid[gid].bcCount=count;
	int Rank=grid[gid].Rank;
	int np=grid[gid].np;
	grid[gid].maps.face2bc.resize(grid[gid].faceCount);
	vector<int> bc_counter (count,0);
	
	for (int b=0;b<count;++b) {

		// Store the reference to current BC region
		Subsection &region=input.section("grid",gid).subsection("BC",b);
				
		// Find out pick method
		string pick=region.get_string("pick");
		int pick_from;
		if (pick.substr(0,2)=="BC") {
			pick_from=atoi((pick.substr(2,pick.length())).c_str());
			pick=pick.substr(0,2);
		}

		if (region.get_string("region")=="box") {
			for (int f=0;f<grid[gid].faceCount;++f) {
				// if the face is not already marked as internal or partition boundary
				// And if the face centroid falls within the defined box
				if ((grid[gid].face[f].bc>=0 || grid[gid].face[f].bc==UNASSIGNED_FACE ) && withinBox(grid[gid].face[f].centroid,region.get_Vec3D("corner_1"),region.get_Vec3D("corner_2"))) {
					if (pick=="override") {
						grid[gid].face[f].bc=b; // real boundary conditions are marked as positive
					} else if (pick=="unassigned" && grid[gid].face[f].bc==UNASSIGNED_FACE) {
						grid[gid].face[f].bc=b; // real boundary conditions are marked as positive
					} else if (pick=="BC" && grid[gid].face[f].bc==(pick_from-1) ) {
						grid[gid].face[f].bc=b;
					} 
				}
			} // face loop
		} // if box

		BCregion bcRegion;

		string type=region.get_string("type");
		string kind=region.get_string("kind");
		if (type=="wall") bcRegion.type=WALL;
		if (type=="inlet") bcRegion.type=INLET;
		if (type=="outlet") bcRegion.type=OUTLET;
		if (type=="symmetry") bcRegion.type=SYMMETRY; 
		if (kind=="slip") bcRegion.kind=SLIP; // This is needed here to omit this BC in nearest wall distance calculation
		// Integrate boundary areas
		bcRegion.area=0.;
		bcRegion.total_area=0.;
		for (int f=0;f<grid[gid].faceCount;++f) {
			if (grid[gid].face[f].bc==b) {
				grid[gid].face[f].symmetry=false;
				grid[gid].maps.face2bc[f]=bc_counter[b];
				bc_counter[b]++;
				bcRegion.area+=grid[gid].face[f].area;
				bcRegion.areaVec+=grid[gid].face[f].area*grid[gid].face[f].normal;
				if (bcRegion.type==SYMMETRY) grid[gid].face[f].symmetry=true;
			}
		}
		// Get the total area
		MPI_Allreduce (&bcRegion.area,&bcRegion.total_area,1,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);
		bc[gid].push_back(bcRegion);
		
		string inter=region.get_string("interface");
		if (inter!="none") {
			int id=interface[gid].size();
			interface[gid].resize(id+1);
			string var,temp;
			int donor_grid,donor_bc;
			extract_in_between(inter,"get","from",var);
			extract_in_between(inter,"grid","bc",temp);
			interface[gid][id].donor_grid=atoi(temp.c_str())-1;
			interface[gid][id].donor_bc=atoi(inter.c_str())-1;
			interface[gid][id].donor_var=var;
			interface[gid][id].donor_eqn=equations[interface[gid][id].donor_grid];			
			interface[gid][id].recv_grid=gid;
			interface[gid][id].recv_bc=b;
			interface[gid][id].recv_eqn=equations[gid];
		} // end if interface!=none
	
	} // bc loop
	
	// Loop each interface to fill in recv_var
	for (int g=0;g<grid.size();++g) {
		for (int i=0;i<interface[g].size();++i) {
			// Loop donor interfaces
			for (int d=0;d<interface[interface[g][i].recv_grid].size();++d) {
				if (interface[g][i].recv_bc==interface[interface[g][i].recv_grid][d].donor_bc) {
					interface[g][i].recv_var=interface[interface[g][i].recv_grid][d].donor_var;
					break;
				}
			}
		}
	}


	grid[gid].boundaryFaceCount.resize(count);
	for (int b=0;b<count;++b) grid[gid].boundaryFaceCount[b].resize(np);
	grid[gid].globalBoundaryFaceCount.resize(count);
	for (int b=0;b<count;++b) {
		grid[gid].boundaryFaceCount[b][Rank]=0;
		for (int f=0;f<grid[gid].faceCount;++f) if(grid[gid].face[f].bc==b) grid[gid].boundaryFaceCount[b][Rank]++; 
		MPI_Allgather(&grid[gid].boundaryFaceCount[b][Rank],1,MPI_INT,&grid[gid].boundaryFaceCount[b][0],1,MPI_INT,MPI_COMM_WORLD);
		//cout << "[I rank=" << Rank << " grid=" << gid+1 << " BC=" << b+1 << "] Number of Faces=" << grid[gid].boundaryFaceCount[b][Rank] << endl;
		for (int p=0;p<np;++p) grid[gid].globalBoundaryFaceCount[b]+=grid[gid].boundaryFaceCount[b][p];
	}
		
	vector<int> number_of_nsf (np,0); // number of noslip faces in each partition
	// Mark nodes that touch boundaries
	for (int f=0;f<grid[gid].faceCount;++f) {
		if (grid[gid].face[f].bc==UNASSIGNED_FACE) {
			cerr << "[E rank=" << Rank << "] Boundary condition could not be found for face " << f << endl;
			exit(1);
		}
		if (grid[gid].face[f].bc>=0) { // if a boundary face
			if (bc[gid][grid[gid].face[f].bc].type==WALL && bc[gid][grid[gid].face[f].bc].kind!=SLIP) number_of_nsf[Rank]++;
			// A fix for centroids of inlet and outlet boundary ghost cells:
			if (bc[gid][grid[gid].face[f].bc].type==INLET || bc[gid][grid[gid].face[f].bc].type==OUTLET) {
				int neighbor=grid[gid].face[f].neighbor;
				int parent=grid[gid].face[f].parent;
				grid[gid].cell[neighbor].centroid=grid[gid].cell[parent].centroid+
						2.*(grid[gid].face[f].centroid-grid[gid].cell[parent].centroid);
			}
		}
	}



	if (Rank==0) cout << "[I] Finding closest wall distances" << endl;
	
	MPI_Allgather(&number_of_nsf[Rank],1,MPI_INT,&number_of_nsf[0],1,MPI_INT,MPI_COMM_WORLD);

	int nsf_sum=0;
	int displacements[np];
	// Total number of wall faces
	for (int p=0;p<np;++p) {
		displacements[p]=nsf_sum;
		nsf_sum+=number_of_nsf[p];
	}

	// Collect wall face centroids
	vector<double> noSlipFaces_x(nsf_sum);
	vector<double> noSlipFaces_y(nsf_sum);
	vector<double> noSlipFaces_z(nsf_sum);

	// Collect wall face normals
	vector<double> noSlipFaces_Nx(nsf_sum);
	vector<double> noSlipFaces_Ny(nsf_sum);
	vector<double> noSlipFaces_Nz(nsf_sum);

	count=0;
	for (int f=0;f<grid[gid].faceCount;++f) { // loop all the local faces
		if (grid[gid].face[f].bc>=0 && bc[gid][grid[gid].face[f].bc].type==WALL && bc[gid][grid[gid].face[f].bc].kind!=SLIP) {
			noSlipFaces_x[displacements[Rank]+count]=grid[gid].face[f].centroid[0];
			noSlipFaces_y[displacements[Rank]+count]=grid[gid].face[f].centroid[1];
			noSlipFaces_z[displacements[Rank]+count]=grid[gid].face[f].centroid[2];
			noSlipFaces_Nx[displacements[Rank]+count]=grid[gid].face[f].normal[0];
			noSlipFaces_Ny[displacements[Rank]+count]=grid[gid].face[f].normal[1];
			noSlipFaces_Nz[displacements[Rank]+count]=grid[gid].face[f].normal[2];
			count++;
		}
	}

	MPI_Allgatherv(&noSlipFaces_x[displacements[Rank]],number_of_nsf[Rank],MPI_DOUBLE,&noSlipFaces_x[0],&number_of_nsf[0],displacements,MPI_DOUBLE,MPI_COMM_WORLD);
	MPI_Allgatherv(&noSlipFaces_y[displacements[Rank]],number_of_nsf[Rank],MPI_DOUBLE,&noSlipFaces_y[0],&number_of_nsf[0],displacements,MPI_DOUBLE,MPI_COMM_WORLD);
	MPI_Allgatherv(&noSlipFaces_z[displacements[Rank]],number_of_nsf[Rank],MPI_DOUBLE,&noSlipFaces_z[0],&number_of_nsf[0],displacements,MPI_DOUBLE,MPI_COMM_WORLD);

	MPI_Allgatherv(&noSlipFaces_Nx[displacements[Rank]],number_of_nsf[Rank],MPI_DOUBLE,&noSlipFaces_Nx[0],&number_of_nsf[0],displacements,MPI_DOUBLE,MPI_COMM_WORLD);
	MPI_Allgatherv(&noSlipFaces_Ny[displacements[Rank]],number_of_nsf[Rank],MPI_DOUBLE,&noSlipFaces_Ny[0],&number_of_nsf[0],displacements,MPI_DOUBLE,MPI_COMM_WORLD);
	MPI_Allgatherv(&noSlipFaces_Nz[displacements[Rank]],number_of_nsf[Rank],MPI_DOUBLE,&noSlipFaces_Nz[0],&number_of_nsf[0],displacements,MPI_DOUBLE,MPI_COMM_WORLD);

	Vec3D thisCentroid;
	Vec3D fN;
	double thisDistance;
	// Loop all cells to find the closest distance to the wall
	for (int c=0;c<grid[gid].cell.size();++c) {
		grid[gid].cell[c].closest_wall_distance=1.e20;
               
		for (int nsf=0;nsf<nsf_sum;++nsf) {
			thisCentroid[0]=noSlipFaces_x[nsf];
			thisCentroid[1]=noSlipFaces_y[nsf];
			thisCentroid[2]=noSlipFaces_z[nsf];
			thisDistance=fabs(grid[gid].cell[c].centroid-thisCentroid);
			if (grid[gid].cell[c].closest_wall_distance>thisDistance) {
				grid[gid].cell[c].closest_wall_distance=thisDistance;
			}
	        }
	}

	// Loop all faces to find the closest distance to the wall
        for (int f=0;f<grid[gid].faceCount;++f) {
        	grid[gid].face[f].closest_wall_distance=1.e20;

		// If the face is not a no-slip wall
                if (grid[gid].face[f].bc<0 || bc[gid][grid[gid].face[f].bc].type!=WALL || bc[gid][grid[gid].face[f].bc].kind==SLIP) {
                    for (int nsf=0;nsf<nsf_sum;++nsf) {
                        thisCentroid[0]=noSlipFaces_x[nsf];
                        thisCentroid[1]=noSlipFaces_y[nsf];
                        thisCentroid[2]=noSlipFaces_z[nsf];
                        thisDistance=fabs(grid[gid].face[f].centroid-thisCentroid);
                        if (grid[gid].face[f].closest_wall_distance>thisDistance) {
                                grid[gid].face[f].closest_wall_distance=thisDistance;
                                fN[0]=noSlipFaces_Nx[nsf];
                                fN[1]=noSlipFaces_Ny[nsf];
                                fN[2]=noSlipFaces_Nz[nsf];
                        }
                    }
                    grid[gid].face[f].dissipation_factor=1.-fabs(fN[0]*grid[gid].face[f].normal[0]+fN[1]*grid[gid].face[f].normal[1]+fN[2]*grid[gid].face[f].normal[2]);
                } else {
                       grid[gid].face[f].closest_wall_distance=0.;
                       grid[gid].face[f].dissipation_factor=0.;
        	}
	}


	noSlipFaces_x.clear();
	noSlipFaces_Nx.clear();
	noSlipFaces_y.clear();
	noSlipFaces_Ny.clear();
	noSlipFaces_z.clear();
	noSlipFaces_Nz.clear();

	return;
}
