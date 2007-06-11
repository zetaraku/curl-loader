
#include "batch.h"

int is_batch_group_leader (batch_context* bctx)
{
  return !bctx->batch_id;
}
