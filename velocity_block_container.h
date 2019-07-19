/*
 * This file is part of Vlasiator.
 * Copyright 2010-2016 Finnish Meteorological Institute
 *
 * For details of usage, see the COPYING file and read the "Rules of the Road"
 * at http://www.physics.helsinki.fi/vlasiator/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef VELOCITY_BLOCK_CONTAINER_H
#define VELOCITY_BLOCK_CONTAINER_H

#include <vector>

#include "common.h"
#include "unistd.h"

#ifdef DEBUG_VBC
   #include <sstream>
#endif

#include "definitions.h"

namespace vmesh {
    
   static const double BLOCK_ALLOCATION_FACTOR = 1.1;
   
   template<typename LID>
   class VelocityBlockContainer {
    public:
      VelocityBlockContainer();
      LID capacity() const;
      size_t capacityInBytes() const;
      void clear();
      void copy(const LID& source,const LID& target);
      static double getBlockAllocationFactor();

      cBlock* getBlocks();
      const cBlock* getBlocks() const;
      Realf* getData(const LID& blockLID, Realf* array);
      const Realf* getData(const LID& blockLID, Realf* array) const;
      Realf* setData(const LID& blockLID, Realf* array);
      void clearBlock(const LID& blockLID);

      //Realf* getNullData();

      Real* getParameters();
      const Real* getParameters() const;
      Real* getParameters(const LID& blockLID);      
      const Real* getParameters(const LID& blockLID) const;
      void pop();
      LID push_back();
      LID push_back(const uint32_t& N_blocks);
      bool recapacitate(const LID& capacity);
      bool setSize(const LID& newSize);
      LID size() const;
      size_t sizeInBytes() const;
      void swap(VelocityBlockContainer& vbc);

      #ifdef DEBUG_VBC
      const Realf& getData(const LID& blockLID,const unsigned int& cell) const;
      const Real& getParameters(const LID& blockLID,const unsigned int& i) const;
      void setData(const LID& blockLID,const unsigned int& cell,const Realf& value);
      #endif

    private:
      typedef union {     
         float f;
         u_int32_t i;
      } float_int;
      void exitInvalidLocalID(const LID& localID,const std::string& funcName) const;
      void resize();
   
      std::vector<cBlock,aligned_allocator<cBlock,1> > block_data;
      //Realf null_block_data[WID3];
      LID currentCapacity;
      LID numberOfBlocks;
      std::vector<Real,aligned_allocator<Real,BlockParams::N_VELOCITY_BLOCK_PARAMS> > parameters;
   };
   
   template<typename LID> inline
   VelocityBlockContainer<LID>::VelocityBlockContainer() {
      currentCapacity = 0;
      numberOfBlocks = 0;
   }

   template<typename LID> inline
   LID VelocityBlockContainer<LID>::capacity() const {
      return currentCapacity;
   }
   
   template<typename LID> inline
   size_t VelocityBlockContainer<LID>::capacityInBytes() const {
#ifdef COMP_SIZE
      size_t compressed_size = 0;
      for (uint i = 0; i < block_data.size(); i++)
      {
         compressed_size += block_data[i].compressedSize();
      }
      return compressed_size + block_data.capacity()*sizeof(cBlock) + parameters.size()*sizeof(Real);
#else
      return block_data.capacity()*sizeof(cBlock) + parameters.size()*sizeof(Real);
#endif
   }

   /** Clears VelocityBlockContainer data and deallocates all memory 
    * reserved for velocity blocks.*/
   template<typename LID> inline
   void VelocityBlockContainer<LID>::clear() {
      std::vector<cBlock,aligned_allocator<cBlock,1> > dummy_data;
      std::vector<Real,aligned_allocator<Real,BlockParams::N_VELOCITY_BLOCK_PARAMS> > dummy_parameters;
      
      block_data.swap(dummy_data);
      parameters.swap(dummy_parameters);
      
      currentCapacity = 0;
      numberOfBlocks = 0;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::copy(const LID& source,const LID& target) {
      #ifdef DEBUG_VBC
         bool ok = true;
         if (source >= numberOfBlocks) ok = false;
         if (source >= currentCapacity) ok = false;
         if (target >= numberOfBlocks) ok = false;
         if (target >= currentCapacity) ok = false;
         if (numberOfBlocks >= currentCapacity) ok = false;
         if (source != numberOfBlocks-1) ok = false;
         if (source >= block_data.size()) ok = false;
         if (source*BlockParams::N_VELOCITY_BLOCK_PARAMS+BlockParams::N_VELOCITY_BLOCK_PARAMS-1 >= parameters.size()) ok = false;
         if (target*BlockParams::N_VELOCITY_BLOCK_PARAMS+BlockParams::N_VELOCITY_BLOCK_PARAMS-1 >= parameters.size()) ok = false;
         if (parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS != block_data.size()) ok = false;
         if (ok == false) {
            std::stringstream ss;
            ss << "VBC ERROR: invalid source LID=" << source << " in copy, target=" << target << " #blocks=" << numberOfBlocks << " capacity=" << currentCapacity << std::endl;
            ss << "or sizes are wrong, data.size()=" << block_data.size() << " parameters.size()=" << parameters.size() << std::endl;
            std::cerr << ss.str();
            sleep(1);
            exit(1);
         }
      #endif

      block_data[target] = block_data[source];
      for (int i=0; i<BlockParams::N_VELOCITY_BLOCK_PARAMS; ++i) {
         parameters[target*BlockParams::N_VELOCITY_BLOCK_PARAMS+i] = parameters[source*BlockParams::N_VELOCITY_BLOCK_PARAMS+i];
      }
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::exitInvalidLocalID(const LID& localID,const std::string& funcName) const {
      int rank;
      MPI_Comm_rank(MPI_COMM_WORLD,&rank);

      std::stringstream ss;
      ss << "Process " << rank << ' ';
      ss << "Invalid localID " << localID << " used in function '" << funcName << "' max allowed value is " << numberOfBlocks << std::endl;
      std::cerr << ss.str();
      sleep(1);
      exit(1);
   }
   
   template<typename LID> inline
   double VelocityBlockContainer<LID>::getBlockAllocationFactor() {
      return BLOCK_ALLOCATION_FACTOR;
   }
   
   template<typename LID> inline
   cBlock* VelocityBlockContainer<LID>::getBlocks() {
      return block_data.data();
   }
   
   template<typename LID> inline
   const cBlock* VelocityBlockContainer<LID>::getBlocks() const {
      return block_data.data();
   } 

   template<typename LID> inline
   Realf* VelocityBlockContainer<LID>::getData(const LID& blockLID, Realf* array) {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"getData");
         if (blockLID >= block_data.size()) exitInvalidLocalID(blockLID,"const getData const");
      #endif
      block_data[blockLID].get(array);
      return array;
   }
   
   template<typename LID> inline
   const Realf* VelocityBlockContainer<LID>::getData(const LID& blockLID, Realf* array) const {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"const getData const");
         if (blockLID >= block_data.size()) exitInvalidLocalID(blockLID,"const getData const");
      #endif
      block_data[blockLID].get(array);
      return array;
   }

   template<typename LID> inline
   Realf* VelocityBlockContainer<LID>::setData(const LID& blockLID, Realf* array) {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"getData");
         if (blockLID >= block_data.size()) exitInvalidLocalID(blockLID,"const getData const");
      #endif
      block_data[blockLID].set(array);
      return array;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::clearBlock(const LID& blockLID) {
      block_data[blockLID].clear();
   }
   
   template<typename LID> inline
   Real* VelocityBlockContainer<LID>::getParameters() {
      return parameters.data();
   }
   
   template<typename LID> inline
   const Real* VelocityBlockContainer<LID>::getParameters() const {
      return parameters.data();
   }

   template<typename LID> inline
   Real* VelocityBlockContainer<LID>::getParameters(const LID& blockLID) {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"getParameters");
         if (blockLID >= parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS) exitInvalidLocalID(blockLID,"getParameters");
      #endif
      return parameters.data() + blockLID*BlockParams::N_VELOCITY_BLOCK_PARAMS;
   }
   
   template<typename LID> inline
   const Real* VelocityBlockContainer<LID>::getParameters(const LID& blockLID) const {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"const getParameters const");
         if (blockLID >= parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS) exitInvalidLocalID(blockLID,"getParameters");
      #endif
      return parameters.data() + blockLID*BlockParams::N_VELOCITY_BLOCK_PARAMS;
   }
   
   template<typename LID> inline
   void VelocityBlockContainer<LID>::pop() {
      if (numberOfBlocks == 0) return;
      --numberOfBlocks;
   }

   template<typename LID> inline
   LID VelocityBlockContainer<LID>::push_back() {
      LID newIndex = numberOfBlocks;
      if (newIndex >= currentCapacity) resize();

      #ifdef DEBUG_VBC
      if (newIndex >= block_data.size() || newIndex >= parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS) {
         std::stringstream ss;
         ss << "VBC ERROR in push_back, LID=" << newIndex << " for new block is out of bounds" << std::endl;
         ss << "\t data.size()=" << block_data.size()  << " parameters.size()=" << parameters.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         exit(1);
      }
      #endif

      // Clear velocity block data to zero values
      block_data[newIndex].clear();
      for (size_t i=0; i<BlockParams::N_VELOCITY_BLOCK_PARAMS; ++i) 
         parameters[newIndex*BlockParams::N_VELOCITY_BLOCK_PARAMS+i] = 0.0;

      ++numberOfBlocks;
      return newIndex;
   }
   
   template<typename LID> inline
   LID VelocityBlockContainer<LID>::push_back(const uint32_t& N_blocks) {
      const LID newIndex = numberOfBlocks;
      numberOfBlocks += N_blocks;
      resize();
      
      // Clear velocity block data to zero values
      for (size_t i=0; i<N_blocks; ++i) block_data[newIndex+i].clear();
      for (size_t i=0; i<BlockParams::N_VELOCITY_BLOCK_PARAMS*N_blocks; ++i)
	      parameters[newIndex*BlockParams::N_VELOCITY_BLOCK_PARAMS+i] = 0.0;

      return newIndex;
   }

   template<typename LID> inline
   bool VelocityBlockContainer<LID>::recapacitate(const LID& newCapacity) {
      if (newCapacity < numberOfBlocks) return false;
      {
         std::vector<cBlock,aligned_allocator<cBlock,1> > dummy_data(newCapacity);
         for (size_t i=0; i<numberOfBlocks; ++i) dummy_data[i] = block_data[i];
         dummy_data.swap(block_data);
      }
      {
         std::vector<Real,aligned_allocator<Real,BlockParams::N_VELOCITY_BLOCK_PARAMS> > dummy_parameters(newCapacity*BlockParams::N_VELOCITY_BLOCK_PARAMS);
         for (size_t i=0; i<numberOfBlocks*BlockParams::N_VELOCITY_BLOCK_PARAMS; ++i) dummy_parameters[i] = parameters[i];
         dummy_parameters.swap(parameters);
      }
      currentCapacity = newCapacity;
      return true;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::resize() {
      if ((numberOfBlocks+1) >= currentCapacity) {
         // Resize so that free space is block_allocation_chunk blocks, 
         // and at least two in case of having zero blocks.
         // The order of velocity blocks is unaltered.
         currentCapacity = 2 + numberOfBlocks * BLOCK_ALLOCATION_FACTOR;
         block_data.resize(currentCapacity);
         parameters.resize(currentCapacity*BlockParams::N_VELOCITY_BLOCK_PARAMS);
      }
   }

   template<typename LID> inline
   bool VelocityBlockContainer<LID>::setSize(const LID& newSize) {
      numberOfBlocks = newSize;
      if (newSize > currentCapacity) resize();
      return true;
   }

   /** Return the number of existing velocity blocks.
    * @return Number of existing velocity blocks.*/
   template<typename LID> inline
   LID VelocityBlockContainer<LID>::size() const {
      return numberOfBlocks;
   }

   template<typename LID> inline
   size_t VelocityBlockContainer<LID>::sizeInBytes() const {
#ifdef COMP_SIZE
      size_t compressed_size = 0;
      for (uint i = 0; i < block_data.size(); i++)
      {
         compressed_size += block_data[i].compressedSize();
      }
      return compressed_size + block_data.size()*sizeof(cBlock) + parameters.size()*sizeof(Real);
#else
      return block_data.size()*sizeof(cBlock) + parameters.size()*sizeof(Real);
#endif
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::swap(VelocityBlockContainer& vbc) {
      block_data.swap(vbc.block_data);
      parameters.swap(vbc.parameters);

      LID dummy = currentCapacity;
      currentCapacity = vbc.currentCapacity;
      vbc.currentCapacity = dummy;
      
      dummy = numberOfBlocks;
      numberOfBlocks = vbc.numberOfBlocks;
      vbc.numberOfBlocks = dummy;
   }
   
   #ifdef DEBUG_VBC

   template<typename LID> inline
   const Realf& VelocityBlockContainer<LID>::getData(const LID& blockLID,const unsigned int& cell) const {
      bool ok = true;
      if (cell >= WID3) ok = false;
      if (blockLID >= numberOfBlocks) ok = false;
      if (blockLID+cell >= block_data.size()) ok = false;
      if (ok == false) {
         std::stringstream ss;
         ss << "VBC ERROR: out of bounds in getData, LID=" << blockLID << " cell=" << cell << " #blocks=" << numberOfBlocks << " data.size()=" << block_data.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         exit(1);
      }

      Realf data[WID3];
      block_data[blockLID+cell].get(data);
      return data;
   }

   template<typename LID> inline
   const Real& VelocityBlockContainer<LID>::getParameters(const LID& blockLID,const unsigned int& cell) const {
      bool ok = true;
      if (cell >= BlockParams::N_VELOCITY_BLOCK_PARAMS) ok = false;
      if (blockLID >= numberOfBlocks) ok = false;
      if (blockLID*BlockParams::N_VELOCITY_BLOCK_PARAMS+cell >= parameters.size()) ok = false;
      if (ok == false) {
         std::stringstream ss;
         ss << "VBC ERROR: out of bounds in getParameters, LID=" << blockLID << " cell=" << cell << " #blocks=" << numberOfBlocks << " parameters.size()=" << parameters.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         exit(1);
      }
      
      return parameters[blockLID*BlockParams::N_VELOCITY_BLOCK_PARAMS+cell];
   }
   
   template<typename LID> inline
   void VelocityBlockContainer<LID>::setData(const LID& blockLID,const unsigned int& cell,const Realf& value) {
      bool ok = true;
      if (cell >= WID3) ok = false;
      if (blockLID >= numberOfBlocks) ok = false;
      if (blockLID+cell >= block_data.size()) ok = false;
      if (ok == false) {
         std::stringstream ss;
         ss << "VBC ERROR: out of bounds in setData, LID=" << blockLID << " cell=" << cell << " #blocks=" << numberOfBlocks << " data.size()=" << block_data.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         exit(1);
      }
      
      Realf data[WID3];
      block_data[blockLID+cell].get(data);
      return data;
   }
   
   #endif
   
} // namespace block_cont

#endif