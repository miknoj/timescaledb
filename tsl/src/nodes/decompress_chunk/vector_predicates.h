/*
 * This file and its contents are licensed under the Timescale License.
 * Please see the included NOTICE for copyright information and
 * LICENSE-TIMESCALE for a copy of the license.
 */

/*
 * Functions for working with vectorized predicates.
 */

#pragma once

typedef void(VectorPredicate)(const ArrowArray *, Datum, uint64 *restrict);

VectorPredicate *get_vector_const_predicate(Oid pg_predicate);

void vector_array_operator_or(VectorPredicate *scalar_predicate, const ArrowArray *vector,
							  Datum array, uint64 *restrict result);
void vector_array_operator_and(VectorPredicate *scalar_predicate, const ArrowArray *vector,
							   Datum array, uint64 *restrict result);
