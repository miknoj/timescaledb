/*
 * This file and its contents are licensed under the Timescale License.
 * Please see the included NOTICE for copyright information and
 * LICENSE-TIMESCALE for a copy of the license.
 */

#include <postgres.h>
#include <catalog/pg_operator.h>
#include <miscadmin.h>
#include <nodes/bitmapset.h>
#include <nodes/makefuncs.h>
#include <nodes/nodeFuncs.h>
#include <optimizer/cost.h>
#include <optimizer/optimizer.h>
#include <optimizer/pathnode.h>
#include <optimizer/paths.h>
#include <parser/parsetree.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>
#include <utils/typcache.h>
#include <utils/fmgroids.h>

#include <planner.h>

#include "nodes/decompress_chunk/decompress_chunk.h"
#include "partialize_agg.h"
#include "utils.h"
#include "debug_assert.h"

#define is_restricted_path(path) list_length(path->parent->baserestrictinfo)

/*
 * Are we able to optimize the path by applying vectorized aggregation?
 */
static bool
is_vectorizable_agg_path(PlannerInfo *root, AggPath *agg_path, Path *path)
{
	Assert(agg_path->aggstrategy == AGG_SORTED || agg_path->aggstrategy == AGG_PLAIN ||
		   agg_path->aggstrategy == AGG_HASHED);

	/* Having is not supported at the moment */
	if (root->hasHavingQual)
		return false;

	/* Only vectorizing within the decompress node is supported so far */
	bool is_decompress_chunk = ts_is_decompress_chunk_path(path);
	if (!is_decompress_chunk)
		return false;

	DecompressChunkPath *decompress_path = (DecompressChunkPath *) path;
	Assert(decompress_path->custom_path.custom_paths != NIL);

	/*
	 * It would be great if we could check decompress_path -> have_bulk_decompression_columns
	 * here (or ensure the column can be bulk decompressed). However, this information is only
	 * computed when a path is selected and the plan is created.
	 *
	 * However, due to the restriction to a particular data type for vectorized aggregates
	 * (i.e., INT4) we can assume that this column type can be bulk decompressed.
	 */
	Path *compressed_path = linitial(decompress_path->custom_path.custom_paths);

	/* No filters are supported at the moment */
	if (is_restricted_path(path) || is_restricted_path(compressed_path))
		return false;

	/* We currently handle only one agg function per node */
	if (list_length(agg_path->path.pathtarget->exprs) != 1)
		return false;

	/* Only sum on int 4 is supported at the moment */
	Node *expr_node = linitial(agg_path->path.pathtarget->exprs);
	if (!IsA(expr_node, Aggref))
		return false;

	Aggref *aggref = castNode(Aggref, expr_node);

	if (aggref->aggfnoid != F_SUM_INT4)
		return false;

	return true;
}

/*
 * Check if we can perform the computation of the aggregate in a vectorized manner directly inside
 * of the decompress chunk node. If this is possible, the decompress chunk node will emit partial
 * aggregates directly, and there is no need for the PostgreSQL aggregation node on top.
 */
bool
apply_vectorized_agg_optimization(PlannerInfo *root, AggPath *aggregation_path, Path *path)
{
	if (!ts_guc_enable_vectorized_aggregation)
		return false;

	Assert(path != NULL);
	Assert(aggregation_path->aggsplit == AGGSPLIT_INITIAL_SERIAL);

	if (is_vectorizable_agg_path(root, aggregation_path, path))
	{
		Assert(ts_is_decompress_chunk_path(path));
		DecompressChunkPath *decompress_path = (DecompressChunkPath *) castNode(CustomPath, path);

		/* Change the output of the path and let the decompress chunk node emit partial aggregates
		 * directly */
		decompress_path->perform_vectorized_aggregation = true;
		decompress_path->custom_path.path.pathtarget = aggregation_path->path.pathtarget;

		/* The decompress chunk node can perform the aggregation directly. No need for a dedicated
		 * agg node on top. */
		return true;
	}

	/* PostgreSQL should handle the aggregation. Regular agg node on top is required. */
	return false;
}
