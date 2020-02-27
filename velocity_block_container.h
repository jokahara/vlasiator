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
      Realf* getData();
      const Realf* getData() const;
      Realf* getData(const LID& blockLID);
      const Realf* getData(const LID& blockLID) const;

      LID compress();
      void decompress();
      Compf* getCompressedData();
      LID getCompressedSize();
      void setToBeDecompressed(LID size);
      void clearCompressedData();

      Realf* getNullData();
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
      void exitInvalidLocalID(const LID& localID,const std::string& funcName) const;
      void resize();
      
      std::vector<Realf,aligned_allocator<Realf,WID3> > block_data;
      Realf null_block_data[WID3];
      LID currentCapacity;
      LID numberOfBlocks;
      std::vector<Real,aligned_allocator<Real,BlockParams::N_VELOCITY_BLOCK_PARAMS> > parameters;

      std::vector<Compf,aligned_allocator<Compf,1>> compressed_data;
      bool mustBeDecompressed;
   };
   
   template<typename LID> inline
   VelocityBlockContainer<LID>::VelocityBlockContainer() {
      currentCapacity = 0;
      numberOfBlocks = 0;
      mustBeDecompressed = false;
   }
   
   template<typename LID> inline
   LID VelocityBlockContainer<LID>::capacity() const {
      return currentCapacity;
   }
   
   template<typename LID> inline
   size_t VelocityBlockContainer<LID>::capacityInBytes() const {
      return (block_data.capacity())*sizeof(Realf) + parameters.capacity()*sizeof(Real) + compressed_data.capacity()*sizeof(Compf);
   }

   /** Clears VelocityBlockContainer data and deallocates all memory 
    * reserved for velocity blocks.*/
   template<typename LID> inline
   void VelocityBlockContainer<LID>::clear() {
      std::vector<Realf,aligned_allocator<Realf,WID3> > dummy_data;
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
         if (source*WID3+WID3-1 >= block_data.size()) ok = false;
         if (source*BlockParams::N_VELOCITY_BLOCK_PARAMS+BlockParams::N_VELOCITY_BLOCK_PARAMS-1 >= parameters.size()) ok = false;
         if (target*BlockParams::N_VELOCITY_BLOCK_PARAMS+BlockParams::N_VELOCITY_BLOCK_PARAMS-1 >= parameters.size()) ok = false;
         if (parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS != block_data.size()/WID3) ok = false;
         if (ok == false) {
            std::stringstream ss;
            ss << "VBC ERROR: invalid source LID=" << source << " in copy, target=" << target << " #blocks=" << numberOfBlocks << " capacity=" << currentCapacity << std::endl;
            ss << "or sizes are wrong, data.size()=" << block_data.size() << " parameters.size()=" << parameters.size() << std::endl;
            std::cerr << ss.str();
            sleep(1);
            exit(1);
         }
      #endif

      for (unsigned int i=0; i<WID3; ++i) block_data[target*WID3+i] = block_data[source*WID3+i];
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
   Realf* VelocityBlockContainer<LID>::getData() {
      
      return block_data.data();
   }
   
   template<typename LID> inline
   const Realf* VelocityBlockContainer<LID>::getData() const {

      return block_data.data();
   }

   template<typename LID> inline
   Realf* VelocityBlockContainer<LID>::getData(const LID& blockLID) {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"getData");
         if (blockLID >= block_data.size()/WID3) exitInvalidLocalID(blockLID,"const getData const");
      #endif
      
      return block_data.data() + blockLID*WID3;
   }
   
   template<typename LID> inline
   const Realf* VelocityBlockContainer<LID>::getData(const LID& blockLID) const {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"const getData const");
         if (blockLID >= block_data.size()/WID3) exitInvalidLocalID(blockLID,"const getData const");
      #endif
      
      return block_data.data() + blockLID*WID3;
   }

   // compress data for MPI transfer
   template<typename LID> inline
   LID VelocityBlockContainer<LID>::compress() {
      if(compressed_data.size() > 0) return compressed_data.size();  // data has been already compressed
      if(numberOfBlocks == 0) return 0;                              // nothing to compress

      phiprof::start("Compressing data");
      compressed_data.resize((WID3+2) * numberOfBlocks); // max_size
      Compf* p = compressed_data.data();
      Realf* data = block_data.data();
      // TODO: omp parallel for
      for (size_t b = 0; b < numberOfBlocks; b++)
      {
         p += cBlock::set(data, p);
         data += WID3;
      }

      size_t compressedSize = p - compressed_data.data();
      // reallocating to reduce memory use
      std::vector<Compf,aligned_allocator<Compf,1> > dummy_data(compressedSize);
      for (size_t i=0; i<compressedSize; ++i) dummy_data[i] = compressed_data[i];
      dummy_data.swap(compressed_data);

      phiprof::stop("Compressing data");
      return compressedSize;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::decompress() {
      phiprof::start("Decompressing data");
      if (mustBeDecompressed) {

         Compf* p = compressed_data.data();
         Realf* data = block_data.data();
         size_t sizes[numberOfBlocks];
         int i;
         for (i = 1; (p - compressed_data.data()) < compressed_data.size(); i++)
         {
            if (i > numberOfBlocks)
            {
               std::cerr << "size exceeded" << std::endl;
            }
            
            sizes[i] = (*p & 0xFF);
            if (sizes[i] == 0)
            {
               p++;
               continue;
            }
            
            if (n_values >= BLOCK_SIZE - 4)
            {
               p += sizes[i] + OFFSET;
            }
            else {

               p += sizes[i] + OFFSET + sizeof(ulong) / sizeof(Compf);
            }
         }
         if (i < numberOfBlocks)
         {
            std::cerr << "size too small" << std::endl;
         }

         Compf* p = compressed_data.data();
         Realf temp[WID3];

         //Realf sum1 = 0, sum2 = 0;
         int z1 = 0, z2 = 0;
         for (size_t b = 0; b < numberOfBlocks; b++)
         {
            p += cBlock::get(temp, p);
            /*
            for (int i = 0; i < WID3; i++)
            {
               if (temp[i] > MIN_VALUE) sum1 += temp[i]; 
               else z1++; 

               if (data[i] > MIN_VALUE) sum2 += data[i];
               else z2++;               
            }
            */

            for (int i = 0; i < WID3; i++)
            {
               if (temp[i] < MIN_VALUE) z1++; 

               if (data[i] < MIN_VALUE) z2++;               
            }
            data += WID3;
         }

         if (z1 != z2)
         {
            //std::cerr << "sum: " << sum2 << " -> " << sum1 << std::endl;
            std::cerr << "zeroes: " << z2 << " -> " << z1 << std::endl;
         }
      }
      phiprof::stop("Decompressing data");

      clearCompressedData();
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::setToBeDecompressed(LID size) {
      if (size == 0) return;
      
      std::vector<Compf,aligned_allocator<Compf,1> > dummy_data(size);
      compressed_data.swap(dummy_data);
      mustBeDecompressed = true;
   }

   template<typename LID> inline
   Compf* VelocityBlockContainer<LID>::getCompressedData() {
      return compressed_data.data();
   }

   template<typename LID> inline
   LID VelocityBlockContainer<LID>::getCompressedSize() {
      return compressed_data.size();
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::clearCompressedData() {
      mustBeDecompressed = false;
      if (compressed_data.size() == 0) return; 
      
      std::vector<Compf,aligned_allocator<Compf,1> > dummy_data;
      compressed_data.swap(dummy_data);
   }

   template<typename LID> inline
   Realf* VelocityBlockContainer<LID>::getNullData() {
       return null_block_data;
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
      if (newIndex >= block_data.size()/WID3 || newIndex >= parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS) {
         std::stringstream ss;
         ss << "VBC ERROR in push_back, LID=" << newIndex << " for new block is out of bounds" << std::endl;
         ss << "\t data.size()=" << block_data.size()  << " parameters.size()=" << parameters.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         exit(1);
      }
      #endif

      // Clear velocity block data to zero values
      for (size_t i=0; i<WID3; ++i) block_data[newIndex*WID3+i] = 0.0;
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
      for (size_t i=0; i<WID3*N_blocks; ++i) block_data[newIndex*WID3+i] = 0.0;
      for (size_t i=0; i<BlockParams::N_VELOCITY_BLOCK_PARAMS*N_blocks; ++i)
	      parameters[newIndex*BlockParams::N_VELOCITY_BLOCK_PARAMS+i] = 0.0;

      return newIndex;
   }

   template<typename LID> inline
   bool VelocityBlockContainer<LID>::recapacitate(const LID& newCapacity) {
      if (newCapacity < numberOfBlocks) return false;
      {
         std::vector<Realf,aligned_allocator<Realf,WID3> > dummy_data(newCapacity*WID3);
         for (size_t i=0; i<numberOfBlocks*WID3; ++i) dummy_data[i] = block_data[i];
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
         block_data.resize(currentCapacity*WID3);
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
      return block_data.size()*sizeof(Realf) + parameters.size()*sizeof(Real) + block_data.size()*sizeof(Compf);
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
      if (blockLID*WID3+cell >= block_data.size()) ok = false;
      if (ok == false) {
         std::stringstream ss;
         ss << "VBC ERROR: out of bounds in getData, LID=" << blockLID << " cell=" << cell << " #blocks=" << numberOfBlocks << " data.size()=" << block_data.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         exit(1);
      }

      return block_data[blockLID*WID3+cell];
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
      if (blockLID*WID3+cell >= block_data.size()) ok = false;
      if (ok == false) {
         std::stringstream ss;
         ss << "VBC ERROR: out of bounds in setData, LID=" << blockLID << " cell=" << cell << " #blocks=" << numberOfBlocks << " data.size()=" << block_data.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         exit(1);
      }
      
      block_data[blockLID*WID3+cell] = value;
   }
   
   #endif
   
} // namespace block_cont

#endif
