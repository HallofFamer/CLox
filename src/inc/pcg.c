/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

#include "pcg.h"

#include "math.h"
#include <stdint.h>
#include <stdio.h>

static uint64_t pcg_state = 0x4d595df4d0f33173;		// Or something seed-dependent
static uint64_t const pcg_multiplier = 6364136223846793005u;
static uint64_t const pcg_increment = 1442695040888963407u;	// Or an arbitrary odd constant

static uint32_t rotr32(uint32_t x, unsigned r)
{
    return x >> r | x << ((~r + 1) & 31);
}

uint32_t pcg32_random_int(){
    pcg_state = pcg_state * pcg_multiplier + pcg_increment;
    uint64_t x = pcg_state;
    unsigned count = (unsigned)(x >> 59);

    x ^= x >> 18;						
    return rotr32((uint32_t)(x >> 27), count);
}

uint32_t pcg32_random_int_bounded(uint32_t bound) {
    uint32_t threshold = (~bound + 1) % bound;
    for (;;) {
        uint32_t r = pcg32_random_int();
        if (r >= threshold)
            return r % bound;
    }
}

bool pcg32_random_bool() {
    uint32_t r = pcg32_random_int();
    return (bool)(r % 2);
}

double pcg32_random_double() {
    double r = ldexp(pcg32_random_int(), -32);
    return r;
}

void pcg32_seed(uint64_t seed) {
    pcg_state = seed + pcg_increment;
}