#include <stdlib.h>
#include "typical-value.h"
#include "util.h"

typedef struct {
  int max_length_found;
  int *lengths_above;
  size_t max_lengths_above;
  int length_step_per_index;
  int typicality;
  int min_total_values_to_care;
} SuggestedWidthStats;

void
init_array_to(int *array, size_t size, int value)
{
  for (size_t i = 0; i < size; i++)
    array[i] = value;
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
item_is_rare(const SuggestedWidthStats *sws, int index)
{
  int total_values = sws->lengths_above[0];
  int buffed_version = sws->lengths_above[index] * sws->typicality;
  int still_cant_compete = (buffed_version < total_values);
  return still_cant_compete;
}

int
conclude(const SuggestedWidthStats *prepared_stats)
{
  const SuggestedWidthStats *sws = prepared_stats;  // alias

  const int total_values = sws->lengths_above[0];
  if (total_values < sws->min_total_values_to_care)
    return sws->max_length_found;

  for (size_t i = 0; i < sws->max_lengths_above - 1; i++) {
    if (item_is_rare(sws, i + 1))
      return (i + 1) * sws->length_step_per_index;
  }

  return sws->max_length_found;
}

void
consider_another_value(int length, void *void_sws)
{
  SuggestedWidthStats *sws = (SuggestedWidthStats *) void_sws;

  if (length > sws->max_length_found)
    sws->max_length_found = length;

  for (size_t i = 0; i < sws->max_lengths_above; i++)
    if (length > sws->length_step_per_index * i)
      sws->lengths_above[i] += 1;
}

int
get_typical_value(TypicalValueArgs args)
{
  SuggestedWidthStats stats;

  stats.max_lengths_above = args.value_group_number;
  stats.lengths_above = malloc(args.value_group_number * sizeof(int));
  if (!stats.lengths_above)
    return -1;

  init_array_to(stats.lengths_above, args.value_group_number, 0);

  stats.max_length_found = 0;
  stats.length_step_per_index = args.value_group_width;
  stats.typicality = args.typicality;
  stats.min_total_values_to_care = args.min_total_values_to_care;

  args.for_each_value(consider_another_value, (void *) &stats);
  return conclude(&stats);
}
