#include <stdio.h>
#include "typical-value.h"

void dont_assert(char *str, int fact);
void few_sequential_numbers(FunctionToRepeat f, void *extra_arguments);
void many_values_one_extreme(FunctionToRepeat f, void *extra_arguments);
void many_values_linear(FunctionToRepeat f, void *extra_arguments);
void many_values_big(FunctionToRepeat f, void *extra_arguments);
int call_with(RepeatForAllItemWidths f);

int
main()
{
  printf("=== typical-value test started ===\n");

  dont_assert(
      "with few samples, typical is the maximum width",
      100 == call_with(few_sequential_numbers));

  dont_assert(
      "with many samples, extremes are thrown out",
      100 > call_with(many_values_one_extreme));

  dont_assert(
      "what's returned is a multiple of the step arg",
      8 == call_with(many_values_one_extreme));

  dont_assert(
      "the percentage of rare values must be low",
      92 == call_with(many_values_linear) && 92 == 4 * 23);

  dont_assert(
      "values above maximum are not considered",
      499 == call_with(many_values_big));

  printf("=== typical-value test finished ===\n");

  return 0;
}


int
call_with(RepeatForAllItemWidths f)
{
  TypicalValueArgs args;
  args.value_group_width = 4;
  args.value_group_number = 100;
  args.typicality = 10;
  args.min_total_values_to_care = 50;
  args.for_each_value = f;

  return get_typical_value(args);
}

void
dont_assert(char *str, int fact)
{
  printf("%s... ", str);
  if (fact)
    printf("\033[0;32mPassed\033[0m\n");
  else
    printf("\033[0;31mFailed\033[0m\n");
}


void
few_sequential_numbers(FunctionToRepeat f, void *extra_arguments)
{
  for (int i = 0; i < 48; i++)
    f(5, extra_arguments);
  f(100, extra_arguments);
}

void
many_values_one_extreme(FunctionToRepeat f, void *extra_arguments)
{
  for (int i = 0; i < 49; i++)
    f(5, extra_arguments);
  f(100, extra_arguments);
}

void
many_values_linear(FunctionToRepeat f, void *extra_arguments)
{
  for (int i = 0; i < 100; i++)
    f(i, extra_arguments);
}

void
many_values_big(FunctionToRepeat f, void *extra_arguments)
{
  for (int i = 0; i < 100; i++)
    f(4 * 100 + i, extra_arguments);
}

