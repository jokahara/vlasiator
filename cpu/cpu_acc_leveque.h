#ifndef CPU_ACC_H
#define CPU_ACC_H

#include <map>
#include "definitions.h"
#include "common.h"
#include "cell_spatial.h"
#include "cpu_common.h"
#include "project.h"

template<typename T> T accIndex(const T& i,const T& j,const T& k) {return k*WID2+j*WID+i;}
template<typename T> T fullInd(const T& i,const T& j,const T& k) {return k*64+j*8+i;}
template<typename T> T isBoundary(const T& STATE,const T& BND) {return STATE & BND;}

template<typename T> T findex(const T& i,const T& j,const T& k) {return k*36+j*6+i;}

template<typename T> T limiter(const T& THETA) {
   //return vanLeer(THETA);
   //return MClimiter(THETA);
   return superbee(THETA);
   //return modbee2(THETA);
}

template<typename T> T velDerivs1(const T& xl2,const T& xl1,const T& xcc,const T& xr1,const T& xr2) {
   return superbee(xl1,xcc,xr1);
}

template<typename T> T velDerivs2(const T& xl2,const T& xl1,const T& xcc,const T& xr1,const T& xr2) {
   return convert<T>(0.0);
}

template<typename REAL,typename UINT> void accumulateChanges(const UINT& BLOCK,const REAL* const dF,REAL* const flux,const UINT* const nbrsVel) {
   UINT nbrBlock;
   REAL* nbrFlux;
   const UINT STATE = nbrsVel[NbrsVel::STATE];
   typedef Parameters P;

   // NOTE: velocity block can have up to 26 neighbours, and we need to copy changes 
   // to each existing neighbour here.
   // NOTE2: dF is a (6,6,6) sized block, nbrFlux is (4,4,4).
   // NOTE3: This function copies values correctly
   const UINT MIN = 0;
   const UINT MAX = 5;
   const UINT ACCMIN = 0;
   const UINT ACCMAX = 3;
   
   // Accumulate changes to this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) 
     flux[BLOCK*SIZE_FLUXS+accIndex(i,j,k)] += dF[findex(i+1,j+1,k+1)];
   
   // Accumulate changes to (iv-1,jv,kv) neighbour if it exists:
   if (isBoundary(STATE,NbrsVel::VX_NEG_BND) == 0) {
      nbrBlock = nbrsVel[NbrsVel::VXNEG];
      nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) nbrFlux[accIndex(ACCMAX,j,k)] += dF[findex(MIN,j+1,k+1)];
      // Accumulate changes to (iv-1,jv+-1,kv) neighbours if they exist:
      if (isBoundary(STATE,NbrsVel::VY_NEG_BND) == 0) {
	 nbrBlock = BLOCK - P::vxblocks_ini - 1; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT k=0; k<WID; ++k) nbrFlux[accIndex(ACCMAX,ACCMAX,k)] += dF[findex(MIN,MIN,k+1)];
      }
      if (isBoundary(STATE,NbrsVel::VY_POS_BND) == 0) {
	 nbrBlock = BLOCK + P::vxblocks_ini - 1; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT k=0; k<WID; ++k) nbrFlux[accIndex(ACCMAX,ACCMIN,k)] += dF[findex(MIN,MAX,k+1)];
      }
   }
   // Accumulate changes to (iv+1,jv,kv) neighbour if it exists:
   if (isBoundary(STATE,NbrsVel::VX_POS_BND) == 0) {
      nbrBlock = nbrsVel[NbrsVel::VXPOS];
      nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) nbrFlux[accIndex(ACCMIN,j,k)] += dF[findex(MAX,j+1,k+1)];
      // Accumulate changes to (iv+1,jv+-1,kv) neighbours if they exist:
      if (isBoundary(STATE,NbrsVel::VY_NEG_BND) == 0) {
	 nbrBlock = BLOCK - P::vxblocks_ini + 1; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT k=0; k<WID; ++k) nbrFlux[accIndex(ACCMIN,ACCMAX,k)] += dF[findex(MAX,MIN,k+1)];
      }
      if (isBoundary(STATE,NbrsVel::VY_POS_BND) == 0) {
	 nbrBlock = BLOCK + P::vxblocks_ini + 1; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT k=0; k<WID; ++k) nbrFlux[accIndex(ACCMIN,ACCMIN,k)] += dF[findex(MAX,MAX,k+1)];
      }
   }
   // Accumulate changes to -vy neighbour if it exists:
   if (isBoundary(STATE,NbrsVel::VY_NEG_BND) == 0) {
      nbrBlock = nbrsVel[NbrsVel::VYNEG];
      nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) nbrFlux[accIndex(i,ACCMAX,k)] += dF[findex(i+1,MIN,k+1)];
      // Accumulate changes to (iv,jv-1,kv+-1) neighbours if they exist:
      if (isBoundary(STATE,NbrsVel::VZ_NEG_BND) == 0) {
	 nbrBlock = BLOCK - P::vyblocks_ini*P::vxblocks_ini - P::vxblocks_ini; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT i=0; i<WID; ++i) nbrFlux[accIndex(i,ACCMAX,ACCMAX)] += dF[findex(i+1,MIN,MIN)];
      }
      if (isBoundary(STATE,NbrsVel::VZ_POS_BND) == 0) {
	 nbrBlock = BLOCK + P::vyblocks_ini*P::vxblocks_ini - P::vxblocks_ini; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT i=0; i<WID; ++i) nbrFlux[accIndex(i,ACCMAX,ACCMIN)] += dF[findex(i+1,MIN,MAX)];
      }
   }
   // Accumulate changes to +vy neighbour if it exists:
   if (isBoundary(STATE,NbrsVel::VY_POS_BND) == 0) {
      nbrBlock = nbrsVel[NbrsVel::VYPOS];
      nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) nbrFlux[accIndex(i,ACCMIN,k)] += dF[findex(i+1,MAX,k+1)];
      // Accumulate changes to (iv,jv+1,kv+-1) neighbours if they exist:
      if (isBoundary(STATE,NbrsVel::VZ_NEG_BND) == 0) {
	 nbrBlock = BLOCK - P::vyblocks_ini*P::vxblocks_ini + P::vxblocks_ini; // temp solution
      	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT i=0; i<WID; ++i) nbrFlux[accIndex(i,ACCMIN,ACCMAX)] += dF[findex(i+1,MAX,MIN)];
      }
      if (isBoundary(STATE,NbrsVel::VZ_POS_BND) == 0) {
	 nbrBlock = BLOCK + P::vyblocks_ini*P::vxblocks_ini + P::vxblocks_ini; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT i=0; i<WID; ++i) nbrFlux[accIndex(i,ACCMIN,ACCMIN)] += dF[findex(i+1,MAX,MAX)];
      }
   }
   // Accumulate changes to -vz neighbour if it exists:
   if (isBoundary(STATE,NbrsVel::VZ_NEG_BND) == 0) {
      nbrBlock = nbrsVel[NbrsVel::VZNEG];
      nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
      for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) nbrFlux[accIndex(i,j,ACCMAX)] += dF[findex(i+1,j+1,MIN)];
      // Accumulate changes to (iv+-1,jv,kv-1) neighbours if they exist:
      if (isBoundary(STATE,NbrsVel::VX_NEG_BND) == 0) {
	 nbrBlock = BLOCK - P::vyblocks_ini*P::vxblocks_ini - 1; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT j=0; j<WID; ++j) nbrFlux[accIndex(ACCMAX,j,ACCMAX)] += dF[findex(MIN,j+1,MIN)];
      }
      if (isBoundary(STATE,NbrsVel::VX_POS_BND) == 0) {
	 nbrBlock = BLOCK - P::vyblocks_ini*P::vxblocks_ini + 1; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT j=0; j<WID; ++j) nbrFlux[accIndex(ACCMIN,j,ACCMAX)] += dF[findex(MAX,j+1,MIN)];
      }
   }
   // Accumulate changes to +vz neighbour if it exists:
   if (isBoundary(STATE,NbrsVel::VZ_POS_BND) == 0) {
      nbrBlock = nbrsVel[NbrsVel::VZPOS];
      nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
      for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) nbrFlux[accIndex(i,j,ACCMIN)] += dF[findex(i+1,j+1,MAX)];
      // Accumulate changes to (iv+-1,jv,kv+1) neighbours if they exist:
      if (isBoundary(STATE,NbrsVel::VX_NEG_BND) == 0) {
	 nbrBlock = BLOCK + P::vyblocks_ini*P::vxblocks_ini - 1; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT j=0; j<WID; ++j) nbrFlux[accIndex(ACCMAX,j,ACCMIN)] += dF[findex(MIN,j+1,MAX)];
      }
      if (isBoundary(STATE,NbrsVel::VX_POS_BND) == 0) {
	 nbrBlock = BLOCK + P::vyblocks_ini*P::vxblocks_ini + 1; // temp solution
	 nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	 for (UINT j=0; j<WID; ++j) nbrFlux[accIndex(ACCMIN,j,ACCMIN)] += dF[findex(MAX,j+1,MAX)];
      }
   }
   
   // Accumulate changes to 8 corner neighbours:
   if (isBoundary(STATE,NbrsVel::VX_NEG_BND) == 0) {
      if (isBoundary(STATE,NbrsVel::VY_NEG_BND) == 0) {
	 if (isBoundary(STATE,NbrsVel::VZ_NEG_BND) == 0) { // (iv-1,jv-1,kv-1)
	    nbrBlock = BLOCK - P::vyblocks_ini*P::vxblocks_ini - P::vxblocks_ini - 1; // temp solution
	    nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	    nbrFlux[accIndex(ACCMAX,ACCMAX,ACCMAX)] += dF[findex(MIN,MIN,MIN)];
	 }
	 if (isBoundary(STATE,NbrsVel::VZ_POS_BND) == 0) { // (iv-1,jv-1,kv+1)
	    nbrBlock = BLOCK + P::vyblocks_ini*P::vxblocks_ini - P::vxblocks_ini - 1; // temp solution
	    nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	    nbrFlux[accIndex(ACCMAX,ACCMAX,ACCMIN)] += dF[findex(MIN,MIN,MAX)];
	 }
      }
      if (isBoundary(STATE,NbrsVel::VY_POS_BND) == 0) {
	 if (isBoundary(STATE,NbrsVel::VZ_NEG_BND) == 0) { // (iv-1,jv+1,kv-1)
	    nbrBlock = BLOCK - P::vyblocks_ini*P::vxblocks_ini + P::vxblocks_ini - 1; // temp solution
	    nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	    nbrFlux[accIndex(ACCMAX,ACCMIN,ACCMAX)] += dF[findex(MIN,MAX,MIN)];
	 } 
	 if (isBoundary(STATE,NbrsVel::VZ_POS_BND) == 0) { // (iv-1,jv+1,kv+1)
	    nbrBlock = BLOCK + P::vyblocks_ini*P::vxblocks_ini + P::vxblocks_ini - 1; // temp solution
	    nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	    nbrFlux[accIndex(ACCMAX,ACCMIN,ACCMIN)] += dF[findex(MIN,MAX,MAX)];
	 }
      }
   }
   if (isBoundary(STATE,NbrsVel::VX_POS_BND) == 0) {
      if (isBoundary(STATE,NbrsVel::VY_NEG_BND) == 0) {
	 if (isBoundary(STATE,NbrsVel::VZ_NEG_BND) == 0) { // (iv+1,jv-1,kv-1)
	    nbrBlock = BLOCK - P::vyblocks_ini*P::vxblocks_ini - P::vxblocks_ini + 1; // temp solution
	    nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	    nbrFlux[accIndex(ACCMIN,ACCMAX,ACCMAX)] += dF[findex(MAX,MIN,MIN)];
	 }
	 if (isBoundary(STATE,NbrsVel::VZ_POS_BND) == 0) { // (iv+1,jv-1,kv+1)
	    nbrBlock = BLOCK + P::vyblocks_ini*P::vxblocks_ini - P::vxblocks_ini + 1; // temp solution
	    nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	    nbrFlux[accIndex(ACCMIN,ACCMAX,ACCMIN)] += dF[findex(MAX,MIN,MAX)];
	 }
      }
      if (isBoundary(STATE,NbrsVel::VY_POS_BND) == 0) { 
	 if (isBoundary(STATE,NbrsVel::VZ_NEG_BND) == 0) { // (iv+1,jv+1,kv-1)
	    nbrBlock = BLOCK - P::vyblocks_ini*P::vxblocks_ini + P::vxblocks_ini + 1; // temp solution
	    nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	    nbrFlux[accIndex(ACCMIN,ACCMIN,ACCMAX)] += dF[findex(MAX,MAX,MIN)];
	 }
	 if (isBoundary(STATE,NbrsVel::VZ_POS_BND) == 0) { // (iv+1,jv+1,kv+1)
	    nbrBlock = BLOCK + P::vyblocks_ini*P::vxblocks_ini + P::vxblocks_ini + 1; // temp solution
	    nbrFlux  = flux + nbrBlock*SIZE_FLUXS;
	    nbrFlux[accIndex(ACCMIN,ACCMIN,ACCMIN)] += dF[findex(MAX,MAX,MAX)];
	 }
      }
   }
}

template<typename REAL,typename UINT> void fetchAllAverages(const UINT& BLOCK,REAL* const avgs,const REAL* const cpu_avgs,const UINT* const nbrsVel) {
   UINT nbrBlock;
   for (UINT i=0; i<8*WID3; ++i) avgs[i] = 0.0;
   const UINT STATE = nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE];
   
   // Copy averages from -x neighbour, or calculate using a boundary function:
   if (isBoundary(STATE,NbrsVel::VX_NEG_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<2; ++i) {
	 avgs[fullInd(i  ,j+2,k+2)] = 0.0; // BOUNDARY VALUE
      }
   } else {
      nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VXNEG];
      creal* const tmp = cpu_avgs + nbrBlock*SIZE_VELBLOCK; 
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	 #pragma ivdep
	 for (UINT i=0; i<2; ++i) {
	 //avgs[fullInd(i  ,j+2,k+2)] = cpu_avgs[nbrBlock*SIZE_VELBLOCK + accIndex(i+2,j,k)];
	 avgs[fullInd(i  ,j+2,k+2)] = tmp[accIndex(i+2,j,k)];
	 }
      }
   }
   // Copy averages from +x neighbour, or calculate using a boundary function:
   if (isBoundary(STATE,NbrsVel::VX_POS_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<2; ++i) {
	 avgs[fullInd(i+6,j+2,k+2)] = 0.0; // BOUNDARY VALUE
      }
   } else {
      nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VXPOS];
      creal* const tmp = cpu_avgs + nbrBlock*SIZE_VELBLOCK;
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	 #pragma ivdep
	 for (UINT i=0; i<2; ++i) {
	    //avgs[fullInd(i+6,j+2,k+2)] = cpu_avgs[nbrBlock*SIZE_VELBLOCK + accIndex(i  ,j,k)];
	    avgs[fullInd(i+6,j+2,k+2)] = tmp[accIndex(i  ,j,k)];
	 }
      }
   }
   // Copy averages from -y neighbour, or calculate using a boundary function:
   if (isBoundary(STATE,NbrsVel::VY_NEG_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<2; ++j) for (UINT i=0; i<WID; ++i) {
	 avgs[fullInd(i+2,j  ,k+2)] = 0.0; // BOUNDARY VALUE
      }
   } else {
      nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VYNEG];
      creal* const tmp = cpu_avgs + nbrBlock*SIZE_VELBLOCK;
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<2; ++j) {
	 #pragma ivdep
	 for (UINT i=0; i<WID; ++i) {
	    //avgs[fullInd(i+2,j  ,k+2)] = cpu_avgs[nbrBlock*SIZE_VELBLOCK + accIndex(i,j+2,k)];
	    avgs[fullInd(i+2,j  ,k+2)] = tmp[accIndex(i,j+2,k)];
	 }
      }
   }
   // Copy averages from +y neighbour, or calculate using a boundary function:
   if (isBoundary(STATE,NbrsVel::VY_POS_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<2; ++j) for (UINT i=0; i<WID; ++i) {
	 avgs[fullInd(i+2,j+6,k+2)] = 0.0; // BOUNDARY VALUE
      }
   } else {
      nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VYPOS];
      creal* const tmp = cpu_avgs + nbrBlock*SIZE_VELBLOCK;
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<2; ++j) {
	 #pragma ivdep
	 for (UINT i=0; i<WID; ++i) {
	    //avgs[fullInd(i+2,j+6,k+2)] = cpu_avgs[nbrBlock*SIZE_VELBLOCK + accIndex(i,j  ,k)];
	    avgs[fullInd(i+2,j+6,k+2)] = tmp[accIndex(i,j  ,k)];
	 }
      }
   }
   // Copy averages from -z neighbour, or calculate using a boundary function:
   if (isBoundary(STATE,NbrsVel::VZ_NEG_BND) > 0) {
      for (UINT k=0; k<2; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
	 avgs[fullInd(i+2,j+2,k  )] = 0.0; // BOUNDARY VALUE
      }
   } else {
      nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VZNEG];
      creal* const tmp = cpu_avgs + nbrBlock*SIZE_VELBLOCK;
      for (UINT k=0; k<2; ++k) for (UINT j=0; j<WID; ++j) {
	 #pragma ivdep
	 for (UINT i=0; i<WID; ++i) {
	    //avgs[fullInd(i+2,j+2,k  )] = cpu_avgs[nbrBlock*SIZE_VELBLOCK + accIndex(i,j,k+2)];
	    avgs[fullInd(i+2,j+2,k  )] = tmp[accIndex(i,j,k+2)];
	 }
      }
   }
   // Copy averages from +z neighbour, or calculate using a boundary function:
   if (isBoundary(STATE,NbrsVel::VZ_POS_BND) > 0) {
      for (UINT k=0; k<2; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
	 avgs[fullInd(i+2,j+2,k+6)] = 0.0; // BOUNDARY VALUE
      }
   } else {
      nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VZPOS];
      creal* const tmp = cpu_avgs + nbrBlock*SIZE_VELBLOCK;
      for (UINT k=0; k<2; ++k) for (UINT j=0; j<WID; ++j) {
	 #pragma ivdep
	 for (UINT i=0; i<WID; ++i) {
	    //avgs[fullInd(i+2,j+2,k+6)] = cpu_avgs[nbrBlock*SIZE_VELBLOCK + accIndex(i,j,k)];
	    avgs[fullInd(i+2,j+2,k+6)] = tmp[accIndex(i,j,k)];
	 }
      }
   }
   // Copy volume averages of this block:
   creal* const tmp = cpu_avgs + BLOCK*SIZE_VELBLOCK;
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
      #pragma ivdep
      for (UINT i=0; i<WID; ++i) {
	 //avgs[fullInd(i+2,j+2,k+2)] = cpu_avgs[BLOCK*SIZE_VELBLOCK + accIndex(i,j,k)];
	 avgs[fullInd(i+2,j+2,k+2)] = tmp[accIndex(i,j,k)];
      }
   }
}
   
template<typename REAL,typename UINT> void fetchAveragesX(const UINT& BLOCK,REAL* const avgs,const REAL* const cpu_avgs,const UINT* const nbrsVel) {
   // The size of array avgs is (5,4,4).
   const UINT XS=5;
   const UINT YS=4;
   const UINT YSXS = YS*XS;
   // Copy averages from -x neighbour, or calculate using a boundary function:
   if (isBoundary(nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE],NbrsVel::VX_NEG_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	 avgs[k*YSXS+j*XS] = 0.0; // BOUNDARY VALUE
      }
   } else {
      const UINT nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VXNEG];
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	 avgs[k*YSXS+j*XS] = cpu_avgs[nbrBlock*SIZE_VELBLOCK + accIndex(convert<UINT>(3),j,k)];
      }
   }
   // Copy volume averages of this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      avgs[k*YSXS+j*XS+(i+1)] = cpu_avgs[BLOCK*SIZE_VELBLOCK + accIndex(i,j,k)];
   }
}

template<typename REAL,typename UINT> void fetchAveragesY(const UINT& BLOCK,REAL* const avgs,const REAL* const cpu_avgs,const UINT* const nbrsVel) {
   // The size of array avgs (4,5,4).
   cuint XS=4;
   cuint YS=5;
   cuint YSXS = YS*XS;
   // Copy averages from -y neighbour, or calculate using a boundary function:
   if (isBoundary(nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE],NbrsVel::VY_NEG_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) {
	 avgs[k*YSXS+i] = 0.0; // BOUNDARY VALUE
      }
   } else {
      const UINT nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VYNEG];
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) {
	 avgs[k*YSXS+i] = cpu_avgs[nbrBlock*SIZE_VELBLOCK + accIndex(i,convert<UINT>(3),k)];
      }
   }
   // Copy volume averages of this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      avgs[k*YSXS+(j+1)*XS+i] = cpu_avgs[BLOCK*SIZE_VELBLOCK + accIndex(i,j,k)];
   }   
}
   
template<typename REAL,typename UINT> void fetchAveragesZ(const UINT& BLOCK,REAL* const avgs,const REAL* const cpu_avgs,const UINT* const nbrsVel) {   
   // The size of array avgs (4,4,5).
   const UINT XS=4;
   const UINT YS=4;
   const UINT YSXS=YS*XS;
   // Copy averages from -z neighbour, or calculate using a boundary function:
   if (isBoundary(nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE],NbrsVel::VZ_NEG_BND) > 0) {
      for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
	 avgs[j*XS+i] = 0.0; // BOUNDARY VALUE
      }
   } else {
      const UINT nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VZNEG];
      for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
	 avgs[j*XS+i] = cpu_avgs[nbrBlock*SIZE_VELBLOCK + accIndex(i,j,convert<UINT>(3))];
      }
   }
   // Copy volume averages of this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      avgs[(k+1)*YSXS+j*XS+i] = cpu_avgs[BLOCK*SIZE_VELBLOCK + accIndex(i,j,k)];
   }
}

template<typename REAL,typename UINT> void fetchDerivsX(const UINT& BLOCK,REAL* const d1x,const REAL* const cpu_d1x,const UINT* const nbrsVel) {
   // The size of array avgs (5,4,4).
   const UINT XS=5;
   const UINT YS=4;
   const UINT YSXS=YS*XS;
   // Copy derivatives from -x neighbour, or calculate using a boundary function:
   if (isBoundary(nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE],NbrsVel::VX_NEG_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	 d1x[k*YSXS+j*XS] = 0.0; // BOUNDARY VALUE
      }
   } else {
      const UINT nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VXNEG];
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	 d1x[k*YSXS+j*XS] = cpu_d1x[nbrBlock*SIZE_DERIV + accIndex(convert<UINT>(3),j,k)];
      }
   }
   // Copy derivatives for this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      d1x[k*YSXS+j*XS+(i+1)] = cpu_d1x[BLOCK*SIZE_DERIV + accIndex(i,j,k)];
   }
}

template<typename REAL,typename UINT> void fetchDerivsY(const UINT& BLOCK,REAL* const d1y,const REAL* const cpu_d1y,const UINT* const nbrsVel) {
   // The size of array avgs (4,5,4).
   const UINT XS=4;
   const UINT YS=5;
   const UINT YSXS=YS*XS;
   // Copy derivatives from -y neighbour, or calculate using a boundary function:
   if (isBoundary(nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE],NbrsVel::VY_NEG_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) {
	 d1y[k*YSXS+i] = 0.0; // BOUNDARY VALUE
      }
   } else {
      const UINT nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VYNEG];
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) {
	 d1y[k*YSXS+i] = cpu_d1y[nbrBlock*SIZE_DERIV + accIndex(i,convert<UINT>(3),k)];
      }
   }
   // Copy derivatives for this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      d1y[k*YSXS+(j+1)*XS+i] = cpu_d1y[BLOCK*SIZE_DERIV + accIndex(i,j,k)];
   }
}

template<typename REAL,typename UINT> void fetchDerivsZ(const UINT& BLOCK,REAL* const d1z,const REAL* const cpu_d1z,const UINT* const nbrsVel) {
   // The size of array avgs (4,4,5)
   const UINT XS=4;
   const UINT YS=4;
   const UINT YSXS=YS*XS;
   // Copy derivatives from -z neighbour, or calculate using a boundary function:
   if (isBoundary(nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE],NbrsVel::VZ_NEG_BND) > 0) {
      for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
	 d1z[j*XS+i] = 0.0; // BOUNDARY VALUE
      }
   } else {
      const UINT nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VZNEG];
      for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
	 d1z[j*XS+i] = cpu_d1z[nbrBlock*SIZE_DERIV + accIndex(i,j,convert<UINT>(3))];
      }
   }
   // Copy derivatives for this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      d1z[(k+1)*YSXS+j*XS+i] = cpu_d1z[BLOCK*SIZE_DERIV + accIndex(i,j,k)];
   }
}

template<typename REAL,typename UINT> void fetchFluxesX(const UINT& BLOCK,REAL* const flux,const REAL* const cpu_fx,const UINT* const nbrsVel) {
   // The size of array fx is (5,4,4):
   const UINT XS = 5;
   const UINT YS = 4;
   const UINT YSXS = YS*XS;
   // Fetch fluxes from +x neighbour, or calculate using a boundary function:
   if (isBoundary(nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE],NbrsVel::VX_POS_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	 flux[k*YSXS+j*XS+4] = 0.0; // BOUNDARY VALUE
      }
   } else {
      const UINT nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VXPOS];
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	 flux[k*YSXS+j*XS+4] = cpu_fx[nbrBlock*SIZE_FLUXS + accIndex(convert<UINT>(0),j,k)];
      }
   }
   // Fetch the fluxes of this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      flux[k*YSXS+j*XS+i] = cpu_fx[BLOCK*SIZE_FLUXS + accIndex(i,j,k)];
   }
}

template<typename REAL,typename UINT> void fetchFluxesY(const UINT& BLOCK,REAL* const flux,const REAL* const cpu_fy,const UINT* const nbrsVel) {
   // The size of array fy is (4,5,4):
   const UINT XS = 4;
   const UINT YS = 5;
   const UINT YSXS = YS*XS;
   // Fetch fluxes from +y neighbour, or calculate using a boundary function:
   if (isBoundary(nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE],NbrsVel::VY_POS_BND) > 0) {
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) {
	 flux[k*YSXS+4*XS+i] = 0.0; // BOUNDARY VALUE
      }
   } else {
      const UINT nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VYPOS];
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) {
	 flux[k*YSXS+4*XS+i] = cpu_fy[nbrBlock*SIZE_FLUXS + accIndex(i,convert<uint>(0),k)];
      }
   }
   // Fetch the fluxes of this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      flux[k*YSXS+j*XS+i] = cpu_fy[BLOCK*SIZE_FLUXS + accIndex(i,j,k)];
   }
}
   
template<typename REAL,typename UINT> void fetchFluxesZ(const UINT& BLOCK,REAL* const flux,const REAL* const cpu_fz,const UINT* const nbrsVel) {
   // The size of array fz is (4,4,5):
   const UINT XS = 4;
   const UINT YS = 4;
   const UINT YSXS = YS*XS;
   // Fetch fluxes from +z neighbour, or calculate using a boundary function:
   if (isBoundary(nbrsVel[BLOCK*SIZE_NBRS_VEL+NbrsVel::STATE],NbrsVel::VZ_POS_BND) > 0) {
      for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
	 flux[4*YSXS+j*XS+i] = 0.0; // BOUNDARY VALUE
      }
   } else {
      const UINT nbrBlock = nbrsVel[BLOCK*SIZE_NBRS_VEL + NbrsVel::VZPOS];
      for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
	 flux[4*YSXS+j*XS+i] = cpu_fz[nbrBlock*SIZE_FLUXS + accIndex(i,j,convert<UINT>(0))];
      }
   }
   // Fetch the fluxes of this block:
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      flux[k*YSXS+j*XS+i] = cpu_fz[BLOCK*SIZE_FLUXS + accIndex(i,j,k)];
   }
}

template<typename REAL,typename UINT,typename CELL> void cpu_clearVelFluxes(CELL& cell,const UINT& BLOCK) {
   for (UINT i=0; i<SIZE_FLUXS; ++i) cell.cpu_fx[BLOCK*SIZE_FLUXS + i] = 0.0;
   /*
   REAL* f = cell.cpu_fx + BLOCK*SIZE_FLUXS;   
   for (UINT i=0; i<SIZE_FLUXS; ++i) f[i] = 0.0;
   
   f = cell.cpu_fy + BLOCK*SIZE_FLUXS;
   for (UINT i=0; i<SIZE_FLUXS; ++i) f[i] = 0.0;
   
   f = cell.cpu_fz + BLOCK*SIZE_FLUXS;
   for (UINT i=0; i<SIZE_FLUXS; ++i) f[i] = 0.0;
    */
}

template<typename REAL,typename UINT,typename CELL> void cpu_calcVelFluxes(CELL& cell,const UINT& BLOCK,const REAL& DT,creal* const accmat) {
   
   const REAL EPSILON = 1.0e-15;

   // Allocate temporary array in which local dF changes are calculated:
   // F is the distribution function, dFdt is its change over timestep DT
   Real dFdt[216];
   for (UINT i=0; i<216; ++i) dFdt[i] = 0.0;
   
   const REAL* const cellParams = cell.cpu_cellParams;
   const REAL* const blockParams = cell.cpu_blockParams + BLOCK*SIZE_BLOCKPARAMS; 
   REAL avgs[8*WID3];
   fetchAllAverages(BLOCK,avgs,cell.cpu_avgs,cell.cpu_nbrsVel);
   
   REAL AX,AY,AZ,VX,VY,VZ;
   REAL dF,theta;
   REAL AX_NEG,AX_POS,AY_NEG,AY_POS,AZ_NEG,AZ_POS;
   REAL INCR_WAVE,CORR_WAVE;
   REAL theta_up,theta_lo;
   const REAL DVX = blockParams[BlockParams::DVX];
   const REAL DVY = blockParams[BlockParams::DVY];
   const REAL DVZ = blockParams[BlockParams::DVZ];
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      // ***********************************
      // ***** INTERFACE BETWEEN I,I-1 *****
      // ***********************************
      
      // Calculate acceleration at face (i-1/2,j,k):
      VX = blockParams[BlockParams::VXCRD];
      VY = blockParams[BlockParams::VYCRD] + (j+0.5)*DVY;
      VZ = blockParams[BlockParams::VZCRD] + (k+0.5)*DVZ;
      //AX = Parameters::q_per_m * (VY*cellParams[CellParams::BZ] - VZ*cellParams[CellParams::BY]);
      //AY = Parameters::q_per_m * (VZ*cellParams[CellParams::BX] - VX*cellParams[CellParams::BZ]);
      //AZ = Parameters::q_per_m * (VX*cellParams[CellParams::BY] - VY*cellParams[CellParams::BX]);
      AX = accmat[0]*VX + accmat[1]*VY + accmat[2]*VZ;
      AY = accmat[3]*VX + accmat[4]*VY + accmat[5]*VZ;
      AZ = accmat[6]*VX + accmat[7]*VY + accmat[8]*VZ;
      
      //AX = -1.0;
      //AY = -1.0;
      //AZ = -1.0;
      
      AX_NEG = std::min(convert<REAL>(0.0),AX);
      AX_POS = std::max(convert<REAL>(0.0),AX);
      AY_NEG = std::min(convert<REAL>(0.0),AY);
      AY_POS = std::max(convert<REAL>(0.0),AY);
      AZ_NEG = std::min(convert<REAL>(0.0),AZ);
      AZ_POS = std::max(convert<REAL>(0.0),AZ);

      // Calculate slope-limited x-derivative of F:
      if (AX > 0.0) theta_up = avgs[fullInd(i+1,j+2,k+2)] - avgs[fullInd(i  ,j+2,k+2)];
      else theta_up = avgs[fullInd(i+3,j+2,k+2)] - avgs[fullInd(i+2,j+2,k+2)];
      theta_lo = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+1,j+2,k+2)] + EPSILON;
      theta = limiter(theta_up/theta_lo);
      
      // Donor cell upwind method, Fx updates:
      dF = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+1,j+2,k+2)]; // F(i,j,k) - F(i-1,j,k), jump in F
      INCR_WAVE = AX_POS*avgs[fullInd(i+1,j+2,k+2)] + AX_NEG*avgs[fullInd(i+2,j+2,k+2)];// + 0.5*fabs(AX)*(1.0-fabs(AX)*DT/DVX)*dF*theta;
      CORR_WAVE = 0.5*fabs(AX)*(1.0-fabs(AX)*DT/DVX)*dF*theta;
      //CORR_WAVE = 0.0;
      cell.cpu_d1x[BLOCK*SIZE_DERIV+accIndex(i,j,k)] = theta;
      cell.cpu_d2x[BLOCK*SIZE_DERIV+accIndex(i,j,k)] = dF*theta;

      dFdt[findex(i+1,j+1,k+1)] += (INCR_WAVE+CORR_WAVE)*DT/DVX;               // Positive change to i
      dFdt[findex(i  ,j+1,k+1)] -= (INCR_WAVE+CORR_WAVE)*DT/DVX;               // Negative change to i-1

      INCR_WAVE = -0.5*DT/DVX*dF;
      CORR_WAVE *= DT/DVX;
      //CORR_WAVE = 0.0;
      // Transverse propagation, Fy updates:
      dFdt[findex(i+1,j+2,k+1)] += (INCR_WAVE*AX_POS*AY_POS + CORR_WAVE*AY_POS)*DT/DVY; // Fy: i,j+1
      dFdt[findex(i+1,j+1,k+1)] -= (INCR_WAVE*AX_POS*AY_POS + CORR_WAVE*AY_POS)*DT/DVY; 
      dFdt[findex(i+1,j+1,k+1)] += (INCR_WAVE*AX_POS*AY_NEG + CORR_WAVE*AY_NEG)*DT/DVY; // Fy: i,j
      dFdt[findex(i+1,j  ,k+1)] -= (INCR_WAVE*AX_POS*AY_NEG + CORR_WAVE*AY_NEG)*DT/DVY;
      dFdt[findex(i  ,j+2,k+1)] += (INCR_WAVE*AX_NEG*AY_POS - CORR_WAVE*AY_POS)*DT/DVY; // Fy: i-1,j+1
      dFdt[findex(i  ,j+1,k+1)] -= (INCR_WAVE*AX_NEG*AY_POS - CORR_WAVE*AY_POS)*DT/DVY;
      dFdt[findex(i  ,j+1,k+1)] += (INCR_WAVE*AX_NEG*AY_NEG - CORR_WAVE*AY_NEG)*DT/DVY; // Fy: i,j
      dFdt[findex(i  ,j  ,k+1)] -= (INCR_WAVE*AX_NEG*AY_NEG - CORR_WAVE*AY_NEG)*DT/DVY;

      // Transverse propagation, Fz updates:
      dFdt[findex(i+1,j+1,k+2)] += (INCR_WAVE*AX_POS*AZ_POS + CORR_WAVE*AZ_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k+1)] -= (INCR_WAVE*AX_POS*AZ_POS + CORR_WAVE*AZ_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k+1)] += (INCR_WAVE*AX_POS*AZ_NEG + CORR_WAVE*AZ_NEG)*DT/DVZ;
      dFdt[findex(i+1,j+1,k  )] -= (INCR_WAVE*AX_POS*AZ_NEG + CORR_WAVE*AZ_NEG)*DT/DVZ;
      dFdt[findex(i  ,j+1,k+2)] += (INCR_WAVE*AX_NEG*AZ_POS - CORR_WAVE*AZ_POS)*DT/DVZ;
      dFdt[findex(i  ,j+1,k+1)] -= (INCR_WAVE*AX_NEG*AZ_POS - CORR_WAVE*AZ_POS)*DT/DVZ;
      dFdt[findex(i  ,j+1,k+1)] += (INCR_WAVE*AX_NEG*AZ_NEG - CORR_WAVE*AZ_NEG)*DT/DVZ;
      dFdt[findex(i  ,j+1,k  )] -= (INCR_WAVE*AX_NEG*AZ_NEG - CORR_WAVE*AZ_NEG)*DT/DVZ;

      INCR_WAVE = DT*DT/DVX/DVZ*dF/6.0;

      // Double transverse propagation, Fy updates:
      dFdt[findex(i+1,j+2,k+1)] += INCR_WAVE*AX_POS*AY_POS*fabs(AZ)*DT/DVY; // Ax > 0
      dFdt[findex(i+1,j+1,k+1)] += INCR_WAVE*AX_POS*AY_NEG*fabs(AZ)*DT/DVY;
      dFdt[findex(i  ,j+2,k+1)] += INCR_WAVE*AX_NEG*AY_POS*fabs(AZ)*DT/DVY; // Ax < 0
      dFdt[findex(i  ,j+1,k+1)] += INCR_WAVE*AX_NEG*AY_NEG*fabs(AZ)*DT/DVY;
      
      dFdt[findex(i+1,j+2,k+2)] -= INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVY; // Ax > 0
      dFdt[findex(i+1,j+1,k+2)] -= INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i+1,j+2,k  )] += INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i+1,j+1,k  )] += INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVY;
      dFdt[findex(i  ,j+2,k+2)] -= INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVY; // Ax < 0
      dFdt[findex(i  ,j+1,k+2)] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i  ,j+2,k  )] += INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i  ,j+1,k  )] += INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVY;
      
      dFdt[findex(i+1,j+1,k+1)] -= INCR_WAVE*AX_POS*AY_POS*fabs(AZ)*DT/DVY; // Ax > 0
      dFdt[findex(i+1,j  ,k+1)] -= INCR_WAVE*AX_POS*AY_NEG*fabs(AZ)*DT/DVY;
      dFdt[findex(i  ,j+1,k+1)] -= INCR_WAVE*AX_NEG*AY_POS*fabs(AZ)*DT/DVY; // Ax < 0
      dFdt[findex(i  ,j  ,k+1)] -= INCR_WAVE*AX_NEG*AY_NEG*fabs(AZ)*DT/DVY;
      
      dFdt[findex(i+1,j+1,k+2)] += INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVY; // Ax > 0
      dFdt[findex(i+1,j  ,k+2)] += INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i+1,j+1,k  )] -= INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i+1,j  ,k  )] -= INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVY;
      dFdt[findex(i  ,j+1,k+2)] += INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVY; // Ax < 0
      dFdt[findex(i  ,j  ,k+2)] += INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i  ,j+1,k  )] -= INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i  ,j  ,k  )] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVY;
      
      INCR_WAVE = DT*DT/DVX/DVY*dF/6.0;
      
      // Double transverse propagation, Fz updates:
      dFdt[findex(i+1,j+1,k+2)] += INCR_WAVE*AX_POS*fabs(AY)*AZ_POS*DT/DVZ; // Ax > 0
      dFdt[findex(i+1,j+1,k+1)] += INCR_WAVE*AX_POS*fabs(AY)*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j+1,k+2)] += INCR_WAVE*AX_NEG*fabs(AY)*AZ_POS*DT/DVZ; // Ax < 0
      dFdt[findex(i  ,j+1,k+1)] += INCR_WAVE*AX_NEG*fabs(AY)*AZ_NEG*DT/DVZ;
      
      dFdt[findex(i+1,j+2,k+2)] -= INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVZ; // Ax > 0
      dFdt[findex(i+1,j+2,k+1)] -= INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i+1,j  ,k+2)] += INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i+1,j  ,k+1)] += INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j+2,k+2)] -= INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVZ; // Ax < 0
      dFdt[findex(i  ,j+2,k+1)] -= INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j  ,k+2)] += INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i  ,j  ,k+1)] += INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVZ;
      
      
      
      
      dFdt[findex(i+1,j+1,k+1)] -= INCR_WAVE*AX_POS*fabs(AY)*AZ_POS*DT/DVZ; // Ax > 0
      dFdt[findex(i+1,j+1,k  )] -= INCR_WAVE*AX_POS*fabs(AY)*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j+1,k+1)] -= INCR_WAVE*AX_NEG*fabs(AY)*AZ_POS*DT/DVZ; // Ax < 0
      dFdt[findex(i  ,j+1,k  )] -= INCR_WAVE*AX_NEG*fabs(AY)*AZ_NEG*DT/DVZ;
      
      dFdt[findex(i+1,j+2,k+1)] += INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVZ; // Ax > 0
      dFdt[findex(i+1,j+2,k  )] += INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i+1,j  ,k+1)] -= INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i+1,j  ,k  )] -= INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j+2,k+1)] += INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVZ; // Ax < 0
      dFdt[findex(i  ,j+2,k  )] += INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j  ,k+1)] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i  ,j  ,k  )] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVZ;
      
      /*
      // Double transverse y/z propagations:
      Fy[(k+2)*YSXS+(j+2)*XS+i+1] -= AX_POS*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AY_POS*AZ_POS;
      Fy[(k+2)*YSXS+(j+2)*XS+i  ] -= AX_NEG*AY_POS*AZ_POS*DT*DT*dF/6.0 + CORR_WAVE*AY_POS*AZ_POS;
      Fy[(k+2)*YSXS+(j+1)*XS+i+1] -= AX_POS*AY_NEG*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AY_NEG*AZ_POS;
      Fy[(k+2)*YSXS+(j+1)*XS+i  ] -= AX_NEG*AY_NEG*AZ_POS*DT*DT*dF/6.0 + CORR_WAVE*AY_NEG*AZ_POS;
      Fy[(k  )*YSXS+(j+2)*XS+i+1] += AX_POS*AY_POS*AZ_NEG*DT*DT*dF/6.0 - CORR_WAVE*AY_POS*AZ_NEG;
      Fy[(k  )*YSXS+(j+2)*XS+i  ] += AX_NEG*AY_POS*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AY_POS*AZ_NEG;
      Fy[(k  )*YSXS+(j+1)*XS+i+1] += AX_POS*AY_NEG*AZ_NEG*DT*DT*dF/6.0 - CORR_WAVE*AY_NEG*AZ_NEG;
      Fy[(k  )*YSXS+(j+1)*XS+i  ] += AX_NEG*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AY_NEG*AZ_NEG;
      
      Fz[(k+2)*YSXS+(j+2)*XS+i+1] -= AX_POS*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AY_POS*AZ_POS;
      Fz[(k+2)*YSXS+(j+2)*XS+i  ] -= AX_NEG*AY_POS*AZ_POS*DT*DT*dF/6.0 + CORR_WAVE*AY_POS*AZ_POS;
      Fz[(k+2)*YSXS+(j  )*XS+i+1] -= AX_POS*AY_NEG*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AY_NEG*AZ_POS;
      Fz[(k+2)*YSXS+(j  )*XS+i  ] -= AX_NEG*AY_NEG*AZ_POS*DT*DT*dF/6.0 + CORR_WAVE*AY_NEG*AZ_POS;
      Fz[(k+1)*YSXS+(j+2)*XS+i+1] -= AX_POS*AY_POS*AZ_NEG*DT*DT*dF/6.0 - CORR_WAVE*AY_POS*AZ_NEG;
      Fz[(k+1)*YSXS+(j+2)*XS+i  ] -= AX_NEG*AY_POS*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AY_POS*AZ_NEG;
      Fz[(k+1)*YSXS+(j  )*XS+i+1] -= AX_POS*AY_NEG*AZ_NEG*DT*DT*dF/6.0 - CORR_WAVE*AY_NEG*AZ_NEG;
      Fz[(k+1)*YSXS+(j  )*XS+i  ] -= AX_NEG*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AY_NEG*AZ_NEG;
      */
      // ***********************************
      // ***** INTERFACE BETWEEN J,J-1 *****
      // ***********************************

      // Calculate acceleration at face (i,j-1/2,k):
      VX = blockParams[BlockParams::VXCRD] + (i+0.5)*DVX;
      VY = blockParams[BlockParams::VYCRD];
      VZ = blockParams[BlockParams::VZCRD] + (k+0.5)*DVZ;
      //AX = Parameters::q_per_m * (VY*cellParams[CellParams::BZ] - VZ*cellParams[CellParams::BY]);
      //AY = Parameters::q_per_m * (VZ*cellParams[CellParams::BX] - VX*cellParams[CellParams::BZ]);
      //AZ = Parameters::q_per_m * (VX*cellParams[CellParams::BY] - VY*cellParams[CellParams::BX]);
      AX = accmat[0]*VX + accmat[1]*VY + accmat[2]*VZ;
      AY = accmat[3]*VX + accmat[4]*VY + accmat[5]*VZ;
      AZ = accmat[6]*VX + accmat[7]*VY + accmat[8]*VZ;

      AX_NEG = std::min(convert<REAL>(0.0),AX);
      AX_POS = std::max(convert<REAL>(0.0),AX);
      AY_NEG = std::min(convert<REAL>(0.0),AY);
      AY_POS = std::max(convert<REAL>(0.0),AY);
      AZ_NEG = std::min(convert<REAL>(0.0),AZ);
      AZ_POS = std::max(convert<REAL>(0.0),AZ);

      // Calculate slope-limited y-derivative of F:
      if (AY > 0.0) theta_up = avgs[fullInd(i+2,j+1,k+2)] - avgs[fullInd(i+2,j  ,k+2)];
      else theta_up = avgs[fullInd(i+2,j+3,k+2)] - avgs[fullInd(i+2,j+2,k+2)];
      theta_lo = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+2,j+1,k+2)] + EPSILON;
      theta = limiter(theta_up/theta_lo);
 
      // Donor cell upwind method, Fy updates:
      dF = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+2,j+1,k+2)]; // F(i,j,k) - F(i-1,j,k), jump in F
      INCR_WAVE = AY_POS*avgs[fullInd(i+2,j+1,k+2)] + AY_NEG*avgs[fullInd(i+2,j+2,k+2)];// + 0.5*fabs(AY)*(1.0-fabs(AY)*DT/DVY)*dF*theta;
      CORR_WAVE = 0.5*fabs(AY)*(1.0-fabs(AY)*DT/DVY)*dF*theta;
      cell.cpu_d1y[BLOCK*SIZE_DERIV+accIndex(i,j,k)] = theta;
      cell.cpu_d2y[BLOCK*SIZE_DERIV+accIndex(i,j,k)] = dF*theta;
      //CORR_WAVE = 0.0;
      dFdt[findex(i+1,j+1,k+1)] += (INCR_WAVE+CORR_WAVE)*DT/DVY;               // Positive change to j
      dFdt[findex(i+1,j  ,k+1)] -= (INCR_WAVE+CORR_WAVE)*DT/DVY;               // Negative change to j-1

      // Transverse propagation, Fx updates:
      INCR_WAVE = -0.5*DT/DVY*dF;
      CORR_WAVE *= DT/DVY;
      cell.cpu_fy[BLOCK*SIZE_FLUXS+accIndex(i,j,k)] = INCR_WAVE*fabs(AX)*fabs(AY) + CORR_WAVE*fabs(AX);
      //CORR_WAVE = 0.0;
      dFdt[findex(i+2,j+1,k+1)] += (INCR_WAVE*AX_POS*AY_POS + CORR_WAVE*AX_POS)*DT/DVX; // Positive change to i
      dFdt[findex(i+1,j+1,k+1)] -= (INCR_WAVE*AX_POS*AY_POS + CORR_WAVE*AX_POS)*DT/DVX; // Negative change to i-1
      dFdt[findex(i+1,j+1,k+1)] += (INCR_WAVE*AX_NEG*AY_POS + CORR_WAVE*AX_NEG)*DT/DVX;
      dFdt[findex(i  ,j+1,k+1)] -= (INCR_WAVE*AX_NEG*AY_POS + CORR_WAVE*AX_NEG)*DT/DVX;
      dFdt[findex(i+2,j  ,k+1)] += (INCR_WAVE*AX_POS*AY_NEG - CORR_WAVE*AX_POS)*DT/DVX;
      dFdt[findex(i+1,j  ,k+1)] -= (INCR_WAVE*AX_POS*AY_NEG - CORR_WAVE*AX_POS)*DT/DVX;
      dFdt[findex(i+1,j  ,k+1)] += (INCR_WAVE*AX_NEG*AY_NEG - CORR_WAVE*AX_NEG)*DT/DVX;
      dFdt[findex(i  ,j  ,k+1)] -= (INCR_WAVE*AX_NEG*AY_NEG - CORR_WAVE*AX_NEG)*DT/DVX;

      // Transverse propagation, Fz updates:
      dFdt[findex(i+1,j+1,k+2)] += (INCR_WAVE*AY_POS*AZ_POS + CORR_WAVE*AZ_POS)*DT/DVZ; // Positive chnage to k
      dFdt[findex(i+1,j+1,k+1)] -= (INCR_WAVE*AY_POS*AZ_POS + CORR_WAVE*AZ_POS)*DT/DVZ; // Negative change to k-1
      dFdt[findex(i+1,j+1,k+1)] += (INCR_WAVE*AY_POS*AZ_NEG + CORR_WAVE*AZ_NEG)*DT/DVZ;
      dFdt[findex(i+1,j+1,k  )] -= (INCR_WAVE*AY_POS*AZ_NEG + CORR_WAVE*AZ_NEG)*DT/DVZ;
      dFdt[findex(i+1,j  ,k+2)] += (INCR_WAVE*AY_NEG*AZ_POS - CORR_WAVE*AZ_POS)*DT/DVZ;
      dFdt[findex(i+1,j  ,k+1)] -= (INCR_WAVE*AY_NEG*AZ_POS - CORR_WAVE*AZ_POS)*DT/DVZ;
      dFdt[findex(i+1,j  ,k+1)] += (INCR_WAVE*AY_NEG*AZ_NEG - CORR_WAVE*AZ_NEG)*DT/DVZ;
      dFdt[findex(i+1,j  ,k  )] -= (INCR_WAVE*AY_NEG*AZ_NEG - CORR_WAVE*AZ_NEG)*DT/DVZ;

      INCR_WAVE = DT*DT/DVY/DVZ*dF/6.0;
      
      // Double transverse propagation, Fx updates:
      dFdt[findex(i+2,j+1,k+1)] += INCR_WAVE*AX_POS*AY_POS*fabs(AZ)*DT/DVX;
      dFdt[findex(i+1,j+1,k+1)] += INCR_WAVE*AX_NEG*AY_POS*fabs(AZ)*DT/DVX;
      dFdt[findex(i+2,j  ,k+1)] += INCR_WAVE*AX_POS*AY_NEG*fabs(AZ)*DT/DVX;
      dFdt[findex(i+1,j  ,k+1)] += INCR_WAVE*AX_NEG*AY_NEG*fabs(AZ)*DT/DVX;
      
      dFdt[findex(i+2,j+1,k+2)] -= INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j+1,k+2)] -= INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVX;
      dFdt[findex(i+2,j  ,k+2)] -= INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j  ,k+2)] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVX;
      dFdt[findex(i+2,j+1,k  )] += INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVX;
      dFdt[findex(i+1,j+1,k  )] += INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVX;
      dFdt[findex(i+2,j  ,k  )] += INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVX;
      dFdt[findex(i+1,j  ,k  )] += INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVX;
      
      dFdt[findex(i+1,j+1,k+1)] -= INCR_WAVE*AX_POS*AY_POS*fabs(AZ)*DT/DVX;
      dFdt[findex(i  ,j+1,k+1)] -= INCR_WAVE*AX_NEG*AY_POS*fabs(AZ)*DT/DVX;
      dFdt[findex(i+1,j  ,k+1)] -= INCR_WAVE*AX_POS*AY_NEG*fabs(AZ)*DT/DVX;
      dFdt[findex(i  ,j  ,k+1)] -= INCR_WAVE*AX_NEG*AY_NEG*fabs(AZ)*DT/DVX;
      
      dFdt[findex(i+1,j+1,k+2)] += INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVX;
      dFdt[findex(i  ,j+1,k+2)] += INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j  ,k+2)] += INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVX;
      dFdt[findex(i  ,j  ,k+2)] += INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j+1,k  )] -= INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVX;
      dFdt[findex(i  ,j+1,k  )] -= INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVX;
      dFdt[findex(i+1,j  ,k  )] -= INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVX;
      dFdt[findex(i  ,j  ,k  )] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVX;
      
      INCR_WAVE = DT*DT/DVX/DVY*dF/6.0;
      
      // Double transverse propagation, Fz updates:
      dFdt[findex(i+1,j+1,k+2)] += INCR_WAVE*fabs(AX)*AY_POS*AZ_POS*DT/DVZ;
      dFdt[findex(i+1,j  ,k+2)] += INCR_WAVE*fabs(AX)*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i+1,j+1,k+1)] += INCR_WAVE*fabs(AX)*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i+1,j  ,k+1)] += INCR_WAVE*fabs(AX)*AY_NEG*AZ_NEG*DT/DVZ;
      
      dFdt[findex(i+2,j+1,k+2)] -= INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVZ;
      dFdt[findex(i  ,j+1,k+2)] += INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVZ;
      dFdt[findex(i+2,j  ,k+2)] -= INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i  ,j  ,k+2)] += INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i+2,j+1,k+1)] -= INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j+1,k+1)] += INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i+2,j  ,k+1)] -= INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j  ,k+1)] += INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVZ;
      
      dFdt[findex(i+1,j+1,k+1)] -= INCR_WAVE*fabs(AX)*AY_POS*AZ_POS*DT/DVZ;
      dFdt[findex(i+1,j  ,k+1)] -= INCR_WAVE*fabs(AX)*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i+1,j+1,k  )] -= INCR_WAVE*fabs(AX)*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i+1,j  ,k  )] -= INCR_WAVE*fabs(AX)*AY_NEG*AZ_NEG*DT/DVZ;
      
      dFdt[findex(i+2,j+1,k+1)] += INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVZ;
      dFdt[findex(i  ,j+1,k+1)] -= INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVZ;
      dFdt[findex(i+2,j  ,k+1)] += INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i  ,j  ,k+1)] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVZ;
      dFdt[findex(i+2,j+1,k  )] += INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j+1,k  )] -= INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVZ;
      dFdt[findex(i+2,j  ,k  )] += INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVZ;
      dFdt[findex(i  ,j  ,k  )] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVZ;
      
      /*
      // Double transverse x/z propagations:
      Fx[(k+2)*YSXS+(j+1)*XS+i+2] -= AX_POS*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_POS*AZ_POS;
      Fx[(k+2)*YSXS+(j  )*XS+i+2] -= AX_POS*AY_NEG*AZ_POS*DT*DT*dF/6.0 + CORR_WAVE*AX_POS*AZ_POS;
      Fx[(k+2)*YSXS+(j+1)*XS+i+1] -= AX_NEG*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_NEG*AZ_POS;
      Fx[(k+2)*YSXS+(j  )*XS+i+1] -= AX_NEG*AY_NEG*AZ_POS*DT*DT*dF/6.0 + CORR_WAVE*AX_NEG*AZ_POS;
      Fx[(k  )*YSXS+(j+1)*XS+i+2] += AX_POS*AY_POS*AZ_NEG*DT*DT*dF/6.0 - CORR_WAVE*AX_POS*AZ_NEG;
      Fx[(k  )*YSXS+(j  )*XS+i+2] += AX_POS*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_POS*AZ_NEG;
      Fx[(k  )*YSXS+(j+1)*XS+i+1] += AX_NEG*AY_POS*AZ_NEG*DT*DT*dF/6.0 - CORR_WAVE*AX_NEG*AZ_NEG;
      Fx[(k  )*YSXS+(j  )*XS+i+1] += AX_NEG*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_NEG*AZ_NEG;
      
      Fz[(k+2)*YSXS+(j+1)*XS+i+2] -= AX_POS*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_POS*AZ_POS;
      Fz[(k+2)*YSXS+(j  )*XS+i+2] -= AX_POS*AY_NEG*AZ_POS*DT*DT*dF/6.0 + CORR_WAVE*AX_POS*AZ_POS;
      Fz[(k+2)*YSXS+(j+1)*XS+i  ] += AX_NEG*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_NEG*AZ_POS;
      Fz[(k+2)*YSXS+(j  )*XS+i  ] += AX_NEG*AY_NEG*AZ_POS*DT*DT*dF/6.0 + CORR_WAVE*AX_NEG*AZ_POS;
      Fz[(k+1)*YSXS+(j+1)*XS+i+2] -= AX_POS*AY_POS*AZ_NEG*DT*DT*dF/6.0 - CORR_WAVE*AX_POS*AZ_NEG;
      Fz[(k+1)*YSXS+(j  )*XS+i+2] -= AX_POS*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_POS*AZ_NEG;
      Fz[(k+1)*YSXS+(j+1)*XS+i  ] += AX_NEG*AY_POS*AZ_NEG*DT*DT*dF/6.0 - CORR_WAVE*AX_NEG*AZ_NEG;
      Fz[(k+1)*YSXS+(j  )*XS+i  ] += AX_NEG*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_NEG*AZ_NEG;
      */
      // ***********************************
      // ***** INTERFACE BETWEEN K,K-1 *****
      // ***********************************

      // Calculate acceleration at face (i,j,k-1/2):
      VX = blockParams[BlockParams::VXCRD] + (i+0.5)*blockParams[BlockParams::DVX];
      VY = blockParams[BlockParams::VYCRD] + (j+0.5)*blockParams[BlockParams::DVY];
      VZ = blockParams[BlockParams::VZCRD];
      //AX = Parameters::q_per_m * (VY*cellParams[CellParams::BZ] - VZ*cellParams[CellParams::BY]);
      //AY = Parameters::q_per_m * (VZ*cellParams[CellParams::BX] - VX*cellParams[CellParams::BZ]);
      //AZ = Parameters::q_per_m * (VX*cellParams[CellParams::BY] - VY*cellParams[CellParams::BX]);
      AX = accmat[0]*VX + accmat[1]*VY + accmat[2]*VZ;
      AY = accmat[3]*VX + accmat[4]*VY + accmat[5]*VZ;
      AZ = accmat[6]*VX + accmat[7]*VY + accmat[8]*VZ;

      AX_NEG = std::min(convert<REAL>(0.0),AX);
      AX_POS = std::max(convert<REAL>(0.0),AX);
      AY_NEG = std::min(convert<REAL>(0.0),AY);
      AY_POS = std::max(convert<REAL>(0.0),AY);
      AZ_NEG = std::min(convert<REAL>(0.0),AZ);
      AZ_POS = std::max(convert<REAL>(0.0),AZ);
      
      // Calculate slope-limited y-derivative of F:
      if (AZ > 0.0) theta_up = avgs[fullInd(i+2,j+2,k+1)] - avgs[fullInd(i+2,j+2,k  )];
      else theta_up = avgs[fullInd(i+2,j+2,k+3)] - avgs[fullInd(i+2,j+2,k+2)];
      theta_lo = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+2,j+2,k+1)] + EPSILON;
      theta = limiter(theta_up/theta_lo);

      // Donor Cell Upwind methods, Fz updates:
      dF = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+2,j+2,k+1)]; // F(i,j,k) - F(i,j,k-1), jump in F
      INCR_WAVE = AZ_POS*avgs[fullInd(i+2,j+2,k+1)] + AZ_NEG*avgs[fullInd(i+2,j+2,k+2)];
      CORR_WAVE = 0.5*fabs(AZ)*(1.0-fabs(AZ)*DT/DVZ)*dF*theta;
      //CORR_WAVE = 0.0;
      cell.cpu_d1z[BLOCK*SIZE_DERIV+accIndex(i,j,k)] = theta;
      cell.cpu_d2z[BLOCK*SIZE_DERIV+accIndex(i,j,k)] = dF*theta;
      
      dFdt[findex(i+1,j+1,k+1)] += (INCR_WAVE+CORR_WAVE)*DT/DVZ;               // Positive change to k
      dFdt[findex(i+1,j+1,k  )] -= (INCR_WAVE+CORR_WAVE)*DT/DVZ;               // Negative change to k-1

      // Transverse propagation, Fx updates:
      INCR_WAVE = -0.5*DT/DVZ*dF;
      CORR_WAVE *= DT/DVZ;
      cell.cpu_fz[BLOCK*SIZE_FLUXS+accIndex(i,j,k)] = dF;//INCR_WAVE*fabs(AX)*fabs(AZ) + CORR_WAVE*fabs(AX);
      //CORR_WAVE = 0.0;
      dFdt[findex(i+2,j+1,k+1)] += (INCR_WAVE*AX_POS*AZ_POS + CORR_WAVE*AX_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k+1)] -= (INCR_WAVE*AX_POS*AZ_POS + CORR_WAVE*AX_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k+1)] += (INCR_WAVE*AX_NEG*AZ_POS + CORR_WAVE*AX_NEG)*DT/DVZ;
      dFdt[findex(i  ,j+1,k+1)] -= (INCR_WAVE*AX_NEG*AZ_POS + CORR_WAVE*AX_NEG)*DT/DVZ;
      dFdt[findex(i+2,j+1,k  )] += (INCR_WAVE*AX_POS*AZ_NEG - CORR_WAVE*AX_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k  )] -= (INCR_WAVE*AX_POS*AZ_NEG - CORR_WAVE*AX_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k  )] += (INCR_WAVE*AX_NEG*AZ_NEG - CORR_WAVE*AX_NEG)*DT/DVZ;
      dFdt[findex(i  ,j+1,k  )] -= (INCR_WAVE*AX_NEG*AZ_NEG - CORR_WAVE*AX_NEG)*DT/DVZ;

      dFdt[findex(i+1,j+2,k+1)] += (INCR_WAVE*AY_POS*AZ_POS + CORR_WAVE*AY_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k+1)] -= (INCR_WAVE*AY_POS*AZ_POS + CORR_WAVE*AY_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k+1)] += (INCR_WAVE*AY_NEG*AZ_POS + CORR_WAVE*AY_NEG)*DT/DVZ;
      dFdt[findex(i+1,j+0,k+1)] -= (INCR_WAVE*AY_NEG*AZ_POS + CORR_WAVE*AY_NEG)*DT/DVZ;
      dFdt[findex(i+1,j+2,k  )] += (INCR_WAVE*AY_POS*AZ_NEG - CORR_WAVE*AY_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k  )] -= (INCR_WAVE*AY_POS*AZ_NEG - CORR_WAVE*AY_POS)*DT/DVZ;
      dFdt[findex(i+1,j+1,k  )] += (INCR_WAVE*AY_NEG*AZ_NEG - CORR_WAVE*AY_NEG)*DT/DVZ;
      dFdt[findex(i+1,j+0,k  )] -= (INCR_WAVE*AY_NEG*AZ_NEG - CORR_WAVE*AY_NEG)*DT/DVZ;

      INCR_WAVE = DT*DT/DVY/DVZ*dF/6.0;
      
      // Double transverse propagation, Fx updates:
      dFdt[findex(i+2,j+1,k+1)] += INCR_WAVE*AX_POS*fabs(AY)*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j+1,k+1)] += INCR_WAVE*AX_NEG*fabs(AY)*AZ_POS*DT/DVX;
      dFdt[findex(i+2,j+1,k  )] += INCR_WAVE*AX_POS*fabs(AY)*AZ_NEG*DT/DVX;
      dFdt[findex(i+1,j+1,k  )] += INCR_WAVE*AX_NEG*fabs(AY)*AZ_NEG*DT/DVX;
      
      dFdt[findex(i+2,j+2,k+1)] -= INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j+2,k+1)] -= INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVX;
      dFdt[findex(i+2,j  ,k+1)] += INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j  ,k+1)] += INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVX;
      dFdt[findex(i+2,j+2,k  )] -= INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVX;
      dFdt[findex(i+1,j+2,k  )] -= INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVX;
      dFdt[findex(i+2,j  ,k  )] += INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVX;
      dFdt[findex(i+1,j  ,k  )] += INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVX;
      
      
      dFdt[findex(i+1,j+1,k+1)] -= INCR_WAVE*AX_POS*fabs(AY)*AZ_POS*DT/DVX;
      dFdt[findex(i  ,j+1,k+1)] -= INCR_WAVE*AX_NEG*fabs(AY)*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j+1,k  )] -= INCR_WAVE*AX_POS*fabs(AY)*AZ_NEG*DT/DVX;
      dFdt[findex(i  ,j+1,k  )] -= INCR_WAVE*AX_NEG*fabs(AY)*AZ_NEG*DT/DVX;
      
      dFdt[findex(i+1,j+2,k+1)] += INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVX;
      dFdt[findex(i  ,j+2,k+1)] += INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j  ,k+1)] -= INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVX;
      dFdt[findex(i  ,j  ,k+1)] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVX;
      dFdt[findex(i+1,j+2,k  )] += INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVX;
      dFdt[findex(i  ,j+2,k  )] += INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVX;
      dFdt[findex(i+1,j  ,k  )] -= INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVX;
      dFdt[findex(i  ,j  ,k  )] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVX;
      
      INCR_WAVE = DT*DT/DVX/DVZ*dF/6.0;
      
      // Double transverse propagation, Fy updates:
      dFdt[findex(i+1,j+2,k+1)] += INCR_WAVE*fabs(AX)*AY_POS*AZ_POS*DT/DVY;
      dFdt[findex(i+1,j+1,k+1)] += INCR_WAVE*fabs(AX)*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i+1,j+2,k  )] += INCR_WAVE*fabs(AX)*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i+1,j+1,k  )] += INCR_WAVE*fabs(AX)*AY_NEG*AZ_NEG*DT/DVY;
      
      dFdt[findex(i+2,j+2,k+1)] -= INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVY;
      dFdt[findex(i  ,j+2,k+1)] += INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVY;
      dFdt[findex(i+2,j+1,k+1)] -= INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i  ,j+1,k+1)] += INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i+2,j+2,k  )] -= INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i  ,j+2,k  )] += INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i+2,j+1,k  )] -= INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVY;
      dFdt[findex(i  ,j+1,k  )] += INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVY;
      
      dFdt[findex(i+1,j+1,k+1)] -= INCR_WAVE*fabs(AX)*AY_POS*AZ_POS*DT/DVY;
      dFdt[findex(i+1,j  ,k+1)] -= INCR_WAVE*fabs(AX)*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i+1,j+1,k  )] -= INCR_WAVE*fabs(AX)*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i+1,j  ,k  )] -= INCR_WAVE*fabs(AX)*AY_NEG*AZ_NEG*DT/DVY;
      
      dFdt[findex(i+2,j+1,k+1)] += INCR_WAVE*AX_POS*AY_POS*AZ_POS*DT/DVY;
      dFdt[findex(i  ,j+1,k+1)] -= INCR_WAVE*AX_NEG*AY_POS*AZ_POS*DT/DVY;
      dFdt[findex(i+2,j  ,k+1)] += INCR_WAVE*AX_POS*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i  ,j  ,k+1)] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_POS*DT/DVY;
      dFdt[findex(i+2,j+1,k  )] += INCR_WAVE*AX_POS*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i  ,j+1,k  )] -= INCR_WAVE*AX_NEG*AY_POS*AZ_NEG*DT/DVY;
      dFdt[findex(i+2,j  ,k  )] += INCR_WAVE*AX_POS*AY_NEG*AZ_NEG*DT/DVY;
      dFdt[findex(i  ,j  ,k  )] -= INCR_WAVE*AX_NEG*AY_NEG*AZ_NEG*DT/DVY;
      
      /*
      dF = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+2,j+2,k+1)];
      CORR_WAVE = 0.25*DT*DT*fabs(AZ)*(1.0-fabs(AZ)*DT)*dFdz;
      
      // Donor cell upwind method:
      Fz[(k+1)*YSXS+(j+1)*XS+i+1] += AZ_POS*avgs[fullInd(i+2,j+2,k+1)] + AZ_NEG*avgs[fullInd(i+2.j+2,k+2)] + 0.5*fabs(AZ)*(1.0-fabs(AZ)*DT)*dFdz;
      
      // Transverse x/y propagations:
      Fx[(k+1)*YSXS+(j+1)*XS+i+2] += -0.5*DT*AX_POS*AZ_POS*dF + AX_POS*fabs(AY)*AZ_POS*DT*DT*dF/6.0 + 2.0/DT*CORR_WAVE*AX_POS;
      Fx[(k+1)*YSXS+(j+1)*XS+i+1] += -0.5*DT*AX_NEG*AZ_POS*dF + AX_NEG*fabs(AY)*AZ_POS*DT*DT*dF/6.0 + 2.0/DT*CORR_WAVE*AX_NEG;
      Fx[(k  )*YSXS+(j+1)*XS+i+2] += -0.5*DT*AX_POS*AZ_NEG*dF + AX_POS*fabs(AY)*AZ_NEG*DT*DT*dF/6.0 - 2.0/DT*CORR_WAVE*AX_POS;
      Fx[(k  )*YSXS+(j+1)*XS+i+1] += -0.5*DT*AX_NEG*AZ_NEG*dF + AX_NEG*fabs(AY)*AZ_NEG*DT*DT*dF/6.0 - 2.0/DT*CORR_WAVE*AX_NEG;
      
      Fy[(k+1)*YSXS+(j+2)*XS+i+1] += -0.5*DT*AY_POS*AZ_POS*dF + fabs(AX)*AY_POS*AZ_POS*DT*DT*dF/6.0 + 2.0/DT*CORR_WAVE*AY_POS;
      Fy[(k+1)*YSXS+(j+1)*XS+i+1] += -0.5*DT*AY_NEG*AZ_POS*dF + fabs(AX)*AY_NEG*AZ_POS*DT*DT*dF/6.0 + 2.0/DT*CORR_WAVE*AY_NEG;
      Fy[(k  )*YSXS+(j+2)*XS+i+1] += -0.5*DT*AY_POS*AZ_NEG*dF + fabs(AX)*AY_POS*AZ_NEG*DT*DT*dF/6.0 - 2.0/DT*CORR_WAVE*AY_POS;
      Fy[(k  )*YSXS+(j+1)*XS+i+1] += -0.5*DT*AY_NEG*AZ_NEG*dF + fabs(AX)*AY_NEG*AZ_NEG*DT*DT*dF/6.0 - 2.0/DT*CORR_WAVE*AY_NEG;
      
      // Double transverse x/y propagations:
      Fx[(k+1)*YSXS+(j+2)*XS+i+2] -= AX_POS*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_POS*AY_POS;
      Fx[(k  )*YSXS+(j+2)*XS+i+2] -= AX_POS*AY_POS*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_POS*AY_POS;
      Fx[(k+1)*YSXS+(j+2)*XS+i+1] -= AX_NEG*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_NEG*AY_POS;
      Fx[(k  )*YSXS+(j+2)*XS+i+1] -= AX_NEG*AY_POS*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_NEG*AY_POS;
      Fx[(k+1)*YSXS+(j  )*XS+i+2] += AX_POS*AY_NEG*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_POS*AY_NEG;
      Fx[(k  )*YSXS+(j  )*XS+i+2] += AX_POS*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_POS*AY_NEG;
      Fx[(k+1)*YSXS+(j  )*XS+i+1] += AX_NEG*AY_NEG*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_NEG*AY_NEG;
      Fx[(k  )*YSXS+(j  )*XS+i+1] += AX_NEG*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_NEG*AY_NEG;
      
      Fy[(k+1)*YSXS+(j+2)*XS+i+2] -= AX_POS*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_POS*AY_POS;
      Fy[(k  )*YSXS+(j+2)*XS+i+2] -= AX_POS*AY_POS*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_POS*AY_POS;
      Fy[(k+1)*YSXS+(j+2)*XS+i  ] += AX_NEG*AY_POS*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_NEG*AY_POS;
      Fy[(k  )*YSXS+(j+2)*XS+i  ] += AX_NEG*AY_POS*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_NEG*AY_POS;
      Fy[(k+1)*YSXS+(j+1)*XS+i+2] -= AX_POS*AY_NEG*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_POS*AY_NEG;
      Fy[(k  )*YSXS+(j+1)*XS+i+2] -= AX_POS*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_POS*AY_NEG;
      Fy[(k+1)*YSXS+(j+1)*XS+i  ] += AX_NEG*AY_NEG*AZ_POS*DT*DT*dF/6.0 - CORR_WAVE*AX_NEG*AY_NEG;
      Fy[(k  )*YSXS+(j+1)*XS+i  ] += AX_NEG*AY_NEG*AZ_NEG*DT*DT*dF/6.0 + CORR_WAVE*AX_NEG*AY_NEG;
       */
   }
   accumulateChanges(BLOCK,dFdt,cell.cpu_fx,cell.cpu_nbrsVel+BLOCK*SIZE_NBRS_VEL);
}
/*   
template<typename REAL,typename UINT,typename CELL> void cpu_calcVelFluxes(CELL& cell,const UINT& BLOCK,const REAL& DT) {
   // Copy volume averages and ghost cells values to a temporary array,
   // which is easier to use to calculate derivatives:

   const REAL EPSILON = 1.0e-15;
   const int OFF_OBL_RIGHT_BOTTOM_NBR = -9;
   const int OFF_OBL_LEFT_TOP_NBR = 9;
   
   Real Fx[100];
   Real Fy[100];
   for (UINT i=0; i<100; ++i) Fx[i] = 0.0;
   for (UINT i=0; i<100; ++i) Fy[i] = 0.0;
   
   const REAL* const cellParams = cell.cpu_cellParams;
   const REAL* const blockParams = cell.cpu_blockParams + BLOCK*SIZE_BLOCKPARAMS;
   
   REAL avgs[8*WID3];
   fetchAllAverages(BLOCK,avgs,cell.cpu_avgs,cell.cpu_nbrsVel);
   
   UINT I,J;
   REAL VX_I,VY_I,VX_J,VY_J,R;
   REAL xl2,xl1,xcc,xr1,xr2;
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      // Calculate velocities on vx,vy faces:
      VX_I = blockParams[BlockParams::VXCRD] + i*blockParams[BlockParams::DVX];
      VY_J = blockParams[BlockParams::VYCRD] + j*blockParams[BlockParams::DVY];
      VY_I = VY_J + 0.5*blockParams[BlockParams::DVY];
      VX_J = VX_I + 0.5*blockParams[BlockParams::DVX];
      
      // Calculate acceleration on vx,vy faces:
      R = -VX_I;
      VX_I = Parameters::q_per_m * cellParams[CellParams::BZ] * VY_I;
      VY_I = Parameters::q_per_m * cellParams[CellParams::BZ] * R;
      R = -VX_J;
      VX_J = Parameters::q_per_m * cellParams[CellParams::BZ] * VY_J;
      VY_J = Parameters::q_per_m * cellParams[CellParams::BZ] * R;
      
      // ***** VX-FLUXES *****
      
      R = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+1,j+2,k+2)]; // x(i,j) - x(i-1,j)
      if (VX_I > 0.0) I = i+1;
      else I = i+2;
      //fx[accIndex(i,j,k)] += VX_I*avgs[fullInd(I,j+2,k+2)];
      Fx[k*25+(j+1)*5+i] += VX_I*avgs[fullInd(I,j+2,k+2)];
      // End of Method #1

      if (VX_I > 0.0) I = i+2;
      else I = i+2-1;
      if (VY_I > 0.0) J = j+2+1;
      else J = j+2;
      const REAL SX = 0.5*DT/blockParams[BlockParams::DVX]*VX_I*VY_I*R;
      Fy[k*25+(J-2)*5+I+1-2] -= SX;
      // End of Method #2
      
      if (VX_I > 0.0) I = i+2-1;
      else I = i+2+1;
      const REAL THETA_UPX = avgs[fullInd(I  ,j+2,k+2)] - avgs[fullInd(I-1,j+2,k+2)];
      const REAL THETA_LOX = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+1,j+2,k+2)] + EPSILON;
      //std::cerr << R << '\t';
      R *= limiter(THETA_UPX/THETA_LOX);
      //std::cerr << R << std::endl;
      Fx[k*25+(j+1)*5+i] += 0.5*fabs(VX_I)*(1.0-DT/blockParams[BlockParams::DVX]*fabs(VX_I))*R;
      // End of Method #3

      Fy[k*25+(J-2)*5+i+1] += DT/blockParams[BlockParams::DVX]*VY_I*SX;
      Fy[k*25+(J-2)*5+i  ] -= DT/blockParams[BlockParams::DVX]*VY_I*SX;
      // End of Method #4
      
      // ***** VY-FLUXES *****
      
      R = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+2,j+1,k+2)]; // x(i,j) - x(i,j-1)
      if (VY_J > 0.0) J = j+1;
      else J = j+2;
      //fy[accIndex(i,j,k)] += VY_J*avgs[fullInd(i+2,J,k+2)];
      Fy[k*25+j*5+i+1] += VY_J*avgs[fullInd(i+2,J,k+2)];
      // End of Method #1
      
      if (VY_J > 0.0) J = j+2;
      else J = j+2-1;
      if (VX_J > 0.0) I = i+2+1;
      else I = i+2;
      const REAL SY = 0.5*DT/blockParams[BlockParams::DVY]*VX_J*VY_J*R;
      Fx[k*25+(J+1-2)*5+I-2] -= SY;
      // End of Method #2
      
      if (VY_J > 0.0) J = j+2-1;
      else J = j+2+1;
      const REAL THETA_UPY = avgs[fullInd(i+2,J  ,k+2)] - avgs[fullInd(i+2,J-1,k+2)];
      const REAL THETA_LOY = avgs[fullInd(i+2,j+2,k+2)] - avgs[fullInd(i+2,j+1,k+2)] + EPSILON;
      R *= limiter(THETA_UPY/THETA_LOY);
      
      Fy[k*25+j*5+i+1] += 0.5*fabs(VY_J)*(1.0-DT/blockParams[BlockParams::DVY]*fabs(VY_J))*R;
      // End of Method #3
      
      Fx[k*25+(j+1)*5+I-2] += DT/blockParams[BlockParams::DVY]*VX_J*SY;
      Fx[k*25+(j  )*5+I-2] -= DT/blockParams[BlockParams::DVY]*VX_J*SY;
      // End of Method #4
   }
   
   // Copy Fx fluxes from temporary blocks:
   const UINT* const nbrsVel = cell.cpu_nbrsVel + BLOCK*SIZE_NBRS_VEL;
   if (isBoundary(nbrsVel[NbrsVel::STATE],NbrsVel::VX_POS_BND) == 0) { // Copy to +vx nbr
      const UINT VXPOSNBR = nbrsVel[NbrsVel::VXPOS];
      REAL* nbrFx = cell.cpu_fx + VXPOSNBR*SIZE_FLUXS;
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	 nbrFx[accIndex(convert<UINT>(0),j,k)] += Fx[k*25+(j+1)*5+4];
      }
      // Check if fluxes copied to right bottom nbr:
      if (isBoundary(nbrsVel[NbrsVel::STATE],NbrsVel::VY_NEG_BND) == 0) {
	 const UINT OBL_NBR = BLOCK - Parameters::vyblocks_ini + 1;
	 nbrFx = cell.cpu_fx + OBL_NBR*SIZE_FLUXS;
	 for (UINT k=0; k<WID; ++k) 
	   nbrFx[accIndex(convert<UINT>(0),convert<UINT>(3),k)] += Fx[k*25+4];
      }
   }
   if (isBoundary(nbrsVel[NbrsVel::STATE],NbrsVel::VY_NEG_BND) == 0) { // Copy to -vy nbr
      const UINT VYNEGNBR = nbrsVel[NbrsVel::VYNEG];
      REAL* const nbrFx = cell.cpu_fx + VYNEGNBR*SIZE_FLUXS;
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) {
	 nbrFx[accIndex(i,convert<UINT>(3),k)] += Fx[k*25+i];
      }
   }
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) 
     cell.cpu_fx[BLOCK*SIZE_FLUXS + accIndex(i,j,k)] += Fx[k*25+(j+1)*5+i];

   // Copy Fy fluxes from temporary blocks:
   if (isBoundary(nbrsVel[NbrsVel::STATE],NbrsVel::VX_NEG_BND) == 0) { // Copy to -vx nbr
      const UINT VXNEGNBR = nbrsVel[NbrsVel::VXNEG];
      REAL* nbrFy = cell.cpu_fy + VXNEGNBR*SIZE_FLUXS;
      for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) {
	nbrFy[accIndex(convert<UINT>(3),j,k)] += Fy[k*25+j*5];
      }
      // Check if fluxes copied to upper left nbr:
      if (isBoundary(nbrsVel[NbrsVel::STATE],NbrsVel::VY_POS_BND) == 0) {
	 const UINT OBL_NBR = BLOCK + Parameters::vyblocks_ini - 1;
	 nbrFy = cell.cpu_fy + OBL_NBR*SIZE_FLUXS;
	 for (UINT k=0; k<WID; ++k)
	   nbrFy[accIndex(convert<UINT>(3),convert<UINT>(0),k)] += Fy[k*25+4*5];
      }
   }
   if (isBoundary(nbrsVel[NbrsVel::STATE],NbrsVel::VY_POS_BND) == 0) { // Copy to +vy nbr
      const UINT VYPOSNBR = nbrsVel[NbrsVel::VYPOS];
      REAL* const nbrFy = cell.cpu_fy + VYPOSNBR*SIZE_FLUXS;
      for (UINT k=0; k<WID; ++k) for (UINT i=0; i<WID; ++i) {
	 nbrFy[accIndex(i,convert<UINT>(0),k)] += Fy[k*25+4*5+i+1];
      }
   }
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i)
     cell.cpu_fy[BLOCK*SIZE_FLUXS + accIndex(i,j,k)] += Fy[k*25+j*5+i+1];

   
}
 */
/*
template<typename REAL,typename UINT,typename CELL> void cpu_calcVelFluxesX(CELL& cell,const UINT& BLOCK) {
   // The size of array is (5,4,4)
   const UINT XS = 5;
   const UINT YS = 4;
   const UINT YSXS = YS*XS;
   // Copy volume averages and fluxes to temporary arrays, which are easier
   // to use to calculate fluxes:
   REAL avgs[SIZE_VELBLOCK + WID2];
   REAL d1x[SIZE_DERIV + SIZE_BDERI];
   REAL d2x[SIZE_DERIV + SIZE_BDERI];
   fetchAveragesX(BLOCK,avgs,cell.cpu_avgs,cell.cpu_nbrsVel);
   fetchDerivsX(BLOCK,d1x,cell.cpu_d1x,cell.cpu_nbrsVel);
   fetchDerivsX(BLOCK,d2x,cell.cpu_d2x,cell.cpu_nbrsVel);
   
   // Reconstruct volume averages at negative and positive side of the face,
   // and calculate vx-flux for each cell in the block:
   REAL avg_neg,avg_pos;
   REAL* const fx           = cell.cpu_fx          + BLOCK*SIZE_FLUXS;
   creal* const blockParams = cell.cpu_blockParams + BLOCK*SIZE_BLOCKPARAMS;
   for (UINT k=0; k<WID; ++k) {
      REAL K = convert<REAL>(1.0)*k;
      for (UINT j=0; j<WID; ++j) {
	 REAL J = convert<REAL>(1.0)*j;
	 #pragma ivdep
	 for (UINT i=0; i<WID; ++i) {
	    avg_neg = reconstruct_neg(avgs[k*YSXS+j*XS+(i  )],d1x[k*YSXS+j*XS+(i  )],d2x[k*YSXS+j*XS+(i  )]);
	    avg_pos = reconstruct_pos(avgs[k*YSXS+j*XS+(i+1)],d1x[k*YSXS+j*XS+(i+1)],d2x[k*YSXS+j*XS+(i+1)]);
	    //fx[accIndex(i,j,k)] = velocityFluxX(i,j,k,avg_neg,avg_pos,cell.cpu_cellParams,blockParams);
	    fx[accIndex(i,j,k)] = velocityFluxX(J,K,avg_neg,avg_pos,cell.cpu_cellParams,blockParams);
	 }
      }
   }
}
   
template<typename REAL,typename UINT,typename CELL> void cpu_calcVelFluxesY(CELL& cell,const UINT& BLOCK) {
   // The size of the array avgs is (4,5,4)
   const UINT XS = 4;
   const UINT YS = 5;
   const UINT YSXS = YS*XS;
   // Copy volume averages and fluxes to temporary arrays, which are easier
   // to use to calculate fluxes:
   REAL avgs[SIZE_VELBLOCK + WID2];
   REAL d1y[SIZE_DERIV + SIZE_BDERI];
   REAL d2y[SIZE_DERIV + SIZE_BDERI];
   fetchAveragesY(BLOCK,avgs,cell.cpu_avgs,cell.cpu_nbrsVel);
   fetchDerivsY(BLOCK,d1y,cell.cpu_d1y,cell.cpu_nbrsVel);
   fetchDerivsY(BLOCK,d2y,cell.cpu_d2y,cell.cpu_nbrsVel);
       
   // Reconstruct volume averages at negative and positive side of the face,
   // and calculate vy-flux for each cell in the block:
   REAL avg_neg,avg_pos;
   REAL* const fy           = cell.cpu_fy          + BLOCK*SIZE_FLUXS;
   creal* const blockParams = cell.cpu_blockParams + BLOCK*SIZE_BLOCKPARAMS;
   for (UINT k=0; k<WID; ++k) {
      const REAL K = convert<REAL>(1.0)*k;
      for (UINT j=0; j<WID; ++j) {
	 for (UINT i=0; i<WID; ++i) {
	    const REAL I = convert<REAL>(1.0)*i;
	    avg_neg = reconstruct_neg(avgs[k*YSXS+(j  )*XS+i],d1y[k*YSXS+(j  )*XS+i],d2y[k*YSXS+(j  )*XS+i]);
	    avg_pos = reconstruct_pos(avgs[k*YSXS+(j+1)*XS+i],d1y[k*YSXS+(j+1)*XS+i],d2y[k*YSXS+(j+1)*XS+i]);
	    //fy[accIndex(i,j,k)] = velocityFluxY(i,j,k,avg_neg,avg_pos,cell.cpu_cellParams,blockParams);
	    fy[accIndex(i,j,k)] = velocityFluxY(I,K,avg_neg,avg_pos,cell.cpu_cellParams,blockParams);
	 }
      }
   }
}

template<typename REAL,typename UINT,typename CELL> void cpu_calcVelFluxesZ(CELL& cell,const UINT& BLOCK) {
   // The size of array avgs is (4,4,5)
   const UINT XS = 4;
   const UINT YS = 4;
   const UINT YSXS = YS*XS;
   // Copy volume averages and fluxes to temporary arrays, which are easier
   // to use to calculate fluxes:
   REAL avgs[SIZE_VELBLOCK + WID2];
   REAL d1z[SIZE_DERIV + SIZE_BDERI];
   REAL d2z[SIZE_DERIV + SIZE_BDERI];
   fetchAveragesZ(BLOCK,avgs,cell.cpu_avgs,cell.cpu_nbrsVel);
   fetchDerivsZ(BLOCK,d1z,cell.cpu_d1z,cell.cpu_nbrsVel);
   fetchDerivsZ(BLOCK,d2z,cell.cpu_d2z,cell.cpu_nbrsVel);
   
   // Reconstruct volume averages at negative and positive side of the face,
   // and calculate vz-flux:
   REAL avg_neg,avg_pos;
   REAL* const fz           = cell.cpu_fz          + BLOCK*SIZE_FLUXS;
   creal* const blockParams = cell.cpu_blockParams + BLOCK*SIZE_BLOCKPARAMS;
   for (UINT k=0; k<WID; ++k) {
      for (UINT j=0; j<WID; ++j) {
	 const REAL J = convert<REAL>(1.0)*j;
	 for (UINT i=0; i<WID; ++i) {
	    const REAL I = convert<REAL>(1.0)*i;
	    avg_neg = reconstruct_neg(avgs[(k  )*YSXS+j*XS+i],d1z[(k  )*YSXS+j*XS+i],d2z[(k  )*YSXS+j*XS+i]);
	    avg_pos = reconstruct_pos(avgs[(k+1)*YSXS+j*XS+i],d1z[(k+1)*YSXS+j*XS+i],d2z[(k+1)*YSXS+j*XS+i]);
	    fz[accIndex(i,j,k)] = velocityFluxZ(I,J,avg_neg,avg_pos,cell.cpu_cellParams,blockParams);
	    //fz[accIndex(i,j,k)] = velocityFluxZ(i,j,k,avg_neg,avg_pos,cell.cpu_cellParams,blockParams);
	 }
      }
   }      
}
*/
template<typename REAL,typename UINT,typename CELL> void cpu_propagateVel(CELL& cell,const UINT& BLOCK,const REAL& DT) {
   //REAL avgs[SIZE_VELBLOCK];
   //REAL flux[SIZE_FLUXS + SIZE_BFLUX];
   //const REAL* const blockParams = cell.cpu_blockParams + BLOCK*SIZE_BLOCKPARAMS;
   
   for (UINT i=0; i<WID3; ++i) cell.cpu_avgs[BLOCK*SIZE_VELBLOCK+i] += cell.cpu_fx[BLOCK*SIZE_VELBLOCK+i];
   
   /*
   // Calculate the contribution to d(avg)/dt from vx-fluxes:
   UINT XS = 5;
   UINT YS = 4;
   UINT YSXS = YS*XS;
   REAL cnst = DT / blockParams[BlockParams::DVX];
   fetchFluxesX(BLOCK,flux,cell.cpu_fx,cell.cpu_nbrsVel);
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      avgs[accIndex(i,j,k)] = (flux[k*YSXS+j*XS+i] - flux[k*YSXS+j*XS+(i+1)])*cnst;
   }
   // Calculate the contribution to d(avg)/dt from vy-fluxes:
   XS = 4;
   YS = 5;
   YSXS = YS*XS;
   cnst = DT / blockParams[BlockParams::DVY];
   fetchFluxesY(BLOCK,flux,cell.cpu_fy,cell.cpu_nbrsVel);
   //for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
   //   avgs[accIndex(i,j,k)] += (flux[k*YSXS+j*XS+i] - flux[k*YSXS+(j+1)*XS+i])*cnst;
   //}
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) avgs[accIndex(i,j,k)] += flux[k*YSXS+j*XS+i]*cnst;
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) avgs[accIndex(i,j,k)] -= flux[k*YSXS+(j+1)*XS+i]*cnst;
   // Calculate the contribution to d(avg)/dt from vz-fluxes:
   XS = 4;
   YS = 4;
   YSXS = YS*XS;
   cnst = DT / blockParams[BlockParams::DVZ];
   fetchFluxesZ(BLOCK,flux,cell.cpu_fz,cell.cpu_nbrsVel);
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      avgs[accIndex(i,j,k)] += (flux[k*YSXS+j*XS+i] - flux[(k+1)*YSXS+j*XS+i])*cnst;
   }
   // Store results:
   REAL* const cpu_avgs = cell.cpu_avgs + BLOCK*SIZE_VELBLOCK;
   for (UINT k=0; k<WID; ++k) for (UINT j=0; j<WID; ++j) for (UINT i=0; i<WID; ++i) {
      cpu_avgs[accIndex(i,j,k)] += avgs[accIndex(i,j,k)];
   }
   */
}
#endif