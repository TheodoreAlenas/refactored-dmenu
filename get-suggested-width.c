#include <stdlib.h>
#include "get-suggested-width.h"
#include "util.h"

typedef struct {
  int max_length_found;
  int *lengths_above;
  size_t max_lengths_above;
  int length_step_per_index;
  int typicality;
} SuggestedWidthStats;

void
init_array_to(int *array, size_t size, int value)
{
  for (size_t i = 0; i < size; i++)
    array[i] = value;
}

void
update_stats(int length, void *void_sws)
{
  SuggestedWidthStats *sws = (SuggestedWidthStats *) void_sws;

  if (length > sws->max_length_found)
    sws->max_length_found = length;

  for (size_t i = 0; i < sws->max_lengths_above; i++)
    if (length > sws->length_step_per_index * i)
      sws->lengths_above[i] += 1;
}

void
print(const SuggestedWidthStats *sws)
{
#include <stdio.h>
  for (int i = 0; i < sws->max_lengths_above; i++) {
    for (int j = 0; j < sws->lengths_above[i]; j += 100)
      printf("-");
    printf("\n");
  }
  printf("\n");
}

int
conclude(const SuggestedWidthStats *prepared_stats)
{
  const SuggestedWidthStats *sws = prepared_stats;  // alias

  for (size_t i = 0; i < sws->max_lengths_above - 1; i++) {
    int next_is_rare = (sws->lengths_above[i + 1] * sws->typicality < sws->lengths_above[0]);
    int plenty_of_this_one = (sws->lengths_above[i] > 50);
    if (next_is_rare && plenty_of_this_one)
      return (i + 1) * sws->length_step_per_index;
  }

  return sws->max_length_found;
}

int
backend(SuggestedWidthStats *stats, RepeatForAllItemWidths repeat_for_all_item_widths)
{
  repeat_for_all_item_widths(update_stats, (void *) stats);
  return conclude(stats);
}

// prepares the stack
int
get_typical_width(int typicality, RepeatForAllItemWidths repeat_for_all_item_widths)
{

#define MAX_LENGTHS_ABOVE 32
#define LENGTH_STEP_PER_INDEX 16

  SuggestedWidthStats stats;
  int lengths_above[MAX_LENGTHS_ABOVE];
  init_array_to(lengths_above, MAX_LENGTHS_ABOVE, 0);

  stats.max_length_found = 0;
  stats.length_step_per_index = LENGTH_STEP_PER_INDEX;
  stats.lengths_above = lengths_above;
  stats.max_lengths_above = MAX_LENGTHS_ABOVE;
  stats.typicality = typicality;

  return backend(&stats, repeat_for_all_item_widths);
}

