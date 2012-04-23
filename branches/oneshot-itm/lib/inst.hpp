/**
 *  Copyright (C) 2011
 *  University of Rochester Department of Computer Science
 *    and
 *  Lehigh University Department of Computer Science and Engineering
 *
 * License: Modified BSD
 *          Please see the file LICENSE.RSTM for licensing information
 */
#ifndef RSTM_INST_H
#define RSTM_INST_H

#include <stdint.h>
#include "tx.hpp"
#include "inst-alignment.hpp"
#include "inst-buffer.hpp"
#include "inst-offsetof.hpp"
#include "inst-baseof.hpp"
#include "inst-stackfilter.hpp"         // not used directly here, but all
#include "inst-raw.hpp"                 // specializers will need them

/**
 *  The intrinsic read and write barriers are implemented by the TM.
 */
void* alg_tm_read_aligned_word(void** addr, stm::TX*, uintptr_t mask);
static void* alg_tm_read_aligned_word_ro(void** addr, stm::TX*, uintptr_t mask);

namespace stm {
  namespace inst {
    /**
     *  This template should be used at the READ_ONLY template parameter for
     *  the read<> function if there is no separate
     *  alg_tm_read_aligned_word_ro instrumentation.
     */
    struct NoReadOnly {
        static inline bool IsReadOnly(TX*) {
            return false;
        }
    };
    /**
     *  Whenever we need to perform a transactional load or store we need a
     *  mask that has 0xFF in all of the bytes that we are intersted in. This
     *  computes a mask given an [i, j) range, where 0 <= i < j <=
     *  sizeof(void*).
     *
     *  NB: When the parameters are compile-time constants we expect this to
     *    become a simple constant in the binary when compiled with
     *    optimizations.
     */
    static inline uintptr_t make_mask(size_t i, size_t j) {
        // assert(0 <= i && i < j && j <= sizeof(void*) && "range is incorrect")
        uintptr_t mask = ~(uintptr_t)0;
        mask = mask >> (8 * (sizeof(void*) - j + i)); // shift 0s to the top
        mask = mask << (8 * i);                       // shift 0s into the bottom
        return mask;
    }

    static inline size_t min(size_t lhs, size_t rhs) {
        return (lhs < rhs) ? lhs : rhs;
    };

    struct ReadAlignedWord {
        static inline void* Read(void** addr, TX* tx, uintptr_t mask) {
            return alg_tm_read_aligned_word(addr, tx, mask);
        }
    };

    struct ReadAlignedWordRO {
        static inline void* Read(void** addr, TX* tx, uintptr_t mask) {
            return alg_tm_read_aligned_word_ro(addr, tx, mask);
        }
    };

    /** The intrinsic read loop. */
    template <typename T, typename RAW, typename READ, size_t N>
    static inline void read_words(TX* tx, void* words[N], void** const base,
                                  const size_t off) {
        // Some read-after-write algorithms need local storage.
        RAW raw;

        // deal with the first word, there's always at least one
        uintptr_t mask = make_mask(off, min(sizeof(void*), off + sizeof(T)));
        if (!raw.hit(base, words[0], tx, mask))
            raw.merge(READ::Read(base, tx, mask), words[0]);

        // deal with any middle words
        mask = make_mask(0, sizeof(void*));
        for (int i = 1, e = N - 1; i < e; ++i) {
            if (raw.hit(base + i, words[i], tx, mask))
                continue;
            raw.merge(READ::Read(base + i, tx, mask), words[i]);
        }

        // deal with the last word, the second check is just offset, because we
        // already checked to see if an access overflowed in dealing with the
        // first word.
        if (N > 1 && off) {
            mask = make_mask(0, off);
            if (!raw.hit(base + N, words[N - 1], tx, mask))
                raw.merge(READ::Read(base + N, tx, mask), words[N]);
        }
    }

    /**
     *  This is a generic read instrumentation routine, that uses the client's
     *  stack filter and read-after-write mechanism. The routine is completely
     *  generic, but we've used as much compile-time logic as we can. We expect
     *  that any modern compiler will be able to eliminate branches where
     *  they're not needed, and spot testing with gcc verifies this.
     */
    template <typename T,              // the type we're loading
              class PREFILTER,         // code to run before anything else
              class RAW,               // the read-after-write algorithm
              class READ_ONLY,         // branch for read-only access
              bool FORCE_ALIGNED>      // force the code to treat Ts as aligned
    static inline T read(T* addr) {
        TX* tx = Self;

        // see if this read should be done in-place
        if (PREFILTER::filter((void**)addr, tx))
            return *addr;

        // sometimes we want to force the instrumentation to be aligned, even
        // if a T isn't guaranteed to be aligned on the architecture, for
        // instance, the library API does this
        const bool ALIGNED = Aligned<T, FORCE_ALIGNED>::value;

        // adjust the base pointer for possibly non-word aligned accesses
        void** base = Base<T, ALIGNED>::BaseOf(addr);

        // compute an offset for this address
        size_t off = Offset<T, ALIGNED>::OffsetOf(addr);

        // the bytes union is used to deal with unaligned and/or subword data.
        enum { N = Buffer<T, ALIGNED>::Words };
        union {
            void* words[N];
            uint8_t bytes[sizeof(void*[N])];
        };

        // branch eliminated for NoReadOnly policy (note readonly is
        // instantiated with NoRAW policy
        if (READ_ONLY::IsReadOnly(tx))
            read_words<T, NoRAW, ReadAlignedWordRO, N>(tx, words, base, off);
        else
            read_words<T, RAW, ReadAlignedWord, N>(tx, words, base, off);

        return *reinterpret_cast<T*>(bytes + off);
    }

    template <typename T,
              class PREFILTER,
              class WRITE,
              bool FORCE_ALIGNED>
    static inline void write(T* addr, T val) {
        TX* tx = Self;

        // see if this write should be done in-place
        if (PREFILTER::filter((void**)addr, tx)) {
            *addr = val;
            return;
        }

        // sometimes we want to force the instrumentation to be aligned, even
        // if a T isn't guaranteed to be aligned on the architecture, for
        // instance, the library API does this
        const bool ALIGNED = Aligned<T, FORCE_ALIGNED>::value;

        // adjust the base pointer for possibly non-word aligned accesses
        void** base = Base<T, ALIGNED>::BaseOf(addr);

        // compute an offset for this address
        size_t off = Offset<T, ALIGNED>::OffsetOf(addr);

        // the bytes union is used to deal with unaligned and/or subword data.
        enum { N = Buffer<T, ALIGNED>::Words };
        union {
            void* words[N];
            uint8_t bytes[sizeof(void*[N])];
        };

        // put the value into the right place on the stack
        *reinterpret_cast<T*>(bytes + off) = val;

        // template function object for writing
        WRITE write;

        // store the first word, there is always at least one
        uintptr_t mask = make_mask(off, min(sizeof(void*), off + sizeof(T)));
        write(base, words[0], tx, mask);

        // deal with any middle words
        mask = make_mask(0, sizeof(void*));
        for (int i = 1, e = N - 1; i < e; ++i)
            write(base + i, words[i], tx, mask);

        // deal with the last word
        if (N > 1 && off) {
            mask = make_mask(0, off);
            write(base + N, words[N - 1], tx, mask);
        }
    }

    /**
     *  Our lazy STMs all basically do the same thing for writing, they simply
     *  buffer the write in the write set. This functor can be used when
     *  instantiating the write template for these TMs.
     */
    struct BufferedWrite {
        void operator()(void** addr, void* val, TX* tx, uintptr_t mask) const {
            // just buffer the write
            tx->writes.insert(addr, val, mask);
        }
    };
  }
}

#endif // RSTM_INST_H
