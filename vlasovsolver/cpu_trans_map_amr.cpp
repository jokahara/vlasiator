#include "cpu_1d_ppm_nonuniform.hpp"
//#include "cpu_1d_ppm_nonuniform_conserving.hpp"
#include "vec.h"
#include "../grid.h"
#include "../object_wrapper.h"
#include "cpu_trans_map_amr.hpp"
#include "cpu_trans_map.hpp"

using namespace std;
using namespace spatial_cell;

// indices in padded source block, which is of type Vec with VECL
// element sin each vector. b_k is the block index in z direction in
// ordinary space [- VLASOV_STENCIL_WIDTH to VLASOV_STENCIL_WIDTH],
// i,j,k are the cell ids inside on block (i in vector elements).
// Vectors with same i,j,k coordinates, but in different spatial cells, are consequtive
//#define i_trans_ps_blockv(j, k, b_k)  ( (b_k + VLASOV_STENCIL_WIDTH ) + ( (((j) * WID + (k) * WID2)/VECL)  * ( 1 + 2 * VLASOV_STENCIL_WIDTH) ) )
#define i_trans_ps_blockv(planeVectorIndex, planeIndex, blockIndex) ( (blockIndex) + VLASOV_STENCIL_WIDTH  +  ( (planeVectorIndex) + (planeIndex) * VEC_PER_PLANE ) * ( 1 + 2 * VLASOV_STENCIL_WIDTH)  )

// indices in padded target block, which is of type Vec with VECL
// element sin each vector. b_k is the block index in z direction in
// ordinary space, i,j,k are the cell ids inside on block (i in vector
// elements).
//#define i_trans_pt_blockv(j, k, b_k) ( ( (j) * WID + (k) * WID2 + ((b_k) + 1 ) * WID3) / VECL )
#define i_trans_pt_blockv(planeVectorIndex, planeIndex, blockIndex)  ( planeVectorIndex + planeIndex * VEC_PER_PLANE + (blockIndex + 1) * VEC_PER_BLOCK)

#define i_trans_ps_blockv_pencil(planeVectorIndex, planeIndex, blockIndex, lengthOfPencil) ( (blockIndex) + VLASOV_STENCIL_WIDTH  +  ( (planeVectorIndex) + (planeIndex) * VEC_PER_PLANE ) * ( lengthOfPencil + 2 * VLASOV_STENCIL_WIDTH) )

int getNeighborhood(const uint dimension, const uint stencil) {

   int neighborhood = 0;

   if (stencil == 1) {
      switch (dimension) {
      case 0:
         neighborhood = VLASOV_SOLVER_TARGET_X_NEIGHBORHOOD_ID;
         break;
      case 1:
         neighborhood = VLASOV_SOLVER_TARGET_Y_NEIGHBORHOOD_ID;
         break;
      case 2:
         neighborhood = VLASOV_SOLVER_TARGET_Z_NEIGHBORHOOD_ID;
         break;
      }
   }

   if (stencil == 2) {
      switch (dimension) {
      case 0:
         neighborhood = VLASOV_SOLVER_X_NEIGHBORHOOD_ID;
         break;
      case 1:
         neighborhood = VLASOV_SOLVER_Y_NEIGHBORHOOD_ID;
         break;
      case 2:
         neighborhood = VLASOV_SOLVER_Z_NEIGHBORHOOD_ID;
         break;
      }
   }
   
   return neighborhood;
   
}

void computeSpatialSourceCellsForPencil(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                                        setOfPencils pencils,
                                        const uint iPencil,
                                        const uint dimension,
                                        SpatialCell **sourceCells){

   // L = length of the pencil iPencil
   int L = pencils.lengthOfPencils[iPencil];
   vector<CellID> ids = pencils.getIds(iPencil);
   
   int neighborhood = getNeighborhood(dimension,2);

   // Get pointers for each cell id of the pencil
   for (int i = 0; i < L; ++i) {
      sourceCells[i + VLASOV_STENCIL_WIDTH] = mpiGrid[ids[i]];
   }
   
   // Insert pointers for neighbors of ids.front() and ids.back()
   auto* frontNbrPairs = mpiGrid.get_neighbors_of(ids.front(), neighborhood);
   auto* backNbrPairs  = mpiGrid.get_neighbors_of(ids.back(),  neighborhood);

   int maxRefLvl = mpiGrid.get_maximum_refinement_level();
   int iSrc = 0;
      
   // Create list of unique distances in the negative direction from the first cell in pencil
   std::set< int > distances;
   for (auto nbrPair : *frontNbrPairs) {
      if(nbrPair.second[dimension] < 0) {
         distances.insert(nbrPair.second[dimension]);
      }
   }

   // Iterate through distances for VLASOV_STENCIL_WIDTH elements starting from the largest distance.
   // Distances are negative here so largest distance has smallest value
   auto ibeg = distances.begin();
   std::advance(ibeg, distances.size() - VLASOV_STENCIL_WIDTH);
   for (auto it = ibeg; it != distances.end(); ++it) {
      // Collect all neighbors at distance *it to a vector
      std::vector< CellID > neighbors;
      for (auto nbrPair : *frontNbrPairs) {
         int distanceInRefinedCells = nbrPair.second[dimension];
         if(distanceInRefinedCells == *it) neighbors.push_back(nbrPair.first);
      }

      int refLvl = mpiGrid.get_refinement_level(ids.front());
      
      if (neighbors.size() == 1) {
         sourceCells[iSrc++] = mpiGrid[neighbors.at(0)];
      } else if ( pencils.path[iPencil][refLvl] < neighbors.size() ) {
         sourceCells[iSrc++] = mpiGrid[neighbors.at(pencils.path[iPencil][refLvl])];
      }
   }

   iSrc = L + VLASOV_STENCIL_WIDTH;
   distances.clear();
   // Create list of unique distances in the positive direction from the last cell in pencil
   for (auto nbrPair : *backNbrPairs) {
      if(nbrPair.second[dimension] > 0) {
         distances.insert(nbrPair.second[dimension]);
      }
   }

   // Iterate through distances for VLASOV_STENCIL_WIDTH elements starting from the smallest distance.
   // Distances are positive here so smallest distance has smallest value.
   auto iend = distances.begin();
   std::advance(iend,VLASOV_STENCIL_WIDTH);
   for (auto it = distances.begin(); it != iend; ++it) {
         
      // Collect all neighbors at distance *it to a vector
      std::vector< CellID > neighbors;
      for (auto nbrPair : *backNbrPairs) {
         int distanceInRefinedCells = nbrPair.second[dimension];
         if(distanceInRefinedCells == *it) neighbors.push_back(nbrPair.first);
      }

      int refLvl = mpiGrid.get_refinement_level(ids.back());

      if (neighbors.size() == 1) {
         sourceCells[iSrc++] = mpiGrid[neighbors.at(0)];
      } else if ( pencils.path[iPencil][refLvl] < neighbors.size() ) {
         sourceCells[iSrc++] = mpiGrid[neighbors.at(pencils.path[iPencil][refLvl])];
      }
   }

   /*loop to neative side and replace all invalid cells with the closest good cell*/
   SpatialCell* lastGoodCell = mpiGrid[ids.front()];
   for(int i = VLASOV_STENCIL_WIDTH - 1; i >= 0 ;i--){
      if(sourceCells[i] == NULL) 
         sourceCells[i] = lastGoodCell;
      else
         lastGoodCell = sourceCells[i];
   }
   /*loop to positive side and replace all invalid cells with the closest good cell*/
   lastGoodCell = mpiGrid[ids.back()];
   for(int i = L + VLASOV_STENCIL_WIDTH; i < L + 2*VLASOV_STENCIL_WIDTH; i++){
      if(sourceCells[i] == NULL) 
         sourceCells[i] = lastGoodCell;
      else
         lastGoodCell = sourceCells[i];
   }
}

/*compute spatial target neighbors for pencils of size N. No boundary cells are included*/
void computeSpatialTargetCellsForPencils(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                                         setOfPencils& pencils,
                                         const uint dimension,
                                         SpatialCell **targetCells){

   int neighborhood = getNeighborhood(dimension,1);
   
   uint GID = 0;
   // Loop over pencils
   for(uint iPencil = 0; iPencil < pencils.N; iPencil++){
      
      // L = length of the pencil iPencil
      int L = pencils.lengthOfPencils[iPencil];
      vector<CellID> ids = pencils.getIds(iPencil);
      
      //std::cout << "Target cells for pencil " << iPencil << ": ";
      
      // Get pointers for each cell id of the pencil
      for (int i = 0; i < L; ++i) {
         targetCells[GID + i + 1] = mpiGrid[ids[i]];
         //std::cout << ids[i] << " ";
      }
      //std::cout << std::endl;

      // Insert pointers for neighbors of ids.front() and ids.back()
      auto frontNbrPairs = mpiGrid.get_neighbors_of(ids.front(), neighborhood);
      auto backNbrPairs  = mpiGrid.get_neighbors_of(ids.back(),  neighborhood);
      
      // std::cout << "Ghost cells: ";
      // std::cout << frontNbrPairs->front().first << " ";
      // std::cout << backNbrPairs->back().first << std::endl;
      vector <CellID> frontNeighborIds;
      for( auto nbrPair: *frontNbrPairs ) {
         if (nbrPair.second.at(dimension) == -1) {
            frontNeighborIds.push_back(nbrPair.first);
         }
      }

      if (frontNeighborIds.size() == 0) {
         abort();
      }
      
      vector <CellID> backNeighborIds;
      for( auto nbrPair: *backNbrPairs ) {
         if (nbrPair.second.at(dimension) == 1) {
            backNeighborIds.push_back(nbrPair.first);
         }
      }

      if (backNeighborIds.size() == 0) {
         abort();
      }

      int refLvl = mpiGrid.get_refinement_level(ids.front());

      if (frontNeighborIds.size() == 1) {
         targetCells[GID] = mpiGrid[frontNeighborIds.at(0)];
      } else if ( pencils.path[iPencil][refLvl] < frontNeighborIds.size() ) {
         targetCells[GID] = mpiGrid[frontNeighborIds.at(pencils.path[iPencil][refLvl])];
      }
      
      refLvl = mpiGrid.get_refinement_level(ids.back());

      if (backNeighborIds.size() == 1) {
         targetCells[GID + L + 1] = mpiGrid[backNeighborIds.at(0)];
      } else if ( pencils.path[iPencil][refLvl] < backNeighborIds.size() ) {
         targetCells[GID + L + 1] = mpiGrid[backNeighborIds.at(pencils.path[iPencil][refLvl])];
      }
           
      // Incerment global id by L + 2 ghost cells.
      GID += (L + 2);
   }

   for (uint i = 0; i < GID; ++i) {

      if (targetCells[i]->sysBoundaryFlag != sysboundarytype::NOT_SYSBOUNDARY ) {
         targetCells[i] = NULL;
      }
   }

}

CellID selectNeighbor(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry> &grid,
                      CellID id, int dimension = 0, uint path = 0) {

   int neighborhood = getNeighborhood(dimension,1);
   
   const auto* nbrPairs = grid.get_neighbors_of(id, neighborhood);
   
   vector < CellID > myNeighbors;
   CellID neighbor = INVALID_CELLID;
   
   // Iterate through neighbor ids in the positive direction of the chosen dimension,
   // select the neighbor indicated by path, if it is local to this process.
   for (const auto nbrPair : *nbrPairs) {
      if (nbrPair.second[dimension] == 1) {
         myNeighbors.push_back(nbrPair.first);
      }
   }

   if( myNeighbors.size() == 0 ) {
      return neighbor;
   }
   
   int neighborIndex = 0;
   if (myNeighbors.size() > 1) {
      neighborIndex = path;
   }
   
   if (grid.is_local(myNeighbors[neighborIndex])) {
      neighbor = myNeighbors[neighborIndex];
   }
   
   return neighbor;
}


// void removeDuplicates(setOfPencils &pencils) {

//    vector<uint> duplicatePencilIds;

//    // Loop over all pencils twice to do cross-comparisons
//    for (uint myPencilId = 0; myPencilId < pencils.N; ++myPencilId) {

//       vector<CellID> myCellIds = pencils.getIds(myPencilId);
      
//       for (uint theirPencilId = 0; theirPencilId < pencils.N; ++theirPencilId) {

//          // Do not compare with self
//          if (myPencilId == theirPencilId) {
//             continue;
//          }

//          // we check if all cells of pencil b ("their") are included in pencil a ("my")
//          bool removeThisPencil = true;
         
//          vector<CellID> theirCellIds = pencils.getIds(theirPencilId);

//          for (auto theirCellId : theirCellIds) {
//             bool matchFound = false;
//             for (auto myCellId : myCellIds) {
//                // Compare each "my" cell to all "their" cells, if any of them match
//                // update a logical value matchFound to true.
//                if (myCellId == theirCellId && pencils.path[myPencilId] == pencils.path[theirPencilId]) {
//                   matchFound = true;
//                }
//             }
//             // If no match was found for this "my" cell, we can end the comparison, these pencils
//             // are not duplicates.
//             if(!matchFound) {
//                removeThisPencil = false;
//                continue;
//             }
//          }

//          if(removeThisPencil) {
//             if(std::find(duplicatePencilIds.begin(), duplicatePencilIds.end(), myPencilId) == duplicatePencilIds.end() ) {
//                duplicatePencilIds.push_back(theirPencilId);  
//             }
            
//          }
         
//       }
      
//    }

//    int myRank;
//    MPI_Comm_rank(MPI_COMM_WORLD,&myRank);

//    for (auto id : duplicatePencilIds) {
//       //pencils.removePencil(id);
//       cout << "I am rank " << myRank << ", I would like to remove pencil number " << id << endl;
//    }   
// }

setOfPencils buildPencilsWithNeighbors( const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry> &grid, 
					setOfPencils &pencils, const CellID startingId,
					vector<CellID> ids, const uint dimension, 
					vector<uint> path, const vector<CellID> &endIds) {

   const bool debug = false;
   CellID nextNeighbor;
   CellID id = startingId;
   uint startingRefLvl = grid.get_refinement_level(id);

   if( ids.size() == 0 )
      ids.push_back(startingId);

   // If the cell where we start is refined, we need to figure out which path
   // to follow in future refined cells. This is a bit hacky but we have to
   // use the order or the children of the parent cell to figure out which
   // corner we are in.
   // Maybe you could use physical coordinates here?
   if( startingRefLvl > path.size() ) {

      vector<CellID> localIndices;
      auto indices = grid.mapping.get_indices(id);
      auto length = grid.mapping.get_cell_length_in_indices(grid.mapping.get_level_0_parent(id));
      for (auto index : indices) {
         localIndices.push_back(index % length);
      }
      
      for ( uint i = path.size(); i < startingRefLvl; i++) {         

         vector<CellID> localIndicesOnRefLvl;
         
         for ( auto lid : localIndices ) {
            localIndicesOnRefLvl.push_back( lid  / pow(2, startingRefLvl - (i + 1) ));
         }

         int i1 = 0;
         int i2 = 0;
         
         switch( dimension ) {
         case 0:
            i1 = localIndicesOnRefLvl.at(1);
            i2 = localIndicesOnRefLvl.at(2);
            break;
         case 1:
            i1 = localIndicesOnRefLvl.at(0);
            i2 = localIndicesOnRefLvl.at(2);
            break;
         case 2:
            i1 = localIndicesOnRefLvl.at(0);
            i2 = localIndicesOnRefLvl.at(1);
            break;
         }

         if( i1 > 1 || i2 > 1) {
            std::cout << __FILE__ << " " << __LINE__ << " Something went wrong, i1 = " << i1 << ", i2 = " << i2 << std::endl;
         }

         path.push_back(i1 + 2 * i2);
      }
   }

   id = startingId;

   bool periodic = false;
   
   while (id != INVALID_CELLID) {

      periodic = false;
      bool neighborExists = false;
      int refLvl = 0;
      
      // Find the refinement level in the neighboring cell. Check all possible neighbors
      // in case some of them are remote.
      for (int tmpPath = 0; tmpPath < 4; ++tmpPath) {
         nextNeighbor = selectNeighbor(grid,id,dimension,tmpPath);
         if(nextNeighbor != INVALID_CELLID) {
            refLvl = max(refLvl,grid.get_refinement_level(nextNeighbor));
            neighborExists = true;
         }
      }
         
      // If there are no neighbors, we can stop.
      if (!neighborExists)
         break;   

      if (refLvl > 0) {
    
         // If we have encountered this refinement level before and stored
         // the path this builder follows, we will just take the same path
         // again.
         if ( static_cast<int>(path.size()) >= refLvl ) {
      
            if(debug) {
               std::cout << "I am cell " << id << ". ";
               std::cout << "I have seen refinement level " << refLvl << " before. Path is ";
               for (auto k = path.begin(); k != path.end(); ++k)
                  std::cout << *k << " ";
               std::cout << std::endl;
            }
	
            nextNeighbor = selectNeighbor(grid,id,dimension,path[refLvl - 1]);      
	
         } else {
	
            if(debug) {
               std::cout << "I am cell " << id << ". ";
               std::cout << "I have NOT seen refinement level " << refLvl << " before. Path is ";
               for (auto k = path.begin(); k != path.end(); ++k)
                  std::cout << *k << ' ';
               std::cout << std::endl;
            }
	
            // New refinement level, create a path through each neighbor cell
            for ( uint i : {0,1,2,3} ) {
	  
               vector < uint > myPath = path;
               myPath.push_back(i);
	  
               nextNeighbor = selectNeighbor(grid,id,dimension,myPath.back());
	  
               if ( i == 3 ) {
	    
                  // This builder continues with neighbor 3
                  //ids.push_back(nextNeighbor);
                  path = myPath;
	    
               } else {
	    
                  // Spawn new builders for neighbors 0,1,2
                  buildPencilsWithNeighbors(grid,pencils,id,ids,dimension,myPath,endIds);
	    
               }
	  
            }
	
         }

      } else {
         if(debug) {
            std::cout << "I am cell " << id << ". ";
            std::cout << " I am on refinement level 0." << std::endl;
         }
      }// Closes if (refLvl == 0)

      // If we found a neighbor, add it to the list of ids for this pencil.
      if(nextNeighbor != INVALID_CELLID) {
         if (debug) {
            std::cout << " Next neighbor is " << nextNeighbor << "." << std::endl;
         }

         if ( std::any_of(endIds.begin(), endIds.end(), [nextNeighbor](uint i){return i == nextNeighbor;}) ||
              !do_translate_cell(grid[nextNeighbor])) {
            // grid[nextNeighbor]->sysBoundaryFlag != sysboundarytype::NOT_SYSBOUNDARY )  {
            // grid[nextNeighbor]->sysBoundaryFlag == sysboundarytype::DO_NOT_COMPUTE ||
            // ( grid[nextNeighbor]->sysBoundaryLayer == 2 &&
            //   grid[nextNeighbor]->sysBoundaryFlag != sysboundarytype::NOT_SYSBOUNDARY ) )  {
            
            nextNeighbor = INVALID_CELLID;
         } else {
            ids.push_back(nextNeighbor);
         }
      }
      
      id = nextNeighbor;
   } // Closes while loop

   // Get the x,y - coordinates of the pencil (in the direction perpendicular to the pencil)
   const auto coordinates = grid.get_center(ids[0]);
   double x,y;
   uint ix,iy,iz;
      
   switch(dimension) {
   case 0: 
      ix = 1;
      iy = 2;
      iz = 0;
      break;
   
   case 1: 
      ix = 2;
      iy = 0;
      iz = 1;
      break;
   
   case 2: 
      ix = 0;
      iy = 1;
      iz = 2;
      break;
   
   default: 
      ix = 0;
      iy = 1;
      iz = 2;
      break;   
   }
   
   x = coordinates[ix];
   y = coordinates[iy];

   pencils.addPencil(ids,x,y,periodic,path);
   
   return pencils;
  
}

//void propagatePencil(Vec dr[], Vec values, Vec z_translation, uint blocks_per_dim ) {
void propagatePencil(Vec* dz, Vec* values, const uint dimension,
                     const uint blockGID, const Realv dt,
                     const vmesh::VelocityMesh<vmesh::GlobalID,vmesh::LocalID> &vmesh, const uint lengthOfPencil) {

   // Get velocity data from vmesh that we need later to calculate the translation
   velocity_block_indices_t block_indices;
   uint8_t refLevel;
   vmesh.getIndices(blockGID,refLevel, block_indices[0], block_indices[1], block_indices[2]);
   Realv dvz = vmesh.getCellSize(refLevel)[dimension];
   Realv vz_min = vmesh.getMeshMinLimits()[dimension];
   
   // Assuming 1 neighbor in the target array because of the CFL condition
   // In fact propagating to > 1 neighbor will give an error
   const uint nTargetNeighborsPerPencil = 1;

   // Vector buffer where we write data, initialized to 0*/
   Vec targetValues[(lengthOfPencil + 2 * nTargetNeighborsPerPencil) * WID3 / VECL];
   
   for (uint i = 0; i < (lengthOfPencil + 2 * nTargetNeighborsPerPencil) * WID3 / VECL; i++) {
      
      // init target_values
      targetValues[i] = Vec(0.0);
      
   }
   
   // Go from 0 to length here to propagate all the cells in the pencil
   for (uint i = 0; i < lengthOfPencil; i++){      
      
      // The source array is padded by VLASOV_STENCIL_WIDTH on both sides.
      uint i_source   = i + VLASOV_STENCIL_WIDTH;
      
      for (uint k = 0; k < WID; ++k) {

         const Realv cell_vz = (block_indices[dimension] * WID + k + 0.5) * dvz + vz_min; //cell centered velocity
         const Vec z_translation = cell_vz * dt / dz[i_source]; // how much it moved in time dt (reduced units)

         // Determine direction of translation
         // part of density goes here (cell index change along spatial direcion)
         Vecb positiveTranslationDirection = (z_translation > Vec(0.0));
         
         // Calculate normalized coordinates in current cell.
         // The coordinates (scaled units from 0 to 1) between which we will
         // integrate to put mass in the target  neighboring cell.
         // Normalize the coordinates to the origin cell. Then we scale with the difference
         // in volume between target and origin later when adding the integrated value.
         Vec z_1,z_2;
         z_1 = select(positiveTranslationDirection, 1.0 - z_translation, 0.0);
         z_2 = select(positiveTranslationDirection, 1.0, - z_translation);

         // if( horizontal_or(abs(z_1) > Vec(1.0)) || horizontal_or(abs(z_2) > Vec(1.0)) ) {
         //    std::cout << "Error, CFL condition violated\n";
         //    std::cout << "Exiting\n";
         //    std::exit(1);
         // }
         
         for (uint planeVector = 0; planeVector < VEC_PER_PLANE; planeVector++) {   
      
            // Compute polynomial coefficients
            Vec a[3];
            // Dz: is a padded array, pointer can point to the beginning, i + VLASOV_STENCIL_WIDTH will get the right cell.
            // values: transpose function adds VLASOV_STENCIL_WIDTH to the block index, therefore we substract it here, then
            // i + VLASOV_STENCIL_WIDTH will point to the right cell. Complicated! Why! Sad! MVGA!
            compute_ppm_coeff_nonuniform(dz,
                                         values + i_trans_ps_blockv_pencil(planeVector, k, i-VLASOV_STENCIL_WIDTH, lengthOfPencil),
                                         h4, VLASOV_STENCIL_WIDTH, a);
            
            // Compute integral
            const Vec ngbr_target_density =
               z_2 * ( a[0] + z_2 * ( a[1] + z_2 * a[2] ) ) -
               z_1 * ( a[0] + z_1 * ( a[1] + z_1 * a[2] ) );
                                    
            // Store mapped density in two target cells
            // in the neighbor cell we will put this density
            targetValues[i_trans_pt_blockv(planeVector, k, i + 1)] += select( positiveTranslationDirection,
                                                                              ngbr_target_density
                                                                              * dz[i_source] / dz[i_source + 1],
                                                                              Vec(0.0));
            targetValues[i_trans_pt_blockv(planeVector, k, i - 1 )] += select(!positiveTranslationDirection,
                                                                              ngbr_target_density
                                                                              * dz[i_source] / dz[i_source - 1],
                                                                              Vec(0.0));
            
            // in the current original cells we will put the rest of the original density
            targetValues[i_trans_pt_blockv(planeVector, k, i)] += 
               values[i_trans_ps_blockv_pencil(planeVector, k, i, lengthOfPencil)] - ngbr_target_density;
         }
      }
   }

   // Write target data into source data
   // VLASOV_STENCIL_WIDTH >= nTargetNeighborsPerPencil is required (default 2 >= 1)

   for (uint i = 0; i < lengthOfPencil + 2 * nTargetNeighborsPerPencil; i++) {

      for (uint k = 0; k < WID; ++k) {
         
         for (uint planeVector = 0; planeVector < VEC_PER_PLANE; planeVector++) {            
            int im1 = i - 1; // doing this to shut up compiler warnings
            values[i_trans_ps_blockv_pencil(planeVector, k, im1, lengthOfPencil)] =
               targetValues[i_trans_pt_blockv(planeVector, k, im1)];
            
         }
      }
   }  
}

void getSeedIds(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                const vector<CellID> &localPropagatedCells,
                const uint dimension,
                vector<CellID> &seedIds) {

   const bool debug = false;
   int myRank;
   if (debug) MPI_Comm_rank(MPI_COMM_WORLD,&myRank);
   
   int neighborhood = getNeighborhood(dimension,1);
   
   for(auto celli : localPropagatedCells) {

      auto myIndices = mpiGrid.mapping.get_indices(celli);
      
      bool addToSeedIds = false;
      // Returns all neighbors as (id, direction-dimension) pair pointers.
      for ( const auto nbrPair : *(mpiGrid.get_neighbors_of(celli, neighborhood)) ) {
         
         if ( nbrPair.second[dimension] == -1 ) {

            // Check that the neighbor is not across a periodic boundary by calculating
            // the distance in indices between this cell and its neighbor.
            auto nbrIndices = mpiGrid.mapping.get_indices(nbrPair.first);

            // If a neighbor is non-local, across a periodic boundary, or in non-periodic boundary layer 1
            // then we use this cell as a seed for pencils
            if ( abs ( myIndices[dimension] - nbrIndices[dimension] ) >
                 pow(2,mpiGrid.get_maximum_refinement_level()) ||
                 !mpiGrid.is_local(nbrPair.first) ||
                 !do_translate_cell(mpiGrid[nbrPair.first]) ) {              
                  
                  // ( mpiGrid[nbrPair.first]->sysBoundaryFlag != sysboundarytype::NOT_SYSBOUNDARY &&
                  //   mpiGrid[nbrPair.first]->sysBoundaryLayer == 1 ) )  {
                  
                  // mpiGrid[nbrPair.first]->sysBoundaryFlag == sysboundarytype::DO_NOT_COMPUTE ||
                  // ( mpiGrid[nbrPair.first]->sysBoundaryLayer == 2 &&
                  //   mpiGrid[nbrPair.first]->sysBoundaryFlag != sysboundarytype::NOT_SYSBOUNDARY ) )  {
                  
               addToSeedIds = true;
            }
         }
      }

      if ( addToSeedIds ) {
         seedIds.push_back(celli);
      }

      //cout << endl;
   }

   if(debug) {
      //cout << "Number of seed ids is " << seedIds.size() << endl;
      cout << "Rank " << myRank << ", Seed ids are: ";
      for (const auto seedId : seedIds) {
         cout << seedId << " ";
      }
      cout << endl;
   }   
}




/* Copy the data to the temporary values array, so that the
 * dimensions are correctly swapped. Also, copy the same block for
 * then neighboring spatial cells (in the dimension). neighbors
 * generated with compute_spatial_neighbors_wboundcond).
 * 
 * This function must be thread-safe.
 *
 * @param source_neighbors Array containing the VLASOV_STENCIL_WIDTH closest 
 * spatial neighbors of this cell in the propagated dimension.
 * @param blockGID Global ID of the velocity block.
 * @param int lengthOfPencil Number of spatial cells in pencil
 * @param values Vector where loaded data is stored.
 * @param cellid_transpose
 * @param popID ID of the particle species.
 */
void copy_trans_block_data_amr(
    SpatialCell** source_neighbors,
    const vmesh::GlobalID blockGID,
    int lengthOfPencil,
    Vec* values,
    const unsigned char* const cellid_transpose,
    const uint popID) { 

   // Allocate data pointer for all blocks in pencil. Pad on both ends by VLASOV_STENCIL_WIDTH
   Realf* blockDataPointer[lengthOfPencil + 2 * VLASOV_STENCIL_WIDTH];   

   for (int b = -VLASOV_STENCIL_WIDTH; b < lengthOfPencil + VLASOV_STENCIL_WIDTH; b++) {
      // Get cell pointer and local block id
      SpatialCell* srcCell = source_neighbors[b + VLASOV_STENCIL_WIDTH];
         
      const vmesh::LocalID blockLID = srcCell->get_velocity_block_local_id(blockGID,popID);
      if (blockLID != srcCell->invalid_local_id()) {
         // Get data pointer
         blockDataPointer[b + VLASOV_STENCIL_WIDTH] = srcCell->get_data(blockLID,popID);
         // //prefetch storage pointers to L1
         // _mm_prefetch((char *)(blockDataPointer[b + VLASOV_STENCIL_WIDTH]), _MM_HINT_T0);
         // _mm_prefetch((char *)(blockDataPointer[b + VLASOV_STENCIL_WIDTH]) + 64, _MM_HINT_T0);
         // _mm_prefetch((char *)(blockDataPointer[b + VLASOV_STENCIL_WIDTH]) + 128, _MM_HINT_T0);
         // _mm_prefetch((char *)(blockDataPointer[b + VLASOV_STENCIL_WIDTH]) + 192, _MM_HINT_T0);
         // if(VPREC  == 8) {
         //   //prefetch storage pointers to L1
         //   _mm_prefetch((char *)(blockDataPointer[b + VLASOV_STENCIL_WIDTH]) + 256, _MM_HINT_T0);
         //   _mm_prefetch((char *)(blockDataPointer[b + VLASOV_STENCIL_WIDTH]) + 320, _MM_HINT_T0);
         //   _mm_prefetch((char *)(blockDataPointer[b + VLASOV_STENCIL_WIDTH]) + 384, _MM_HINT_T0);
         //   _mm_prefetch((char *)(blockDataPointer[b + VLASOV_STENCIL_WIDTH]) + 448, _MM_HINT_T0);
         // }
         
      } else {
         blockDataPointer[b + VLASOV_STENCIL_WIDTH] = NULL;
      }
   }
   
   //  Copy volume averages of this block from all spatial cells:
   for (int b = -VLASOV_STENCIL_WIDTH; b < lengthOfPencil + VLASOV_STENCIL_WIDTH; b++) {
      if(blockDataPointer[b + VLASOV_STENCIL_WIDTH] != NULL) {
         Realv blockValues[WID3];
         const Realf* block_data = blockDataPointer[b + VLASOV_STENCIL_WIDTH];
         // Copy data to a temporary array and transpose values so that mapping is along k direction.
         // spatial source_neighbors already taken care of when
         // creating source_neighbors table. If a normal spatial cell does not
         // simply have the block, its value will be its null_block which
         // is fine. This null_block has a value of zero in data, and that
         // is thus the velocity space boundary
         for (uint i=0; i<WID3; ++i) {
            blockValues[i] = block_data[cellid_transpose[i]];
         }
         
         // now load values into the actual values table..
         uint offset =0;
         for (uint k=0; k<WID; k++) {
            for(uint planeVector = 0; planeVector < VEC_PER_PLANE; planeVector++){
               // store data, when reading data from data we swap dimensions 
               // using precomputed plane_index_to_id and cell_indices_to_id
               values[i_trans_ps_blockv_pencil(planeVector, k, b, lengthOfPencil)].load(blockValues + offset);
               offset += VECL;
            }
         }
      } else {
         for (uint k=0; k<WID; ++k) {
            for(uint planeVector = 0; planeVector < VEC_PER_PLANE; planeVector++) {
               values[i_trans_ps_blockv_pencil(planeVector, k, b, lengthOfPencil)] = Vec(0);
            }
         }
      }
   }
}

/* 
Check whether the ghost cells around the pencil contain higher refinement than the pencil does.
If they do, the pencil must be split to match the finest refined ghost cell. This function checks
One neighbor pair, but takes as an argument the offset from the pencil. Call multiple times for
Multiple ghost cells.
 */
void check_ghost_cells(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                       setOfPencils& pencils,
                       uint dimension) {

   const bool debug = false;
   int neighborhoodId = getNeighborhood(dimension,VLASOV_STENCIL_WIDTH);

   int myRank;
   if(debug) {
      MPI_Comm_rank(MPI_COMM_WORLD,&myRank);
   }
   
   std::vector<CellID> idsToSplit;
   
   for (uint pencili = 0; pencili < pencils.N; ++pencili) {

      if(pencils.periodic[pencili]) continue;
         
      auto ids = pencils.getIds(pencili);

      // It is possible that the pencil has already been refined by the pencil building algorithm
      // and is on a higher refinement level than the refinement level of any of the cells it contains
      // due to e.g. process boundaries.
      int maxPencilRefLvl = pencils.path.at(pencili).size();
      int maxNbrRefLvl = 0;
         
      const auto* frontNeighbors = mpiGrid.get_neighbors_of(ids.front(),neighborhoodId);
      const auto* backNeighbors  = mpiGrid.get_neighbors_of(ids.back() ,neighborhoodId);

      for (auto nbrPair: *frontNeighbors) {
	//if((nbrPair.second[dimension] + 1) / pow(2,mpiGrid.get_refinement_level(nbrPair.first)) == -offset) {
	maxNbrRefLvl = max(maxNbrRefLvl,mpiGrid.mapping.get_refinement_level(nbrPair.first));
	//}
      }
         
      for (auto nbrPair: *backNeighbors) {
	//if((nbrPair.second[dimension] + 1) / pow(2,mpiGrid.get_refinement_level(nbrPair.first)) == offset) {
	maxNbrRefLvl = max(maxNbrRefLvl,mpiGrid.get_refinement_level(nbrPair.first));
	//}
      }

      if (maxNbrRefLvl > maxPencilRefLvl) {
         if(debug) {
            std::cout << "I am rank " << myRank << ". ";
            std::cout << "Found refinement level " << maxNbrRefLvl << " in one of the ghost cells of pencil " << pencili << ". ";
            std::cout << "Highest refinement level in this pencil is " << maxPencilRefLvl;
            std::cout << ". Splitting pencil " << pencili << endl;
         }
         // Let's avoid modifying pencils while we are looping over it. Write down the indices of pencils
         // that need to be split and split them later.
         idsToSplit.push_back(pencili);
      }
   }

   for (auto pencili: idsToSplit) {

      Realv dx = 0.0;
      Realv dy = 0.0;
      // TODO: Double-check that this gives you the right dimensions!
      auto ids = pencils.getIds(pencili);
      switch(dimension) {
      case 0:
         dx = mpiGrid[ids[0]]->SpatialCell::parameters[CellParams::DY];
         dy = mpiGrid[ids[0]]->SpatialCell::parameters[CellParams::DZ];
         break;
      case 1:
         dx = mpiGrid[ids[0]]->SpatialCell::parameters[CellParams::DX];
         dy = mpiGrid[ids[0]]->SpatialCell::parameters[CellParams::DZ];
         break;
      case 2:
         dx = mpiGrid[ids[0]]->SpatialCell::parameters[CellParams::DX];
         dy = mpiGrid[ids[0]]->SpatialCell::parameters[CellParams::DY];
         break;
      }

      pencils.split(pencili,dx,dy);
         
   }
}

bool checkPencils(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                  const std::vector<CellID> &cells, const setOfPencils& pencils) {

   bool correct = true;

   for (auto id : cells) {

      if (mpiGrid[id]->sysBoundaryFlag == sysboundarytype::NOT_SYSBOUNDARY )  {
      
         int myCount = std::count(pencils.ids.begin(), pencils.ids.end(), id);
         
         if( myCount == 0 || (myCount != 1 && myCount % 4 != 0)) {        
            
            std::cerr << "ERROR: Cell ID " << id << " Appears in pencils " << myCount << " times!"<< std::endl;            
            correct = false;
         }

      }
      
   }

   return correct;
   
}

void printPencilsFunc(const setOfPencils& pencils, const uint dimension, const int myRank) {
   
   // Print out ids of pencils (if needed for debugging)
   uint ibeg = 0;
   uint iend = 0;
   std::cout << "I am rank " << myRank << ", I have " << pencils.N << " pencils along dimension " << dimension << ":\n";
   MPI_Barrier(MPI_COMM_WORLD);
   if(myRank == MASTER_RANK) {
      std::cout << "N, mpirank, (x, y): indices {path} " << std::endl;
      std::cout << "-----------------------------------------------------------------" << std::endl;
   }
   MPI_Barrier(MPI_COMM_WORLD);
   for (uint i = 0; i < pencils.N; i++) {
      iend += pencils.lengthOfPencils[i];
      std::cout << i << ", ";
      std::cout << myRank << ", ";
      std::cout << "(" << pencils.x[i] << ", " << pencils.y[i] << "): ";
      for (auto j = pencils.ids.begin() + ibeg; j != pencils.ids.begin() + iend; ++j) {
         std::cout << *j << " ";
      }
      ibeg  = iend;
      
      std::cout << "{";         
      for (auto step : pencils.path[i]) {
         std::cout << step << ", ";
      }
      std::cout << "}";
      
      std::cout << std::endl;
   }

   MPI_Barrier(MPI_COMM_WORLD);
}

bool trans_map_1d_amr(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                      const vector<CellID>& localPropagatedCells,
                      const vector<CellID>& remoteTargetCells,
                      const uint dimension,
                      const Realv dt,
                      const uint popID) {

   const bool printPencils = false;
   const bool printTargets = false;
   Realv dvz,vz_min;  
   uint cell_indices_to_id[3]; /*< used when computing id of target cell in block*/
   unsigned char  cellid_transpose[WID3]; /*< defines the transpose for the solver internal (transposed) id: i + j*WID + k*WID2 to actual one*/
   const uint blocks_per_dim = 1;
   // return if there's no cells to propagate
   if(localPropagatedCells.size() == 0) {
      cout << "Returning because of no cells" << endl;
      return false;
   }

   int myRank;
   if(printTargets || printPencils) MPI_Comm_rank(MPI_COMM_WORLD,&myRank);
   
   // Vector with all cell ids
   vector<CellID> allCells(localPropagatedCells);
   allCells.insert(allCells.end(), remoteTargetCells.begin(), remoteTargetCells.end());  

   // Vectors of pointers to the cell structs
   std::vector<SpatialCell*> allCellsPointer(allCells.size());  
   
   // Initialize allCellsPointer
   #pragma omp parallel for
   for(uint celli = 0; celli < allCells.size(); celli++){
      allCellsPointer[celli] = mpiGrid[allCells[celli]];
   }

   // Fiddle indices x,y,z in VELOCITY SPACE
   switch (dimension) {
   case 0:
      // set values in array that is used to convert block indices 
      // to global ID using a dot product.
      cell_indices_to_id[0]=WID2;
      cell_indices_to_id[1]=WID;
      cell_indices_to_id[2]=1;
      break;
   case 1:
      // set values in array that is used to convert block indices 
      // to global ID using a dot product
      cell_indices_to_id[0]=1;
      cell_indices_to_id[1]=WID2;
      cell_indices_to_id[2]=WID;
      break;
   case 2:
      // set values in array that is used to convert block indices
      // to global id using a dot product.
      cell_indices_to_id[0]=1;
      cell_indices_to_id[1]=WID;
      cell_indices_to_id[2]=WID2;
      break;
   default:
      cerr << __FILE__ << ":"<< __LINE__ << " Wrong dimension, abort"<<endl;
      abort();
      break;
   }
           
   // init cellid_transpose
   for (uint k=0; k<WID; ++k) {
      for (uint j=0; j<WID; ++j) {
         for (uint i=0; i<WID; ++i) {
            const uint cell =
               i * cell_indices_to_id[0] +
               j * cell_indices_to_id[1] +
               k * cell_indices_to_id[2];
            cellid_transpose[ i + j * WID + k * WID2] = cell;
         }
      }
   }
   // ****************************************************************************

   // compute pencils => set of pencils (shared datastructure)
   
   vector<CellID> seedIds;
   getSeedIds(mpiGrid, localPropagatedCells, dimension, seedIds);
   
   // Empty vectors for internal use of buildPencilsWithNeighbors. Could be default values but
   // default vectors are complicated. Should overload buildPencilsWithNeighbors like suggested here
   // https://stackoverflow.com/questions/3147274/c-default-argument-for-vectorint
   vector<CellID> ids;
   vector<uint> path;

   // Output vectors for ready pencils
   setOfPencils pencils;
   vector<setOfPencils> pencilSets;

   for (const auto seedId : seedIds) {
      // Construct pencils from the seedIds into a set of pencils.
      pencils = buildPencilsWithNeighbors(mpiGrid, pencils, seedId, ids, dimension, path, seedIds);
   }   
   
   // Check refinement of two ghost cells on each end of each pencil
   check_ghost_cells(mpiGrid,pencils,dimension);
   // ****************************************************************************   

   if(printPencils) printPencilsFunc(pencils,dimension,myRank);

   if(!checkPencils(mpiGrid, localPropagatedCells, pencils)) {
      abort();
   }
   
   // // Remove duplicates
   // removeDuplicates(pencils);
   
   // Add the final set of pencils to the pencilSets - vector.
   // Only one set is created for now but we retain support for multiple sets
   pencilSets.push_back(pencils);
   
   const uint8_t VMESH_REFLEVEL = 0;
   
   // Get a pointer to the velocity mesh of the first spatial cell
   const vmesh::VelocityMesh<vmesh::GlobalID,vmesh::LocalID>& vmesh = allCellsPointer[0]->get_velocity_mesh(popID);   
      
   // Get a unique sorted list of blockids that are in any of the
   // propagated cells. First use set for this, then add to vector (may not
   // be the most nice way to do this and in any case we could do it along
   // dimension for data locality reasons => copy acc map column code, TODO: FIXME
   // TODO: Do this separately for each pencil?
   std::unordered_set<vmesh::GlobalID> unionOfBlocksSet;    
   
   for(auto cell : allCellsPointer) {
      vmesh::VelocityMesh<vmesh::GlobalID,vmesh::LocalID>& vmesh = cell->get_velocity_mesh(popID);
      for (vmesh::LocalID block_i=0; block_i< vmesh.size(); ++block_i) {
         unionOfBlocksSet.insert(vmesh.getGlobalID(block_i));
      }
   }
   
   std::vector<vmesh::GlobalID> unionOfBlocks;
   unionOfBlocks.reserve(unionOfBlocksSet.size());
   for(const auto blockGID:  unionOfBlocksSet) {
      unionOfBlocks.push_back(blockGID);
   }
   // ****************************************************************************
   
   int t1 = phiprof::initializeTimer("mapping");
   int t2 = phiprof::initializeTimer("store");
   
#pragma omp parallel
   {
      // Loop over velocity space blocks. Thread this loop (over vspace blocks) with OpenMP.    
#pragma omp for schedule(guided)
      for(uint blocki = 0; blocki < unionOfBlocks.size(); blocki++) {         

         // Get global id of the velocity block
         vmesh::GlobalID blockGID = unionOfBlocks[blocki];

         velocity_block_indices_t block_indices;
         uint8_t vRefLevel;
         vmesh.getIndices(blockGID,vRefLevel, block_indices[0],
                          block_indices[1], block_indices[2]);
         
         // Loop over sets of pencils
         // This loop only has one iteration for now
         for ( auto pencils: pencilSets ) {

            phiprof::start(t1);
            
            std::vector<Realf> targetBlockData((pencils.sumOfLengths + 2 * pencils.N) * WID3);
            // Allocate vectorized targetvecdata sum(lengths of pencils)*WID3 / VECL)
            // Add padding by 2 for each pencil
            Vec targetVecData[(pencils.sumOfLengths + 2 * pencils.N) * WID3 / VECL];
            
            // Initialize targetvecdata to 0
            for( uint i = 0; i < (pencils.sumOfLengths + 2 * pencils.N) * WID3 / VECL; i++ ) {
               targetVecData[i] = Vec(0.0);
            }
            
            // TODO: There's probably a smarter way to keep track of where we are writing
            //       in the target data array.
            uint targetDataIndex = 0;
            
            // Compute spatial neighbors for target cells.
            // For targets we need the local cells, plus a padding of 1 cell at both ends
            std::vector<SpatialCell*> targetCells(pencils.sumOfLengths + pencils.N * 2 );
            //std::vector<bool> targetsValid(pencils.sumOfLengths + 2 * pencils.N) = false;
            
            computeSpatialTargetCellsForPencils(mpiGrid, pencils, dimension, targetCells.data());           

            // Loop over pencils
            uint totalTargetLength = 0;
            for(uint pencili = 0; pencili < pencils.N; ++pencili){
               
               int L = pencils.lengthOfPencils[pencili];
               uint targetLength = L + 2;
               uint sourceLength = L + 2 * VLASOV_STENCIL_WIDTH;
               
               // Compute spatial neighbors for the source cells of the pencil. In
               // source cells we have a wider stencil and take into account boundaries.
               std::vector<SpatialCell*> sourceCells(sourceLength);

               computeSpatialSourceCellsForPencil(mpiGrid, pencils, pencili, dimension, sourceCells.data());

               // dz is the cell size in the direction of the pencil
               Vec dz[sourceCells.size()];
               uint i = 0;
               for(auto cell: sourceCells) {
                  switch (dimension) {
                  case(0):
                     dz[i] = cell->SpatialCell::parameters[CellParams::DX];
                     break;                  
                  case(1):
                     dz[i] = cell->SpatialCell::parameters[CellParams::DY];
                     break;                  
                  case(2):
                     dz[i] = cell->SpatialCell::parameters[CellParams::DZ];
                     break;
                  }
                  
                  i++;
               }

               // Allocate source data: sourcedata<length of pencil * WID3)
               // Add padding by 2 * VLASOV_STENCIL_WIDTH
               Vec sourceVecData[(sourceLength) * WID3 / VECL];                              

               // load data(=> sourcedata) / (proper xy reconstruction in future)
               copy_trans_block_data_amr(sourceCells.data(), blockGID, L, sourceVecData,
                                         cellid_transpose, popID);

               // Dz and sourceVecData are both padded by VLASOV_STENCIL_WIDTH
               // Dz has 1 value/cell, sourceVecData has WID3 values/cell
               propagatePencil(dz, sourceVecData, dimension, blockGID, dt, vmesh, L);

               if (printTargets) std::cout << "Target cells for pencil " << pencili << ", rank " << myRank << ": ";
               // sourcedata => targetdata[this pencil])
               for (uint i = 0; i < targetLength; i++) {
		 if (printTargets) {
		   if( targetCells[i + totalTargetLength] != NULL) {		     
		     std::cout << targetCells[i + totalTargetLength]->parameters[CellParams::CELLID] << " ";
		   } else {
		     std::cout << "NULL" << " ";
		   }
		 }
                  for (uint k=0; k<WID; k++) {
                     for(uint planeVector = 0; planeVector < VEC_PER_PLANE; planeVector++){
                        
                        targetVecData[i_trans_pt_blockv(planeVector, k, totalTargetLength + i - 1)] =
                           sourceVecData[i_trans_ps_blockv_pencil(planeVector, k, i - 1, L)];
                     
                     }
                  }
               }
               if (printTargets) std::cout << std::endl;
               totalTargetLength += targetLength;
               // dealloc source data -- Should be automatic since it's declared in this loop iteration?
            }

            phiprof::stop(t1);
            phiprof::start(t2);
            
            // reset blocks in all non-sysboundary neighbor spatial cells for this block id
            // At this point the data is saved in targetVecData so we can reset the spatial cells

            for (auto *spatial_cell: targetCells) {
               // Check for null and system boundary
               if (spatial_cell && spatial_cell->sysBoundaryFlag == sysboundarytype::NOT_SYSBOUNDARY) {
                  // Get local velocity block id
                  const vmesh::LocalID blockLID = spatial_cell->get_velocity_block_local_id(blockGID, popID);
                  // Check for invalid id
                  if (blockLID != vmesh::VelocityMesh<vmesh::GlobalID,vmesh::LocalID>::invalidLocalID()) {
                     // Get a pointer to the block data
                     Realf* blockData = spatial_cell->get_data(blockLID, popID);
                     // Loop over velocity block cells
                     for(int i = 0; i < WID3; i++) {
                        blockData[i] = 0.0;
                     }
                  }
               }
            }

            // store_data(target_data => targetCells)  :Aggregate data for blockid to original location 
            // Loop over pencils again
            totalTargetLength = 0;
            for(uint pencili = 0; pencili < pencils.N; pencili++){
               
               int L = pencils.lengthOfPencils[pencili];
               uint targetLength = L + 2;
               //vector<CellID> pencilIds = pencils.getIds(pencili);

               // Calculate the max and min refinement levels in this pencil.
               // All cells that are not on the max refinement level will be split
               // Into multiple pencils. This has to be taken into account when adding
               // up the contributions from each pencil.
               // The most convenient way is to just count how many refinement steps the
               // pencil has taken on its path.
               int pencilRefLvl = pencils.path[pencili].size();
               
               // Unpack the vector data

               // Loop over cells in pencil +- 1 padded cell
               for ( uint celli = 0; celli < targetLength; ++celli ) {
                  
                  Realv vector[VECL];
                  // Loop over 1st vspace dimension
                  for (uint k = 0; k < WID; ++k) {
                     // Loop over 2nd vspace dimension
                     for(uint planeVector = 0; planeVector < VEC_PER_PLANE; planeVector++) {
                        targetVecData[i_trans_pt_blockv(planeVector, k, totalTargetLength + celli - 1)].store(vector);
                        // Loop over 3rd (vectorized) vspace dimension
                        for (uint i = 0; i < VECL; i++) {
                           targetBlockData[(totalTargetLength + celli) * WID3 +
                                           cellid_transpose[i + planeVector * VECL + k * WID2]]
                              = vector[i];
                        }
                     }
                  }
               }

               // store values from targetBlockData array to the actual blocks
               // Loop over cells in the pencil, including the padded cells of the target array
               for ( uint celli = 0; celli < targetLength; celli++ ) {
                  
                  uint GID = celli + totalTargetLength; 
                  SpatialCell* targetCell = targetCells[GID];

                  if(targetCell) {
                  
                     const vmesh::LocalID blockLID = targetCell->get_velocity_block_local_id(blockGID, popID);

                     if( blockLID == vmesh::VelocityMesh<vmesh::GlobalID,vmesh::LocalID>::invalidLocalID() ) {
                        // Invalid target spatial cell
                        continue;
                     }
                     
                     Realf* blockData = targetCell->get_data(blockLID, popID);
                     
                     // areaRatio is the reatio of the cross-section of the spatial cell to the cross-section of the pencil.
                     Realf areaRatio = pow(pow(2,targetCell->SpatialCell::parameters[CellParams::REFINEMENT_LEVEL] - pencils.path[pencili].size()),2);;
                     
                     // Realf checksum = 0.0;
                     for(int i = 0; i < WID3 ; i++) {
                        blockData[i] += targetBlockData[GID * WID3 + i] * areaRatio;
                        // checksum += targetBlockData[GID * WID3 + i] * areaRatio;
                     }
                  }
               }

               totalTargetLength += targetLength;
               
               // dealloc target data -- Should be automatic again?
            }

            phiprof::stop(t2);
         }
      }
   }

   return true;
}

int get_sibling_index(dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid, const CellID& cellid) {

   const int NO_SIBLINGS = 0;
   const int ERROR = -1;
   
   if(mpiGrid.get_refinement_level(cellid) == 0) {
      return NO_SIBLINGS;
   }
   
   CellID parent = mpiGrid.get_parent(cellid);

   if (parent == INVALID_CELLID) {
      return ERROR;
   }
   
   vector<CellID> siblings = mpiGrid.get_all_children(parent);
   auto location = std::find(siblings.begin(),siblings.end(),cellid);
   auto index = std::distance(siblings.begin(), location);

   return index;
   
}

/*!

  This function communicates the mapping on process boundaries, and then updates the data to their correct values.
  TODO, this could be inside an openmp region, in which case some m ore barriers and masters should be added

  \par dimension: 0,1,2 for x,y,z
  \par direction: 1 for + dir, -1 for - dir
*/
void update_remote_mapping_contribution(
   dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
   const uint dimension,
   int direction,
   const uint popID) {
   
   vector<CellID> local_cells = mpiGrid.get_cells();
   const vector<CellID> remote_cells = mpiGrid.get_remote_cells_on_process_boundary(VLASOV_SOLVER_NEIGHBORHOOD_ID);
   vector<CellID> receive_cells;
   set<CellID> send_cells;
   
   vector<CellID> receive_origin_cells;
   vector<uint> receive_origin_index;

   int neighborhood = 0;

   // For debugging
   int myRank;
   MPI_Comm_rank(MPI_COMM_WORLD,&myRank);
   
   //normalize and set neighborhoods
   if(direction > 0) {
      direction = 1;
      switch (dimension) {
      case 0:
         neighborhood = SHIFT_P_X_NEIGHBORHOOD_ID;
         break;
      case 1:
         neighborhood = SHIFT_P_Y_NEIGHBORHOOD_ID;
         break;
      case 2:
         neighborhood = SHIFT_P_Z_NEIGHBORHOOD_ID;
         break;
      }
   }
   if(direction < 0) {
      direction = -1;
      switch (dimension) {
      case 0:
         neighborhood = SHIFT_M_X_NEIGHBORHOOD_ID;
         break;
      case 1:
         neighborhood = SHIFT_M_Y_NEIGHBORHOOD_ID;
         break;
      case 2:
         neighborhood = SHIFT_M_Z_NEIGHBORHOOD_ID;
         break;
      }
   }

   // MPI_Barrier(MPI_COMM_WORLD);
   // cout << "begin update_remote_mapping_contribution, dimension = " << dimension << ", direction = " << direction << endl;
   // MPI_Barrier(MPI_COMM_WORLD);

   // Initialize remote cells
   for (auto rc : remote_cells) {
      SpatialCell *ccell = mpiGrid[rc];
      // Initialize number of blocks to 0 and block data to a default value.
      // We need the default for 1 to 1 communications
      if(ccell) {
         for (uint i = 0; i < MAX_NEIGHBORS_PER_DIM; ++i) {
            ccell->neighbor_block_data[i] = ccell->get_data(popID);
            ccell->neighbor_number_of_blocks[i] = 0;
         }
      }
   }

   // Initialize local cells
   for (auto lc : local_cells) {
      SpatialCell *ccell = mpiGrid[lc];
      if(ccell) {
         // Initialize number of blocks to 0 and neighbor block data pointer to the local block data pointer
         for (uint i = 0; i < MAX_NEIGHBORS_PER_DIM; ++i) {
            ccell->neighbor_block_data[i] = ccell->get_data(popID);
            ccell->neighbor_number_of_blocks[i] = 0;
         }
      }
   }
   
   vector<Realf*> receiveBuffers;
   vector<Realf*> sendBuffers;

   // Sort local cells.
   // This is done to make sure that when receiving data, a refined neighbor will overwrite a less-refined neighbor
   // on the same rank. This is done because a non-refined neighbor, if such exist simultaneously with a refined neighbor
   // is a non-face neighbor and therefore does not store the received data, but can screw up the block count.
   // The sending rank will only consider face neighbors when determining the number of blocks it will send.
   std::sort(local_cells.begin(), local_cells.end());
   
   for (auto c : local_cells) {
      
      SpatialCell *ccell = mpiGrid[c];

      if (!ccell) continue;

      // Send to neighbors_to
      auto* nbrToPairVector = mpiGrid.get_neighbors_to(c, neighborhood);
      // Receive from neighbors_of
      auto* nbrOfPairVector = mpiGrid.get_neighbors_of(c, neighborhood);
      
      uint sendIndex = 0;
      uint recvIndex = 0;

      int mySiblingIndex = get_sibling_index(mpiGrid,c);
      
      // Set up sends if any neighbor cells in nbrToPairVector are non-local.
      if (!all_of(nbrToPairVector->begin(), nbrToPairVector->end(),
                  [&mpiGrid](pair<CellID,array<int,4>> i){return mpiGrid.is_local(i.first);})) {

         // ccell adds a neighbor_block_data block for each neighbor in the positive direction to its local data
         for (auto nbrPair : *nbrToPairVector) {
         
            bool initBlocksForEmptySiblings = false;
            CellID nbr = nbrPair.first;
            
            //Send data in nbr target array that we just mapped to, if
            // 1) it is a valid target,
            // 2) the source cell in center was translated,
            // 3) Cell is remote.
            if(nbr != INVALID_CELLID && do_translate_cell(ccell) && !mpiGrid.is_local(nbr)) {

               /*
                 Select the index to the neighbor_block_data and neighbor_number_of_blocks arrays
                 1) Ref_c == Ref_nbr == 0, index = 0
                 2) Ref_c == Ref_nbr != 0, index = c sibling index
                 3) Ref_c >  Ref_nbr     , index = c sibling index
                 4) Ref_c <  Ref_nbr     , index = nbr sibling index
                */
               
               if(mpiGrid.get_refinement_level(c) >= mpiGrid.get_refinement_level(nbr)) {
                  sendIndex = mySiblingIndex;
               } else if (mpiGrid.get_refinement_level(nbr) >  mpiGrid.get_refinement_level(c)) {
                  sendIndex = get_sibling_index(mpiGrid,nbr);
               }               
            
               SpatialCell *pcell = mpiGrid[nbr];
               
               // 4) it exists and is not a boundary cell,
               if(pcell && pcell->sysBoundaryFlag == sysboundarytype::NOT_SYSBOUNDARY) {
                  
                  if(send_cells.find(nbr) == send_cells.end()) {
                     // 5a) We have not already sent data from this rank to this cell.                     
                     
                     ccell->neighbor_number_of_blocks.at(sendIndex) = pcell->get_number_of_velocity_blocks(popID);
                     ccell->neighbor_block_data.at(sendIndex) = pcell->get_data(popID);
                     
                     auto *allNbrs = mpiGrid.get_neighbors_of(c, FULL_NEIGHBORHOOD_ID);
                     bool faceNeighbor = false;
                     for (auto nbrPair : *allNbrs) {
                        if(nbrPair.first == nbr && abs(nbrPair.second.at(dimension)) == 1) {
                           faceNeighbor = true;
                        }
                     }
                     if (faceNeighbor) {
                        send_cells.insert(nbr);
                     }
                     
                  } else {
                     ccell->neighbor_number_of_blocks.at(sendIndex) = mpiGrid[nbr]->get_number_of_velocity_blocks(popID);
                     ccell->neighbor_block_data.at(sendIndex) =
                        (Realf*) aligned_malloc(ccell->neighbor_number_of_blocks.at(sendIndex) * WID3 * sizeof(Realf), 64);
                     sendBuffers.push_back(ccell->neighbor_block_data.at(sendIndex));
                     for (uint j = 0; j < ccell->neighbor_number_of_blocks.at(sendIndex) * WID3; ++j) {
                        ccell->neighbor_block_data[sendIndex][j] = 0.0;
                        
                     } // closes for(uint j = 0; j < ccell->neighbor_number_of_blocks.at(sendIndex) * WID3; ++j)
                     
                  } // closes if(send_cells.find(nbr) == send_cells.end())
                  
               } // closes if(pcell && pcell->sysBoundaryFlag == sysboundarytype::NOT_SYSBOUNDARY)
               
            } // closes if(nbr != INVALID_CELLID && do_translate_cell(ccell) && !mpiGrid.is_local(nbr))
            
         } // closes for(uint i_nbr = 0; i_nbr < nbrs_to.size(); ++i_nbr)
        
      } // closes if(!all_of(nbrs_to.begin(), nbrs_to.end(),[&mpiGrid](CellID i){return mpiGrid.is_local(i);}))

      // Set up receives if any neighbor cells in nbrOfPairVector are non-local.
      if (!all_of(nbrOfPairVector->begin(), nbrOfPairVector->end(),
                  [&mpiGrid](pair<CellID,array<int,4>> i){return mpiGrid.is_local(i.first);})) {

         // ccell adds a neighbor_block_data block for each neighbor in the positive direction to its local data
         for (auto nbrPair : *nbrOfPairVector) {     

            CellID nbr = nbrPair.first;
         
            if (nbr != INVALID_CELLID && !mpiGrid.is_local(nbr) &&
                ccell->sysBoundaryFlag == sysboundarytype::NOT_SYSBOUNDARY) {
               //Receive data that ncell mapped to this local cell data array,
               //if 1) ncell is a valid source cell, 2) center cell is to be updated (normal cell) 3) ncell is remote

               SpatialCell *ncell = mpiGrid[nbr];

               // Check for null pointer
               if(!ncell) {
                  continue;
               }

               /*
                 Select the index to the neighbor_block_data and neighbor_number_of_blocks arrays
                 1) Ref_nbr == Ref_c == 0, index = 0
                 2) Ref_nbr == Ref_c != 0, index = nbr sibling index
                 3) Ref_nbr >  Ref_c     , index = nbr sibling index
                 4) Ref_nbr <  Ref_c     , index = c   sibling index
                */
               
               if(mpiGrid.get_refinement_level(nbr) >= mpiGrid.get_refinement_level(c)) {
                  recvIndex = get_sibling_index(mpiGrid,nbr);
               } else if (mpiGrid.get_refinement_level(c) >  mpiGrid.get_refinement_level(nbr)) {
                  recvIndex = mySiblingIndex;
               }
                              
               if(mpiGrid.get_refinement_level(nbr) < mpiGrid.get_refinement_level(c)) {

                  auto mySiblings = mpiGrid.get_all_children(mpiGrid.get_parent(c));
                  auto myIndices = mpiGrid.mapping.get_indices(c);
                  
                  // Allocate memory for each sibling to receive all the data sent by coarser ncell. 
                  // nbrs_to of the sender will only include the face neighbors, only allocate blocks for those.
                  for (uint i_sib = 0; i_sib < MAX_NEIGHBORS_PER_DIM; ++i_sib) {

                     auto sibling = mySiblings.at(i_sib);
                     auto sibIndices = mpiGrid.mapping.get_indices(sibling);
                     
                     // Only allocate siblings that are remote face neighbors to ncell
                     if(mpiGrid.get_process(sibling) != mpiGrid.get_process(nbr)
                        && myIndices.at(dimension) == sibIndices.at(dimension)) {
                     
                        auto* scell = mpiGrid[sibling];
                        
                        ncell->neighbor_number_of_blocks.at(i_sib) = scell->get_number_of_velocity_blocks(popID);
                        ncell->neighbor_block_data.at(i_sib) =
                           (Realf*) aligned_malloc(ncell->neighbor_number_of_blocks.at(i_sib) * WID3 * sizeof(Realf), 64);
                        receiveBuffers.push_back(ncell->neighbor_block_data.at(i_sib));
                     } 
                  } 
               } else {

                  // Allocate memory for one sibling at the previously calculated recvIndex.
                  ncell->neighbor_number_of_blocks.at(recvIndex) = ccell->get_number_of_velocity_blocks(popID);
                  ncell->neighbor_block_data.at(recvIndex) =
                     (Realf*) aligned_malloc(ncell->neighbor_number_of_blocks.at(recvIndex) * WID3 * sizeof(Realf), 64);
                  receiveBuffers.push_back(ncell->neighbor_block_data.at(recvIndex));
               }

               // Only nearest neighbors (nbrpair.second(dimension) == 1 are added to the
               // block data of the receiving cells
               if (abs(nbrPair.second.at(dimension)) == 1) {
               
                  receive_cells.push_back(c);
                  receive_origin_cells.push_back(nbr);
                  receive_origin_index.push_back(recvIndex);
                  
               }
               
            } // closes (nbr != INVALID_CELLID && !mpiGrid.is_local(nbr) && ...)
            
         } // closes for(uint i_nbr = 0; i_nbr < nbrs_of.size(); ++i_nbr)
         
      } // closes if(!all_of(nbrs_of.begin(), nbrs_of.end(),[&mpiGrid](CellID i){return mpiGrid.is_local(i);}))
      
   } // closes for (auto c : local_cells) {
   
   // Do communication
   SpatialCell::setCommunicatedSpecies(popID);
   SpatialCell::set_mpi_transfer_type(Transfer::NEIGHBOR_VEL_BLOCK_DATA);
   mpiGrid.update_copies_of_remote_neighbors(neighborhood);

   // Reduce data: sum received data in the data array to 
   // the target grid in the temporary block container   
   //#pragma omp parallel
   {
      for (size_t c = 0; c < receive_cells.size(); ++c) {
         SpatialCell* receive_cell = mpiGrid[receive_cells[c]];
         SpatialCell* origin_cell = mpiGrid[receive_origin_cells[c]];

         if(!receive_cell || !origin_cell) {
            continue;
         }
         
         Realf *blockData = receive_cell->get_data(popID);
         Realf *neighborData = origin_cell->neighbor_block_data[receive_origin_index[c]];

         // cout << "Rank " << myRank << ", dim " << dimension << ", dir " << direction;
         // cout << ". Neighbor data of remote cell " << receive_origin_cells[c] << " is added to local cell " << receive_cells[c];
         // cout << " with index " << receive_origin_index[c];

         Realf checksum = 0.0;
         
         //#pragma omp for 
         for(uint vCell = 0; vCell < VELOCITY_BLOCK_LENGTH * receive_cell->get_number_of_velocity_blocks(popID); ++vCell) {
            blockData[vCell] += neighborData[vCell];
            checksum += neighborData[vCell];
         }

         //cout << ". Sum is " << checksum << endl;

         // array<Realf,8> allChecksums = {};
         // cout << ". Sums are ";         
         // for (uint i = 0; i < MAX_NEIGHBORS_PER_DIM; ++i) {
         //    neighborData = origin_cell->neighbor_block_data[i];
         //    for(uint vCell = 0; vCell < VELOCITY_BLOCK_LENGTH * receive_cell->get_number_of_velocity_blocks(popID); ++vCell) {
         //       allChecksums[i] += neighborData[vCell];
         //    }
         //    cout << allChecksums[i] << " ";
         // }
         // cout << endl;
      }
      
      // send cell data is set to zero. This is to avoid double copy if
      // one cell is the neighbor on bot + and - side to the same process
      for (auto c : send_cells) {
         SpatialCell* spatial_cell = mpiGrid[c];
         Realf * blockData = spatial_cell->get_data(popID);         
         //#pragma omp for nowait
         for(unsigned int vCell = 0; vCell < VELOCITY_BLOCK_LENGTH * spatial_cell->get_number_of_velocity_blocks(popID); ++vCell) {
            // copy received target data to temporary array where target data is stored.
            blockData[vCell] = 0;
         }
      }
   }

   for (auto p : receiveBuffers) {      
      aligned_free(p);
   }
   for (auto p : sendBuffers) {      
      aligned_free(p);
   }

   // MPI_Barrier(MPI_COMM_WORLD);
   // cout << "end update_remote_mapping_contribution, dimension = " << dimension << ", direction = " << direction << endl;
   // MPI_Barrier(MPI_COMM_WORLD);

}
