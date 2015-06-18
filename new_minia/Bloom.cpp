//
//  Bloom.cpp
//
//  Created by Guillaume Rizk on 9/02/12.
//

#include <iostream>
#include <stdio.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <math.h>
#include "Bloom.h"
#include <set>

int Bloom::getHashSize(){
    return hashSize;
}

uint64_t Bloom::getBloomMask(){
    return bloomMask;
}

Bloom::Bloom()
{
    //empty default constructor
    nb_elem = 0;
    blooma = NULL;
}

void Bloom::fakify(std::set<bloom_elem> valid_kmers){
    fake = true;
    valid_set = valid_kmers;
    for(auto it = valid_set.begin(); it != valid_set.end(); it++){
        valid_hash0.insert(get_rolling_hash(*it,0));
        valid_hash1.insert(get_rolling_hash(*it,1));
    }
}

void Bloom::setSeed(uint64_t seed)
{
    if(user_seed==0)
    {
        user_seed = seed;
        this->generate_hash_seed(); //regenerate the hash with the new seed
    }
    else{
        fprintf(stderr,"Warning! you should not change the seed a second time!, resuming with previous seed %llu \n",(unsigned long long)user_seed);
    }
}

void Bloom::set_number_of_hash_func(int i)
{
    if(i>NSEEDSBLOOM || i<1){
        fprintf(stderr,"%i is not a valid value for number of hash funcs, should be in [1-%i], resuming wild old value %i\n",i,NSEEDSBLOOM,n_hash_func );
        return;
    }  
    n_hash_func = i;
}

void Bloom::generate_hash_seed()
{
    unsigned int i;
    for ( i = 0; i < NSEEDSBLOOM; ++i)
    {
        seed_tab[i]= rbase[i];
    }
    for ( i = 0; i < NSEEDSBLOOM; ++i)
    {
        seed_tab[i]= seed_tab[i] * seed_tab[(i+3) % NSEEDSBLOOM] + user_seed ;
    }

    for ( i = 0; i < 4; ++i)
    {
        char_hash[0][i]= rbase[i];
    }
    for ( i = 0; i < 4; ++i)
    {
         char_hash[0][i]=  char_hash[0][i] *  char_hash[0][(i+3) % 4] + user_seed ;
    }
    for ( i = 0; i < 4; ++i)
    {
        char_hash[0][i] &= bloomMask;
       // printf("%lli \n", char_hash[0][i]); //seems random!
    }

    for ( i = 0; i < 4; ++i)
    {
        char_hash[1][i]= rbase[i+4];
    }
    for ( i = 0; i < 4; ++i)
    {
         char_hash[1][i]=  char_hash[1][i] *  char_hash[1][(i+3) % 4] + user_seed ;
    }
    for ( i = 0; i < 4; ++i)
    {
        char_hash[1][i] &= bloomMask;
       // printf("%lli \n", char_hash[1][i]); //seems random
    }
}

#ifdef _largeint
inline uint64_t Bloom::hash_func(LargeInt<KMER_PRECISION> elem, int num_hash)
{
    // hash = XOR_of_series[hash(i-th chunk iof 64 bits)]
    uint64_t result = 0, chunk, mask = ~0;
    LargeInt<KMER_PRECISION> intermediate = elem;
    int i;
    for (i=0;i<KMER_PRECISION;i++)
    {
        chunk = (intermediate & mask).toInt();
        intermediate = intermediate >> 64;
   
        result ^= hash_func(chunk,num_hash);
    }
    return result;
}
#endif

#ifdef _ttmath
inline uint64_t Bloom::hash_func(ttmath::UInt<KMER_PRECISION> elem, int num_hash)
{
    // hash = XOR_of_series[hash(i-th chunk iof 64 bits)]
    uint64_t result = 0, to_hash;
    ttmath::UInt<KMER_PRECISION> intermediate = elem;
    uint32_t mask=~0, chunk;
    int i;
    for (i=0;i<KMER_PRECISION/2;i++)
    {
        // retrieve a 64 bits part to hash 
        (intermediate & mask).ToInt(chunk);
        to_hash = chunk;
        intermediate >>= 32;
        (intermediate & mask).ToInt(chunk);
        to_hash |= ((uint64_t)chunk) << 32 ;
        intermediate >>= 32;

        result ^= hash_func(to_hash,num_hash);
    }
    return result;
}
#endif

#ifdef _LP64
inline uint64_t Bloom::hash_func( __uint128_t elem, int num_hash)
{
    // hash(uint128) = ( hash(upper 64 bits) xor hash(lower 64 bits))
    return hash_func((uint64_t)(elem>>64),num_hash) ^ hash_func((uint64_t)(elem&((((__uint128_t)1)<<64)-1)),num_hash);
}
#endif

inline uint64_t Bloom::hash_func( uint64_t key, int num_hash)
{
    uint64_t hash = seed_tab[num_hash];
    hash ^= (hash <<  7) ^  key * (hash >> 3) ^ (~((hash << 11) + (key ^ (hash >> 5))));
    hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
    hash = hash ^ (hash >> 24);
    hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
    hash = hash ^ (hash >> 14);
    hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
    hash = hash ^ (hash >> 28);
    hash = hash + (hash << 31);
    return hash;
}

//tai is 2^tai_bloom
Bloom::Bloom(int tai_bloom)
{

    printf("tai_bloom: %d \n", tai_bloom);
    n_hash_func = 4 ;//def
    user_seed =0;
    nb_elem = 0;
    //tai = (1LL << tai_bloom);

   // printf("tai: %d \n", tai);
    nchar = tai_bloom/8LL;
    blooma =(unsigned char *)  malloc( nchar *sizeof(unsigned char)); // 1 bit per elem
    memset(blooma,0,nchar *sizeof(unsigned char));
    //fprintf(stderr,"malloc Power-of-two bloom %lli MB nchar %llu %llu\n",(long long)((tai/8LL)/1024LL/1024LL),(unsigned long long)nchar,(unsigned long long)(tai/8));
    this->generate_hash_seed();
}


 Bloom::Bloom(uint64_t tai_bloom, int kVal)
 {
    fake = false;
     //printf("custom construc \n");
    k = kVal;
     n_hash_func = 4 ;//def
     user_seed =0;
     nb_elem = 0;
     hashSize = (int) log2(tai_bloom)+1;
     //printf("Hash size: %d \n", hashSize);
     tai = pow(2, hashSize);
     //printf("Tai: %lli \n", tai);
     if(tai == 0){
        tai = 1;
     }
     bloomMask = tai-1;
     //printf("Mask: %lli \n", bloomMask);
     nchar = (tai/8LL);
     blooma =(unsigned char *)  malloc( nchar *sizeof(unsigned char)); // 1 bit per elem
     //printf("Allocation for filter: %lli bits. \n",nchar *sizeof(unsigned char)*8);
     memset(blooma,0,nchar *sizeof(unsigned char));
     //fprintf(stderr,"malloc bloom %lli MB \n",(tai/8LL)/1024LL/1024LL);
     this->generate_hash_seed();
 }


uint64_t Bloom::getCharHash(int key, int num_hash){
    return char_hash[num_hash][key];
}

uint64_t Bloom::getLastCharHash(uint64_t key, int num_hash){
    return char_hash[num_hash][(int)(key & 3)];
}

//only for num_hash = 0 or 1
uint64_t Bloom::get_rolling_hash(uint64_t key, int num_hash)
{
    uint64_t hash = getLastCharHash(key, num_hash);
    for(int i = 1; i < k; i++){
        hash = rotate_right(hash, 1);
        key >>= 2;
        hash ^= getLastCharHash(key, num_hash);
    }
    return hash;
}


Bloom::~Bloom()
{
  if(blooma!=NULL) 
    free(blooma);
}

void Bloom::dump(char * filename)
{
 FILE *file_data;
 file_data = fopen(filename,"wb");
 fwrite(blooma, sizeof(unsigned char), nchar, file_data); //1+
 printf("bloom dumped \n");

}


void Bloom::load(char * filename)
{
 FILE *file_data;
 file_data = fopen(filename,"rb");
 printf("loading bloom filter from file, nelem %lli \n",nchar);
 int a = fread(blooma, sizeof(unsigned char), nchar, file_data);// go away warning..
 printf("bloom loaded\n");
}

long Bloom::weight()
{
    // return the number of 1's in the Bloom, nibble by nibble
    const unsigned char oneBits[] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
    long weight = 0;
    for(uint64_t index = 0; index < nchar; index++)
    {
        unsigned char current_char = blooma[index];
        weight += oneBits[current_char&0x0f];
        weight += oneBits[current_char>>4];
    }
    return weight;
}

Bloom* Bloom::create_bloom_filter_optimal(uint64_t estimated_items, float fpRate){
     
    Bloom * bloo1;
    int bits_per_item = -log(fpRate)/log(2)/log(2); // needed to process argv[5]

    //printf("Bits per kmer: %d \n", bits_per_item);
    // int estimated_bloom_size = max( (int)ceilf(log2f(nb_reads * NBITS_PER_KMER )), 1);
    uint64_t estimated_bloom_size = (uint64_t) (estimated_items*bits_per_item);
    //printf("Estimated items: %lli \n", estimated_items);
    //printf("Estimated bloom size: %lli.\n", estimated_bloom_size);
    
    //printf("BF memory: %f MB\n", (float)(estimated_bloom_size/8LL /1024LL)/1024);
    bloo1 = new Bloom(estimated_bloom_size, sizeKmer);

    //printf("Number of hash functions: %d \n", (int)floorf(0.7*bits_per_item));
    bloo1->set_number_of_hash_func((int)floorf(0.7*bits_per_item));

    return bloo1;
}