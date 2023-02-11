#include "stdlib.h"

typedef void (*FunctionToRepeat) (int text_width, void *extra_argumens);
typedef void (*RepeatForAllItemWidths) (FunctionToRepeat, void *extra_arguments);

typedef struct {
  size_t value_group_number;
  int value_group_width;
  int typicality;
  int min_total_values_to_care;
  RepeatForAllItemWidths for_each_value;
} TypicalValueArgs;

int get_typical_value(TypicalValueArgs args);

