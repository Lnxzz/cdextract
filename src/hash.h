/**************************************************************************

  libcdextract - 64-bit internal hash function for compact disc identification

  Copyright (C) 2021-2025 E. Heerschop (github@heerschop.frl)

  This function is used to generate a hash value for a disc based on the
  number of tracks, the disc length and the frame lengths of the tracks. 
  The resulting hash can be used as an index to speed up the identification 
  of a specific disc within a large dataset.
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published 
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.

 **************************************************************************/

 #ifndef HASH_H
 #define HASH_H

 #ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> // size_t
#include <stdint.h> // uint64_t


// try to force inline for hash functions
#if defined( __GNUC__ ) || defined( __clang__ )
#define FORCE_HASH_INLINE inline __attribute__((always_inline))
#else
#define FORCE_HASH_INLINE inline
#endif


/*
 * the hash seed uses the golden gamma for 40 bits:: 
 * floor(((1+sqrt(5))/2) * 2^40 mod 2^40) = 679535556991 = 0x9e3779b97f
 */
 #define HASH_SEED 0x9e3779b97fUL


/**
 * @brief initialize the hash with the given seed
 */
FORCE_HASH_INLINE void hash_init(int disc_length, int number_of_tracks, uint64_t *hash) {
  *hash = (HASH_SEED & 0x000000FFFFFFFFFFUL) |                  // 40 bits: seed for the hash of the frame lengths
          ((number_of_tracks & 0x00000000000000FFUL) << 56) |   // 8 bits: keep most significant bits for the number of tracks
          ((disc_length & 0x000000000000FFFFUL) << 40);         // 16 bits: disc length
}

/**
 * @brief update the hash with the given track frames
 */
FORCE_HASH_INLINE void hash_update(int32_t frames, uint64_t *hash) {
  *hash = (*hash & 0xFFFFFF0000000000UL) |                      // keep the number of tracks and disc length
         ((*hash & 0x0000000007FFFFFFUL) << 13) |               // rotate bits: shift 13 bits to the left
         ((*hash & 0x000000FFF8000000UL) >> 27);                // rotate bits: shift 27 bits to the right
  *hash = *hash ^ (frames & 0x000000000007FFFFUL);              // xor the hash with the track frames (mask to keep 19 bits)
}


#ifdef __cplusplus
}
#endif

#endif