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
 *     http://www.pcg-random.org
 */

#ifndef PCG_H_INCLUDED
#define PCG_H_INCLUDED 1

#include <inttypes.h>
#include <stdbool.h>

uint32_t pcg32_random_int();
uint32_t pcg32_random_int_bounded(uint32_t bound);
bool pcg32_random_bool();
double pcg32_random_double();
void pcg32_seed(uint64_t seed);

#endif // PCG_H_INCLUDED
